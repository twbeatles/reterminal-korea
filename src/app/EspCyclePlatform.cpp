#include "CyclePlatform.h"
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "../boot/SerialLog.h"
#include "../drivers/Battery.h"
#include "../drivers/EspClockSync.h"
#include "../drivers/Pmic.h"
#include "../drivers/Sht4x_esp32.h"
#include "../drivers/Watchdog.h"
#include "../net/WifiConnector.h"
#include "../power/RtcPersistence.h"
#include "../power/SleepController.h"
#include "../provision/PortalRunner.h"
#include "../ui/FramePresenter.h"
#include "../ui/RefreshPolicy.h"
#include "../util/SnapshotLog.h"

namespace kdash { namespace app {
class EspCyclePlatform final : public ICyclePlatform {
 public:
  uint32_t Millis() override { return millis(); }
  int64_t Utc() override { return (int64_t)time(nullptr); }
  bool Usb() override { return pmic::UsbAttached(); }
  int BatteryMv() override { return ReadBatteryMv(); }
  uint32_t Crashes() override { return power::Crashes(); }
  void SyncClock(bool wifi, AppState& s) override { clock::SyncClock(wifi, s); }
  void OpenPortal(AppState& s, IStorage& st) override { portal::OpenPortal(s, st); }
  bool ReadIndoor(float& t, float& rh) override {
    const bool ok = kdash::ReadIndoor(t, rh);
    // SHT40 is near the board (self-heating possible), so this is a hint, not a cell/panel probe.
    if (ok && !ui::PanelTempInRange(t))
      KDASH_LOG.printf("display: indoor_c=%.1f outside_operating_range=%.0f..%.0f\n",
                       (double)t, (double)ui::kPanelMinTempC, (double)ui::kPanelMaxTempC);
    return ok;
  }
  bool Wifi() override { return WiFi.status() == WL_CONNECTED; }
  bool ConnectWifi(const Settings& s) override { return net::ConnectWifi(s); }
  bool HasRender() override { return ui::HasRender(); }
  bool Render(AppState& s, const Snapshot& snap, int mv, const char* version,
              int crashes, bool force) override {
    return ui::RenderSnapshot(s, snap, mv, version, crashes, force);
  }
  void Checkpoint(IStorage& st, const AppState& s, const Snapshot& sn, int64_t now) override {
    power::CheckpointRuntime(st, s, sn, now);
  }
  void ClearCrashes() override { power::ClearCrashes(); }
  void SetPending(bool value) override { power::SetCyclePending(value); }
  void Retain(const Snapshot& sn, const AppState& s, const KisSession& k) override {
    power::SaveRtc(sn, s, k);
  }
  void Sleep(int64_t seconds, const Snapshot& sn, const AppState& s, const KisSession& k) override {
    power::SleepFor(seconds, sn, s, k);
  }
  void LogPass(const PassLog& log, const Snapshot& sn) override {
    for (int i = 0; i < log.count; ++i)
      KDASH_LOG.printf("fetch: provider=%d outcome=%s http=%d requests=%d\n",
          (int)log.entries[i].provider, ToString(log.entries[i].outcome),
          log.entries[i].http_status, log.entries[i].requests);
    if (log.count) debug::PrintSnapshot(sn);
  }
  void Heartbeat() override { wdt::Feed(); }
};

void RunCycle(power::CycleWake reason, ButtonEvent event, AppState& state,
              Snapshot& snap, KisSession& kis, IHttpClient* http,
              IStorage& storage, int jitter_slot, const char* version) {
  EspCyclePlatform platform;
  RunCycle(reason, event, state, snap, kis, http, storage, jitter_slot, version, platform);
}
} }
#endif
