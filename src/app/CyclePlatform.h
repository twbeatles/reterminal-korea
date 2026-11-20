#pragma once
#include "CycleRunner.h"

namespace kdash { namespace app {
// Platform effects only. The same RunCycle orchestrator runs on host and E1001.
class ICyclePlatform {
 public:
  virtual ~ICyclePlatform() = default;
  virtual uint32_t Millis() = 0;
  virtual int64_t Utc() = 0;
  virtual bool Usb() = 0;
  virtual int BatteryMv() = 0;
  virtual uint32_t Crashes() = 0;
  virtual void SyncClock(bool wifi, AppState&) = 0;
  virtual void OpenPortal(AppState&, IStorage&) = 0;
  virtual bool ReadIndoor(float&, float&) = 0;
  virtual bool Wifi() = 0;
  virtual bool ConnectWifi(const Settings&) = 0;
  virtual bool HasRender() = 0;
  virtual bool Render(AppState&, const Snapshot&, int battery_mv,
                      const char* version, int crashes, bool force) = 0;
  virtual void Checkpoint(IStorage&, const AppState&, const Snapshot&, int64_t) = 0;
  virtual void ClearCrashes() = 0;
  virtual void SetPending(bool) = 0;
  virtual void Retain(const Snapshot&, const AppState&, const KisSession&) = 0;
  virtual void Sleep(int64_t, const Snapshot&, const AppState&, const KisSession&) = 0;
  virtual void LogPass(const PassLog&, const Snapshot&) = 0;
  virtual void Heartbeat() = 0;
};
void RunCycle(power::CycleWake reason, ButtonEvent event, AppState& state,
              Snapshot& snap, KisSession& kis, IHttpClient* http,
              IStorage& storage, int jitter_slot, const char* fw_version,
              ICyclePlatform& platform);
} }
