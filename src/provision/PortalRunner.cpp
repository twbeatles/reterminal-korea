#include "PortalRunner.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include <memory>
#include <new>

#include "../boot/SerialLog.h"
#include "../drivers/Battery.h"
#include "../drivers/Pmic.h"
#include "../drivers/Watchdog.h"
#include "../net/WifiConnector.h"
#include "../power/RtcPersistence.h"
#include "../ui/FramePresenter.h"
#include "../ui/ViewModel.h"
#include "LanPolicy.h"
#include "LanPortal.h"
#include "PortalHandler.h"
#include "PortalServer.h"

#ifndef KDASH_FW_VERSION
#define KDASH_FW_VERSION "0.0.0"
#endif

namespace kdash {
namespace portal {
namespace {

LanPortal g_lan;
uint32_t g_next_open_ms = 0;  // Always 모드 재시도 간격(연결 직후 실패 대비)

// reset=true(공장초기화) 또는 데이터 지문이 바뀐 저장이면 런타임 캐시를 지운다.
// 기기 설정만 바뀐 저장은 캐시를 유지하고 인증 차단만 다음 부팅에 푼다(ADR 0026, ISSUE-008).
[[noreturn]] void RestartAfterConfig(IStorage& storage, bool reset) {
  if (reset || LastSaveChangedData()) {
    power::InvalidateForPortal(storage);
  } else {
    power::RequestAuthClearOnBoot();
    KDASH_LOG.println("config: device-only change; runtime cache kept");
  }
  g_lan.Close();
  WiFi.mode(WIFI_OFF);
  KDASH_LOG.println("config: saved/reset; rebooting");
  KDASH_LOG.flush();
  ESP.restart();
  for (;;) delay(1000);
}

// 메뉴/상태 화면에 보일 글. 비밀값 없음. 첫 줄(버전·신호·전원)은 메뉴에, 전체는 /status에 나온다.
// 공급자 줄은 기기 탭과 같은 문구(vm::BuildDevice)를 쓴다. 5초마다만 다시 만든다.
void StatusLine(char* out, size_t cap, bool usb, const app::AppState* st = nullptr) {
  static char cache[1024];
  static uint32_t built_ms = 0;
  static bool have = false;
  if (!st) {
    snprintf(out, cap, "FW %s · Wi-Fi %d dBm · %s", KDASH_FW_VERSION, (int)WiFi.RSSI(),
             usb ? "USB 전원" : "배터리");
    return;
  }
  if (!have || (uint32_t)(millis() - built_ms) >= 5000) {
    const int64_t now = (int64_t)time(nullptr);
    std::unique_ptr<vm::DevicePage> dev(new (std::nothrow) vm::DevicePage(
        vm::BuildDevice(*st, now, st->time_verified, WiFi.status() == WL_CONNECTED, -1,
                        KDASH_FW_VERSION, usb)));
    size_t used = (size_t)snprintf(cache, sizeof(cache), "FW %s · Wi-Fi %d dBm · %s\n%s",
                                   KDASH_FW_VERSION, (int)WiFi.RSSI(), usb ? "USB 전원" : "배터리",
                                   st->time_verified ? "시계 정상(NTP/RTC)" : "시계 확인 필요");
    if (dev) {
      for (int i = 0; i < (int)sched::Provider::Count && used < sizeof(cache); i++)
        used += (size_t)snprintf(cache + used, sizeof(cache) - used, "\n%s", dev->providers[i]);
    }
    built_ms = millis();
    have = true;
  }
  snprintf(out, cap, "%s", cache);
}

void ClearStatus(app::PortalStatus& p) {
  const ui::Page keep = p.return_page;
  p = app::PortalStatus();
  p.return_page = keep;
}

void SyncStatus(app::AppState& st) {
  LanInfo i;
  if (!g_lan.Info(i)) { ClearStatus(st.portal); return; }
  app::PortalStatus& p = st.portal;
  p.open = true;
  p.locked = i.locked;
  p.remaining_min = i.remaining_min;
  snprintf(p.ip, sizeof(p.ip), "%s", i.ip);
  snprintf(p.host, sizeof(p.host), "%s", i.host);
  snprintf(p.code, sizeof(p.code), "%s", i.code);
}

void CloseLan(app::AppState& st) {
  const bool was_open = st.portal.open;
  g_lan.Close();
  ClearStatus(st.portal);
  if (was_open && st.page == ui::Page::Device && st.settings.device.lan_mode != LanPortalMode::Always) {
    st.page = st.portal.return_page;  // 포털을 열기 전 화면으로
    st.redraw_requested = true;
  }
  st.last_page_change_ms = millis();
}

// SoftAP 포털(기존 동작). Saved/Reset이면 재부팅.
bool RunAp(app::AppState& state, IStorage& storage) {
  const PortalResult result = RunPortal(state.settings, storage, ui::ShowPortal);
  if (result == PortalResult::Saved || result == PortalResult::Reset)
    RestartAfterConfig(storage, result == PortalResult::Reset);
  if (result == PortalResult::DisplayUnavailable)
    KDASH_LOG.println("portal: no readable local password; AP not opened");
  return false;
}

LanTick HandleEvent(LanEvent ev, app::AppState& state, IStorage& storage) {
  switch (ev) {
    case LanEvent::Saved:
    case LanEvent::Reset:
      RestartAfterConfig(storage, ev == LanEvent::Reset);
    case LanEvent::Closed:
      KDASH_LOG.println("portal: lan closed");
      CloseLan(state);
      return state.redraw_requested ? LanTick::Redraw : LanTick::None;
    case LanEvent::Reboot:
      KDASH_LOG.println("portal: reboot requested from lan session");
      KDASH_LOG.flush();
      g_lan.Close();
      ESP.restart();
      break;
    case LanEvent::OpenAp:
      KDASH_LOG.println("portal: switching to ap portal on request");
      CloseLan(state);
      RunAp(state, storage);
      state.redraw_requested = true;
      return LanTick::Redraw;
    case LanEvent::SaveFailed:
      KDASH_LOG.println("portal: save failed; previous settings kept");
      break;
    case LanEvent::Refresh:
      return LanTick::Refresh;
    default:
      break;
  }
  return LanTick::None;
}

// 배터리 전원: 패널에 주소/코드를 그리고 최대 10분 블로킹. 닫히면 호출자가 잠든다.
bool RunLanBlocking(app::AppState& state, IStorage& storage) {
  LanInfo i;
  if (!g_lan.Info(i)) return false;
  auto show = [&]() {
    char url[40], host[48];
    snprintf(url, sizeof(url), "http://%s", i.ip);
    snprintf(host, sizeof(host), i.host[0] ? "또는 %s.local" : "", i.host);
    return ui::ShowLanPortal(url, host, i.locked ? "잠김 (5분 뒤 새 코드)" : i.code);
  };
  if (!show()) {
    KDASH_LOG.println("portal: panel unavailable; lan portal not opened");
    CloseLan(state);
    return false;
  }
  g_lan.TakeDisplayChanged();  // 첫 표시는 방금 했다
  static char status[1024];
  const uint32_t started = millis();
  uint32_t last_battery_check = 0;
  int battery_mv = -1;
  while (g_lan.IsOpen()) {
    wdt::Feed();
    // 배터리: 절대 상한(30분)과 저전압이면 닫고 잠든다. 창 만료는 Poll이 따로 본다(ISSUE-007).
    if ((uint32_t)(millis() - last_battery_check) >= 10000) {
      last_battery_check = millis();
      battery_mv = ReadBatteryMv();
    }
    if (LanBlockingShouldExit((uint32_t)(millis() - started), battery_mv)) {
      KDASH_LOG.printf("portal: lan blocking exit (elapsed_s=%lu battery_mv=%d)\n",
                       (unsigned long)((millis() - started) / 1000), battery_mv);
      break;
    }
    StatusLine(status, sizeof(status), false, &state);
    const LanEvent ev = g_lan.Poll(status);
    if (ev != LanEvent::None) {
      HandleEvent(ev, state, storage);  // Refresh는 배터리 블로킹에서는 무시(닫힌 뒤 사이클이 갱신)
      if (!g_lan.IsOpen()) break;
    }
    if (g_lan.TakeDisplayChanged() && g_lan.Info(i)) show();  // 코드 교체/잠금
    delay(5);
  }
  CloseLan(state);
  return false;
}

}  // namespace

bool OpenPortal(app::AppState& state, IStorage& storage) {
  const bool usb = pmic::UsbAttached();
  const LanPortalMode mode = state.settings.device.lan_mode;
  if (g_lan.IsOpen()) {
    if (mode == LanPortalMode::Always) {  // 상시 모드: 길게 누르면 기기 탭에서 주소/코드 보기
      if (state.page != ui::Page::Device) state.portal.return_page = state.page;
      state.page = ui::Page::Device;
      SyncStatus(state);
    } else {
      CloseLan(state);  // 토글: 다시 길게 누르면 닫는다
    }
    return false;
  }
  if (mode != LanPortalMode::Off && state.settings.wifi_ssid[0]) {
    const bool wifi = WiFi.status() == WL_CONNECTED || net::ConnectWifi(state.settings);
    if (wifi && g_lan.Open(state.settings, storage, kWindowSec)) {
      KDASH_LOG.println("portal: lan open (10 min)");
      state.portal.return_page = state.page == ui::Page::Device ? ui::Page::Life : state.page;
      SyncStatus(state);
      if (usb) {  // 논블로킹: 기기 탭이 주소와 코드를 보여 주고 loop()가 요청을 처리한다
        state.page = ui::Page::Device;
        state.redraw_requested = true;
        g_lan.TakeDisplayChanged();
        return false;
      }
      return RunLanBlocking(state, storage);
    }
    KDASH_LOG.println("portal: lan unavailable -> ap");
  }
  return RunAp(state, storage);
}

LanTick TickLanPortal(app::AppState& state, IStorage& storage, bool usb) {
  const LanPortalMode mode = state.settings.device.lan_mode;
  if (!g_lan.IsOpen()) {
    if (!usb || mode != LanPortalMode::Always || !state.settings.wifi_ssid[0] ||
        WiFi.status() != WL_CONNECTED || (int32_t)(millis() - g_next_open_ms) < 0)
      return LanTick::None;
    if (!g_lan.Open(state.settings, storage, 0)) {
      g_next_open_ms = millis() + 30000;
      return LanTick::None;
    }
    KDASH_LOG.println("portal: lan open (always)");
  } else if (!usb && mode == LanPortalMode::Always) {
    CloseLan(state);  // 배터리로 바뀌면 상시 포털은 닫고 즉시 사이클을 돌려 잠든다(G-7)
    return LanTick::Redraw;
  }
  static char status[1024];
  StatusLine(status, sizeof(status), usb, &state);
  LanTick tick = HandleEvent(g_lan.Poll(status), state, storage);
  if (!g_lan.IsOpen()) return tick;
  SyncStatus(state);
  if (g_lan.TakeDisplayChanged() && tick == LanTick::None && state.page == ui::Page::Device)
    tick = LanTick::Redraw;
  return tick;
}

}  // namespace portal
}  // namespace kdash
#else
// Host/CI guard: AP/LAN portal needs the ESP32 toolchain.
#endif
