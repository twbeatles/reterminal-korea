#pragma once
// CyclePlanner: wake -> plan (T05 3-1). Portable.
// Decides Wi-Fi/render/portal/sleep from wake reason, buttons, clock,
// battery and provider due state. Never loops; single pure function.
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../ui/Layout.h"

namespace kdash {
namespace power {

enum class CycleWake : uint8_t { Timer = 0, Button, FirstBoot, Unknown };

struct CycleInput {
  CycleWake reason = CycleWake::Timer;
  uint8_t button_mask = 0;  // bit0=KEY0 ...
  bool long_press = false;
  int64_t now_utc = 0;
  bool clock_verified = false;
  bool usb = false;  // U-12 unresolved => false (battery schedule)
  int battery_mv = -1;
  const app::AppState* st = nullptr;
  int64_t next_due_utc = INT64_MAX;
  int crashes = 0;
};

struct CyclePlan {
  bool connect_wifi = false;
  bool sync_clock = false;    // NTP recovery only; HTTPS still requires verified UTC
  bool force_refresh = false;
  bool enter_portal = false;
  ui::Page page = ui::Page::Life;
  bool render = false;
  int64_t sleep_seconds = 3600;
  bool low_battery_mode = false;
  bool safe_mode = false;
};

inline constexpr int64_t kMaxSleepBatterySec = 6 * 3600;
inline constexpr int64_t kMaxSleepLowSec = 24 * 3600;
inline constexpr int64_t kMinSleepSec = 60;

CyclePlan PlanCycle(const CycleInput& in);

}  // namespace power
}  // namespace kdash
