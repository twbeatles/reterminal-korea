// K-Dash E1001 - boot entry (Composition Root). Builds under `pio run -e reterminal_e1001` only.
// Boot contract (01): UART boot line carries fw version, reset reason,
// heap/PSRAM, clock state - and NEVER secrets (SEC-01).
//
// SOLID composition root: this file owns object wiring plus the
// setup()/loop() lifecycle only; business logic lives in focused modules.
//  - settings load .... boot/SettingsLoader (NVS -> dev -> defaults)
//  - Wi-Fi ............. net/WifiConnector
//  - time .............. drivers/EspClockSync (IClockPlatform impl)
//  - diagnostics ....... util/SnapshotLog
//  - display ........... ui/FramePresenter (ViewModel -> Canvas -> Panel)
//  - RTC/NVS retain .... power/RtcPersistence (IStorage abstraction)
//  - portal ............ provision/PortalRunner
//  - calendar refresh .. app/CalendarRefresh (IHttpClient abstraction)
//  - sleep/buttons ..... power/SleepController
//  - cycle ............. app/CycleRunner (high-level orchestration)
// Hardware behaviour remains unverified until tested on a physical E1001.
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <time.h>

#include <driver/rtc_io.h>

#include <memory>
#include <new>

#include "app/Coordinator.h"
#include "app/CycleRunner.h"
#include "app/PageRotation.h"
#include "boot/SerialLog.h"
#include "boot/SettingsLoader.h"
#include "drivers/Battery.h"
#include "drivers/ButtonDecoder.h"
#include "drivers/Pcf8563_esp32.h"
#include "drivers/Pmic.h"
#include "drivers/Watchdog.h"
#include "domain/Freshness.h"
#include "net/HttpTransport.h"
#include "net/WifiConnector.h"
#include "power/CyclePlanner.h"
#include "power/RtcPersistence.h"
#include "power/SleepController.h"
#include "provision/PortalRunner.h"
#include "storage/NvsStorage.h"
#include "ui/Pages.h"
#include "util/BuildIdentity.h"

#ifndef KDASH_FW_VERSION
#define KDASH_FW_VERSION "0.0.0"
#endif

namespace {

using namespace kdash;

// State owned by the composition root. Module-internal state (clock/NTP,
// RTC blob, panel, calendar backoff, cycle budget) is encapsulated per
// module; this file only wires them together.
NvsStorage g_nvs;
app::AppState g_state;
app::Snapshot g_snap;
app::KisSession g_kis;
net::EspHttpClient* g_http = nullptr;
int g_jitter_slot = 0;
uint32_t g_last_pass_ms = 0;
constexpr uint32_t kPassEveryMs = 30 * 1000;  // due check cadence (USB idle)
ButtonDecoder g_buttons;
bool g_suppress_buttons = false;
uint8_t g_last_button_mask = 0;
uint64_t g_last_health_ms = 0;
#ifdef KDASH_DEV_STABILITY_SELFTEST
RTC_NOINIT_ATTR uint32_t g_diag_magic;
RTC_NOINIT_ATTR uint32_t g_diag_phase;
void StabilitySelftest() {
  if (g_diag_magic != 0x4C543033u) { g_diag_magic=0x4C543033u; g_diag_phase=0; }
  if (g_diag_phase == 0 && millis() >= 45000) {
    g_diag_phase=1;
    KDASH_LOG.println("diag: intentional wifi disconnect once");
    WiFi.disconnect(false,false);
  }
  if (g_diag_phase == 1 && millis() >= 100000) {
    g_diag_phase=2;  // retained BEFORE stall: never repeat on watchdog reboot
    power::SetCyclePending(true);
    KDASH_LOG.println("diag: intentional loop stall; expect task watchdog reset");
    KDASH_LOG.flush();
    while (true) delay(1000);  // no Feed; compiled ONLY in validation env
  }
}
#endif

// Internal RAM and PSRAM are separate: a large PSRAM pool must not hide
// exhaustion/fragmentation of the RAM needed by Wi-Fi and TLS.
void LogHealth(bool usb) {
  const uint64_t now = (uint64_t)esp_timer_get_time() / 1000;
  if (g_last_health_ms && now - g_last_health_ms < 60000) return;
  g_last_health_ms = now;
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  KDASH_LOG.printf("health: uptime_ms=%llu heap=%u min_heap=%u largest=%u psram=%u usb=%d wifi=%d\n",
                   (unsigned long long)now,
                   (unsigned)heap_caps_get_free_size(caps),
                   (unsigned)heap_caps_get_minimum_free_size(caps),
                   (unsigned)heap_caps_get_largest_free_block(caps),
                   (unsigned)ESP.getFreePsram(), (int)usb,
                   (int)(WiFi.status() == WL_CONNECTED));
}

}  // namespace

void setup() {
  KDASH_LOG.begin(115200, SERIAL_8N1, kLogRxPin, kLogTxPin);
  delay(200);
  KDASH_LOG.printf("kdash boot fw=%s board=e1001 reset=%d heap=%u psram=%u (no secrets logged)\n",
                  KDASH_FW_VERSION, (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
  KDASH_LOG.println(BuildIdentity());
  for (int i = 0; i < 3; ++i) { rtc_gpio_deinit((gpio_num_t)(3 + i)); pinMode(3 + i, INPUT); }
  power::BootTrackReset();
  KDASH_LOG.printf("reset_guard: boots=%u crashes=%u pending=%d\n",
      (unsigned)power::Boots(),(unsigned)power::Crashes(),(int)power::CyclePending());
  if (!wdt::Begin()) KDASH_LOG.println("watchdog: subscription FAILED");
  g_jitter_slot = (int)(esp_random() % 1000);
  pcf::PcfInit();
  BatteryBegin();
  pmic::Diagnose();
  g_nvs.begin();
  boot::LoadSettings(g_nvs, g_state.settings);
#ifdef KDASH_DEV_CHARGE_PROTECT
  // Local device opt-in only when no stored settings exist; portal/NVS wins.
  if (strcmp(boot::ConfigSource(), "dev") == 0) g_state.settings.device.charge_protect = true;
#endif
  g_state.charge_state = pmic::MaintainCharge(g_state.settings.device.charge_protect);
  g_state.page = (ui::Page)g_state.settings.display_page;
  power::RestoreRuntime(g_nvs, g_state, g_snap, g_kis);
  KDASH_LOG.printf("config: source=%s\n", boot::ConfigSource());
  KDASH_LOG.printf("restore: kis_quotes=%d kospi=%d fx=%d\n",
                   (int)(g_snap.has_kis[0] || g_snap.has_kis[1] || g_snap.has_kis[2] || g_snap.has_kis[3]),
                   (int)g_snap.has_kospi, (int)g_snap.has_fx);
  boot::LogConfigRedacted(g_state.settings);
  g_http = new (std::nothrow) net::EspHttpClient();
  // USB-powered: bring Wi-Fi up first so the first frame and the device tab show the real link
  // state even when no provider is due (restored refresh history would otherwise skip it).
  if (pmic::UsbAttached()) net::ConnectWifi(g_state.settings);
  ButtonEvent event = ButtonEvent::None;
  power::CycleWake reason = power::CycleWake::FirstBoot;
  const esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
  if (wake == ESP_SLEEP_WAKEUP_TIMER) reason = power::CycleWake::Timer;
  if (wake == ESP_SLEEP_WAKEUP_EXT1) {
    reason = power::CycleWake::Button;
    const uint8_t pins = (uint8_t)esp_sleep_get_ext1_wakeup_status();
    const uint32_t start = millis();
    while ((power::PressedButtons() & 1) && (uint32_t)(millis() - start) < 2000) delay(10);
    event = WakeButtons(pins, power::PressedButtons(), (uint32_t)(millis() - start) >= 2000);
    g_suppress_buttons = power::PressedButtons() != 0;
  }
  app::RunCycle(reason, event, g_state, g_snap, g_kis, g_http, g_nvs,
                g_jitter_slot, KDASH_FW_VERSION);
  g_last_pass_ms = millis();
  g_state.last_page_change_ms = millis();  // 자동 순환 타이머 시작
#ifdef KDASH_DEV_SCAN_SELFTEST
  // 실기 검증 전용(-D로만 켬): 연결된 상태에서 주변 Wi-Fi 검색이 되는지 개수만 기록한다(SSID 미기록).
  if (WiFi.status() == WL_CONNECTED) {
    const int found = WiFi.scanNetworks(false, false);
    KDASH_LOG.printf("diag: wifi scan found=%d connected=%d\n", found, (int)(WiFi.status() == WL_CONNECTED));
    WiFi.scanDelete();
  }
#endif
}

void loop() {
  wdt::Feed();
#ifdef KDASH_DEV_STABILITY_SELFTEST
  StabilitySelftest();
#endif
  const bool usb = pmic::UsbAttached();
  const charge::State charging = pmic::MaintainCharge(g_state.settings.device.charge_protect);
  if (charging != g_state.charge_state) {
    g_state.charge_state = charging;
    if (g_state.page == ui::Page::Device) g_state.redraw_requested = true;
  }
  LogHealth(usb);
  net::MaintainWifi(g_state.settings, usb);
  if (usb && net::TakeWifiRedrawRequest()) g_state.redraw_requested = true;
  const uint8_t pressed = power::PressedButtons();
  if (pressed != g_last_button_mask) {
    KDASH_LOG.printf("buttons: mask=%u\n", (unsigned)pressed);
    g_last_button_mask = pressed;
  }
  if (!g_suppress_buttons) {
    const ButtonEvent event = g_buttons.Feed({millis(), pressed});
    if (event != ButtonEvent::None) {
      KDASH_LOG.printf("button: event=%u page_before=%u\n", (unsigned)event,
                       (unsigned)g_state.page);
      power::SetCyclePending(true);
      app::RunCycle(power::CycleWake::Button, event, g_state, g_snap, g_kis,
                    g_http, g_nvs, g_jitter_slot, KDASH_FW_VERSION);
      g_last_pass_ms = millis();
      g_state.last_page_change_ms = millis();  // 사람이 조작하면 자동 순환 타이머를 다시 시작
      KDASH_LOG.printf("button: page_after=%u display_failed=%d\n",
                       (unsigned)g_state.page, (int)g_state.display_failed);
    }
  } else if (!pressed) { g_suppress_buttons = false; g_buttons.Reset(); }
  // 같은 Wi-Fi 설정 포털(Always 모드 자동 열기, 요청 처리). 코드 변경/새로고침 요청이면 한 번 그린다.
  const portal::LanTick lan = portal::TickLanPortal(g_state, g_nvs, usb);
  if (lan != portal::LanTick::None) {
    power::SetCyclePending(true);
    app::RunCycle(power::CycleWake::Button,
                  lan == portal::LanTick::Refresh ? ButtonEvent::ShortKey0 : ButtonEvent::None,
                  g_state, g_snap, g_kis, g_http, g_nvs, g_jitter_slot, KDASH_FW_VERSION);
    g_last_pass_ms = millis();
  }
  // 생활 <-> 시장 자동 순환 (USB 전원일 때만, 사람이 누르지 않아도).
  if (usb) {
    app::RotationInput rot;
    rot.device = &g_state.settings.device;
    rot.page = g_state.page;
    rot.usb = usb;
    const int64_t now_utc = (int64_t)time(nullptr);
    rot.kst_hour = g_state.time_verified ? BreakKst(now_utc).hour : -1;
    if (g_state.time_verified) {  // 주말·공휴일·휴장일엔 시장 화면을 순환에 넣지 않는다(G-2)
      const cal::DayKind kind = cal::ClassifyKstDay(now_utc, g_state.calendars, 2);
      rot.trading_day = kind != cal::DayKind::Weekend && kind != cal::DayKind::PublicHoliday &&
                        kind != cal::DayKind::MarketClosed;
    }
    // "필요할 때" 포털을 쓰는 동안은 화면을 바꾸지 않는다(상시 모드는 늘 열려 있어 제외, G-3).
    rot.portal_open = g_state.portal.open &&
                      g_state.settings.device.lan_mode != LanPortalMode::Always;
    rot.now_ms = millis();
    rot.last_change_ms = g_state.last_page_change_ms;
    const app::RotationDecision d = app::DecideRotation(rot);
    if (d.due) {
      g_state.last_page_change_ms = millis();
      if (!d.change)
        KDASH_LOG.printf("rotate: hold page=%u kst_hour=%d\n", (unsigned)g_state.page, rot.kst_hour);
    }
    if (d.change) {
      KDASH_LOG.printf("rotate: page %u -> %u\n", (unsigned)g_state.page, (unsigned)d.next);
      g_state.page = d.next;
      g_state.redraw_requested = true;
      power::SetCyclePending(true);
      app::RunCycle(power::CycleWake::Button, ButtonEvent::None, g_state, g_snap, g_kis,
                    g_http, g_nvs, g_jitter_slot, KDASH_FW_VERSION);
      g_last_pass_ms = millis();
    }
  }
  if ((uint32_t)(millis() - g_last_pass_ms) >= kPassEveryMs) {
    g_last_pass_ms = millis();
    power::SetCyclePending(true);
    app::RunCycle(power::CycleWake::Timer, ButtonEvent::None, g_state, g_snap,
                  g_kis, g_http, g_nvs, g_jitter_slot, KDASH_FW_VERSION);
  }
  delay(10);
}
#else
#error "K-Dash firmware requires Arduino on reTerminal E1001"
#endif
