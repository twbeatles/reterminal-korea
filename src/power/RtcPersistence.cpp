#include "RtcPersistence.h"

#include <memory>
#include <new>

#include "../app/CalendarRefresh.h"
#include "../app/Coordinator.h"
#include "../storage/CacheStore.h"
#include "../storage/CalendarStore.h"
#include "../storage/RuntimeStore.h"
#include "../storage/SettingsCodec.h"
#include "../ui/FramePresenter.h"
#include "RtcState.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <esp_system.h>
#include "../boot/SerialLog.h"
#endif

namespace kdash {
namespace power {

uint32_t SettingsFingerprint(const Settings& s) {
  // Same binding as the NVS checkpoint: data-relevant settings only (ADR 0026).
  return settings_codec::DataFingerprint(s);
}

void ApplyBootPage(app::AppState& state, bool config_saved, uint8_t retained_page) {
  state.page = (ui::Page)(config_saved ? state.settings.display_page : retained_page);
}
bool RestoreRetainedRuntime(const uint8_t* blob, size_t len, bool config_saved,
                            app::AppState& state, app::Snapshot& snap,
                            app::KisSession& kis, uint32_t& crc, rtc::RenderState& render) {
  uint8_t page=0, parts=0;
  uint32_t boots=0, crashes=0;
  if (!rtc::Decode(blob,len,snap,state.providers,(size_t)sched::Provider::Count,
                   kis,page,crc,parts,boots,crashes,&render)) return false;
  ApplyBootPage(state,config_saved,page);
  return true;
}

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
namespace {
RuntimeStore g_runtime;
// .rtc.data is initialized on watchdog/software reset in the pinned SDK.
// NOINIT retains reset evidence and CRC-guarded snapshots. Never read before
// BootTrackReset's magic/power-on checks; no secrets are written to NVS.
RTC_NOINIT_ATTR uint8_t g_rtc_blob[sizeof(rtc::Header) + rtc::kPayloadMax];
RTC_NOINIT_ATTR uint32_t g_rtc_length;
RTC_NOINIT_ATTR uint32_t g_rtc_settings;
RTC_NOINIT_ATTR uint32_t g_guard_magic;
RTC_NOINIT_ATTR uint32_t g_boots;
RTC_NOINIT_ATTR uint32_t g_crashes;
RTC_NOINIT_ATTR uint8_t g_cycle_pending;
RTC_NOINIT_ATTR uint32_t g_auth_clear;  // kAuthClearMagic => clear auth blocks once at boot
constexpr uint32_t kAuthClearMagic = 0x4B44434Cu;
uint32_t g_settings_fingerprint = 0;
}  // namespace

void RequestAuthClearOnBoot() { g_auth_clear = kAuthClearMagic; }

void SaveRtc(const app::Snapshot& snap, const app::AppState& state,
             const app::KisSession& kis) {
  g_rtc_length = (uint32_t)rtc::Encode(snap, state.providers,
      (size_t)sched::Provider::Count, kis, (uint8_t)state.page,
      ui::HasRender() && !state.display_failed ? ui::LastRenderCrc() : 0,
      0, g_boots, g_crashes,
      g_rtc_blob, sizeof(g_rtc_blob), ui::RetainedRenderState());
  g_rtc_settings = g_settings_fingerprint;
}

void RestoreRuntime(IStorage& storage, app::AppState& state,
                    app::Snapshot& snap, app::KisSession& kis) {
  g_settings_fingerprint = SettingsFingerprint(state.settings);
  g_runtime.Restore(storage, state.settings, state, snap);
  cal::LoadTables(storage, state.calendars);
  if (g_rtc_settings != g_settings_fingerprint) {
    g_rtc_length = 0;
    app::ResetCalendarRetry();
  }
  if (esp_reset_reason() != ESP_RST_POWERON && g_settings_fingerprint &&
      g_rtc_length >= sizeof(rtc::Header) && g_rtc_length <= sizeof(g_rtc_blob)) {
    uint32_t crc = 0;
    rtc::RenderState render;
    if (RestoreRetainedRuntime(g_rtc_blob,g_rtc_length,g_auth_clear == kAuthClearMagic,
                               state,snap,kis,crc,render)) {
      ui::RestoreRenderState(crc, render);
    }
  }
  // After a portal save that kept the caches: the user re-checked the keys, so
  // auth-blocked providers may try again (after both NVS and RTC restores).
  if (g_auth_clear == kAuthClearMagic) {
    app::ClearAuthBlocks(state);
    KDASH_LOG.println("restore: auth blocks cleared after settings save");
  }
  g_auth_clear = 0;
  // Clock trust is always re-established from RTC/NTP, never restored from flash.
  state.time_verified = false;
}

void InvalidateForPortal(IStorage& storage) {
  g_rtc_length = 0;
  g_rtc_settings = 0;
  g_runtime.Invalidate(storage);
}

bool CheckpointRuntime(IStorage& storage, const app::AppState& state,
                       const app::Snapshot& snap, int64_t now_utc) {
  const bool saved = g_runtime.Checkpoint(storage, state, snap, now_utc);
  if (g_runtime.CheckpointFailed()) KDASH_LOG.println("checkpoint: persistence FAILED");
  return saved;
}

void BootTrackReset() {
  const esp_reset_reason_t reset = esp_reset_reason();
  if (g_guard_magic != 0x4B444247u || reset == ESP_RST_POWERON) {
    g_guard_magic = 0x4B444247u;
    g_boots = g_crashes = g_rtc_length = 0;
    g_rtc_settings = g_auth_clear = 0;
    app::ResetCalendarRetry();
    g_cycle_pending = false;
  } else if (g_cycle_pending &&
             (reset == ESP_RST_PANIC || reset == ESP_RST_INT_WDT ||
              reset == ESP_RST_TASK_WDT || reset == ESP_RST_WDT)) {
    if (g_crashes < 100) ++g_crashes;
  }
  ++g_boots;
  g_cycle_pending = true;
}

uint32_t Boots() { return g_boots; }
uint32_t Crashes() { return g_crashes; }
void ClearCrashes() { g_crashes = 0; }
bool CyclePending() { return g_cycle_pending; }
void SetCyclePending(bool pending) { g_cycle_pending = pending; }

#else
// Host/CI guard: RTC slow memory needs the ESP32 toolchain.
// SettingsFingerprint() above stays portable for host tests.
#endif

}  // namespace power
}  // namespace kdash
