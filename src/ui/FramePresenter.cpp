#include "FramePresenter.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include <string.h>

#include <memory>
#include <new>

#include "../boot/SerialLog.h"
#include "../domain/Calendar.h"
#include "../drivers/EpdPanel.h"
#include "../drivers/Pmic.h"
#include "Canvas.h"
#include "Layout.h"
#include "RefreshPolicy.h"
#include "Renderer.h"
#include "ViewModel.h"

namespace kdash {
namespace ui {
namespace {
epd::EpdPanel g_panel;
Canvas* g_canvas = nullptr;
bool g_panel_started = false;
bool g_has_render = false;
uint32_t g_last_render_crc = 0;
// Content hash and UTC refresh time are retained in the CRC-guarded RTC blob.
uint32_t g_last_content_hash = 0;
bool g_has_content_hash = false;
uint32_t g_last_refresh_ms = 0;
int64_t g_last_refresh_utc = 0;
bool g_restored_refresh = false;
// Fast differential refresh (ADR 0034): copy of the frame now on the panel.
// RAM only: deep sleep loses it, so battery wakes always start with a Full frame.
uint8_t* g_prev = nullptr;
bool g_prev_valid = false;
bool g_prev_gray = false;  // panel shows a 4-gray frame (Full), not a mono fast frame
int g_partials = 0;
}  // namespace

namespace {
bool EnsureCanvas() {
  if (!g_canvas) {
    void* mem = psramFound() ? ps_malloc(sizeof(ui::Canvas)) : malloc(sizeof(ui::Canvas));
    if (!mem) return false;
    g_canvas = new (mem) ui::Canvas();
  }
  if (!g_panel_started) { g_panel.begin(); g_panel_started = true; }
  return true;
}

bool EnsurePrevBuffer() {
  if (!g_prev) {
    // PSRAM only: never take 96 KB of internal SRAM for an optional feature.
    g_prev = psramFound() ? static_cast<uint8_t*>(ps_malloc(Canvas::kBytes)) : nullptr;
    g_prev_valid = false;
  }
  return g_prev != nullptr;
}

// Pushes the canvas as Full (4-gray) or fast mono; a failed fast frame falls
// back to Full. Tracks the previous-frame copy and the partial counter.
bool PushFrame(bool fast_enabled, const char*& mode) {
  if (!fast_enabled) g_prev_valid = false;
  const bool have_prev = fast_enabled && EnsurePrevBuffer() && g_prev_valid;
  RefreshDecision d = DecideRefresh(true, true, g_partials,
                                    FastRefreshCaps(fast_enabled, g_panel.fast_capable(), have_prev));
  bool ok = false;
  if (d == RefreshDecision::Partial) {
    ok = g_panel.push_fast(g_prev, g_prev_gray, g_canvas->data());
    if (!ok) {
      KDASH_LOG.println("display: fast refresh FAILED, falling back to full");
      d = RefreshDecision::Full;
    }
  }
  if (d == RefreshDecision::Full) ok = g_panel.push_gray(g_canvas->data());
  mode = d == RefreshDecision::Partial ? "fast" : "full";
  if (!ok) {
    g_prev_valid = false;  // panel content unknown: next frame must be Full
    return false;
  }
  g_partials = NoteRefreshDone(d, g_partials);
  if (fast_enabled && g_prev) {
    memcpy(g_prev, g_canvas->data(), Canvas::kBytes);
    g_prev_valid = true;
    g_prev_gray = d == RefreshDecision::Full;
  }
  return true;
}

const char* MarketState(int64_t now, const app::AppState& state) {
  return cal::MarketStateText(now, state.time_verified, state.calendars, 2);
}
}  // namespace

bool RenderSnapshot(app::AppState& state, const app::Snapshot& snap,
                    int battery_mv, const char* fw_version, int crashes,
                    bool force) {
  if (!EnsureCanvas()) { KDASH_LOG.println("display: allocation FAILED"); return false; }
  const int64_t now = (int64_t)time(nullptr);
  const bool wifi = WiFi.status() == WL_CONNECTED;
  // View models return directly into bounded heap storage; never stack a framebuffer.
  switch (state.page) {
    case ui::Page::Life: {
      std::unique_ptr<vm::LifePage> page(new (std::nothrow)
          vm::LifePage(vm::BuildLife(snap, state, now, state.time_verified, wifi, false)));
      if (!page) return false;
      ui::RenderLife(*page, *g_canvas);
      break;
    }
    case ui::Page::Market: {
      std::unique_ptr<vm::MarketPage> page(new (std::nothrow)
          vm::MarketPage(vm::BuildMarket(snap, state, now, state.time_verified,
                                         wifi, MarketState(now, state), false)));
      if (!page) return false;
      ui::RenderMarket(*page, *g_canvas);
      break;
    }
    case ui::Page::Calendar: {
      std::unique_ptr<vm::CalendarPage> page(new (std::nothrow)
          vm::CalendarPage(vm::BuildCalendar(state, now, state.time_verified, wifi, false)));
      if (!page) return false;
      ui::RenderCalendar(*page, *g_canvas);
      break;
    }
    default: {
      std::unique_ptr<vm::DevicePage> page(new (std::nothrow)
          vm::DevicePage(vm::BuildDevice(state, now, state.time_verified, wifi,
                                         battery_mv, fw_version, pmic::UsbAttached(),
                                         crashes >= 3)));
      if (!page) return false;
      ui::RenderDevice(*page, *g_canvas);
      break;
    }
  }
  const uint32_t crc = g_canvas->crc32();
  const Rect clock{kRightX, kHeaderTop, 748 - kRightX, 28};
  const uint32_t content = g_canvas->content_hash(clock);
  const bool anything_changed = crc != g_last_render_crc;
  const bool content_changed = g_has_content_hash ? content != g_last_content_hash
                                                  : anything_changed;
  if (!ShouldPresent(force, g_has_render, content_changed, anything_changed,
                     g_restored_refresh ?
                       (state.time_verified && now >= g_last_refresh_utc &&
                        now - g_last_refresh_utc < 1800 ?
                          (uint32_t)(now - g_last_refresh_utc) * 1000u : kClockOnlyRefreshMs) :
                       (uint32_t)(millis() - g_last_refresh_ms)))
  {
    if (anything_changed)
      KDASH_LOG.printf("display: skip page=%u reason=clock_only\n", (unsigned)state.page);
    return true;  // nothing visible changed (or only the clock): keep the panel as is
  }
  const uint32_t started = millis();
  const char* mode = "full";
  const bool refreshed = PushFrame(state.settings.device.fast_refresh, mode);
  KDASH_LOG.printf("display: page=%u crc=%08lx mode=%s partials=%d elapsed_ms=%lu ok=%d\n",
                   (unsigned)state.page, (unsigned long)crc, mode, g_partials,
                   (unsigned long)(millis() - started), (int)refreshed);
  if (!refreshed) {
    KDASH_LOG.println("display: refresh FAILED");
    return false;
  }
  g_last_render_crc = crc;
  g_last_content_hash = content;
  g_has_content_hash = true;
  g_last_refresh_ms = millis();
  g_last_refresh_utc = state.time_verified ? now : 0;
  g_restored_refresh = false;
  g_has_render = true;
  state.last_render_utc = state.time_verified ? now : 0;
  return true;
}

bool ShowPortal(const char* ap, const char* pass, const char* url) {
  if (!EnsureCanvas()) return false;
  ui::RenderPortal(*g_canvas, ap, pass, url);
  // The AP password appears only on the local panel, never UART/portal responses.
  const bool shown = g_panel.push_gray(g_canvas->data());
  g_has_render = false;  // normal page must replace the setup screen on close
  g_prev_valid = false;  // setup screen is not tracked: next page is a Full frame
  return shown;
}

bool ShowLanPortal(const char* url, const char* host, const char* code) {
  if (!EnsureCanvas()) return false;
  ui::RenderLanPortal(*g_canvas, url, host, code);
  const bool shown = g_panel.push_gray(g_canvas->data());  // 코드는 패널에만 표시
  g_has_render = false;
  g_prev_valid = false;  // 포털 화면은 추적하지 않음: 다음 페이지는 Full
  return shown;
}

bool HasRender() { return g_has_render; }
uint32_t LastRenderCrc() { return g_last_render_crc; }
void SetRenderState(uint32_t crc, bool has) {
  g_last_render_crc = crc;
  g_has_render = has;
}
rtc::RenderState RetainedRenderState() {
  return {g_last_content_hash, g_has_content_hash ? g_last_refresh_utc : 0};
}
void RestoreRenderState(uint32_t crc, const rtc::RenderState& render) {
  SetRenderState(crc, crc != 0);
  g_has_content_hash = crc != 0 && render.refreshed_utc > 0;
  g_last_content_hash = render.content_hash;
  g_last_refresh_utc = render.refreshed_utc;
  g_restored_refresh = g_has_content_hash;
}
void HibernatePanel() {
  if (g_panel_started) g_panel.hibernate();
}

}  // namespace ui
}  // namespace kdash
#else
// Host/CI guard: panel/Canvas hardware needs the ESP32 toolchain.
#endif
