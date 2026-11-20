#include "CyclePlanner.h"

#include "BatteryPolicy.h"

namespace kdash {
namespace power {

CyclePlan PlanCycle(const CycleInput& in) {
  CyclePlan p;
  if (!in.st) {
    p.enter_portal = true;
    p.sleep_seconds = kMinSleepSec;
    return p;
  }
  p.page = in.st->page;
  if (in.crashes >= 3) {
    p.safe_mode = true;
    p.render = true;
    p.sleep_seconds = 6 * 3600;
    return p;
  }
  // Power protection also applies to first boot and manual button actions.
  if (!in.usb && in.battery_mv >= 0 && battery::IsLowPower(in.battery_mv)) {
    p.low_battery_mode = true;
    p.render = true;
    p.sleep_seconds = kMaxSleepLowSec;
    return p;
  }
  // First boot without config => portal.
  bool has_wifi = in.st->settings.wifi_ssid[0] != '\0';
  if (in.reason == CycleWake::FirstBoot && !has_wifi) {
    p.enter_portal = true;
    p.render = true;
    p.sleep_seconds = kMinSleepSec;
    return p;
  }
  // Buttons.
  if (in.reason == CycleWake::Button) {
    if ((in.button_mask & 0x01) && in.long_press) {
      p.enter_portal = true;
      p.render = true;
      p.sleep_seconds = kMinSleepSec;
      return p;
    }
    // Front view, left to right: KEY2 (gray) previous page, KEY1 (gray) next page,
    // KEY0 (green) refresh; KEY0 held 2 s opens setup (handled above).
    if (in.button_mask & 0x04) {
      p.page = app::PrevPage(in.st->page);
      p.render = true;
      p.sleep_seconds = in.usb ? 30 : kMaxSleepBatterySec;
      return p;
    }
    if (in.button_mask & 0x02) {
      p.page = app::NextPage(in.st->page);
      p.render = true;
      p.sleep_seconds = in.usb ? 30 : kMaxSleepBatterySec;
      return p;
    }
    if (in.button_mask & 0x01) {
      p.force_refresh = true;
      p.connect_wifi = in.clock_verified;
      // A refresh press with an untrusted clock still tries NTP; HTTPS waits
      // until the clock is verified (RunCycle re-checks after SyncClock).
      p.sync_clock = !in.clock_verified;
      p.render = true;
      p.sleep_seconds = kMinSleepSec;
      return p;
    }
  }
  // Clock unverified => retry in 30 min, no HTTPS.
  if (!in.clock_verified) {
    p.sync_clock = true;
    p.sleep_seconds = 30 * 60;
    p.render = true;
    return p;
  }
  // Timer wake: fetch only when something is due.
  if (in.next_due_utc == INT64_MAX) {
    p.sleep_seconds = in.usb ? 30 : kMaxSleepBatterySec;
    return p;
  }
  if (in.next_due_utc <= in.now_utc) {
    p.connect_wifi = true;
    p.render = true;  // data may change
    int64_t next = in.next_due_utc;
    int64_t sleep = next - in.now_utc;
    if (sleep < kMinSleepSec) sleep = kMinSleepSec;
    // low_battery_mode always returns early above (non-USB + low), so the
    // timer-path cap is the constant battery cadence.
    int64_t maxs = kMaxSleepBatterySec;
    if (sleep > maxs) sleep = maxs;
    p.sleep_seconds = sleep;
    return p;
  }
  int64_t sleep = in.next_due_utc - in.now_utc;
  if (sleep < kMinSleepSec) sleep = kMinSleepSec;
  int64_t maxs = kMaxSleepBatterySec;
  if (sleep > maxs) sleep = maxs;
  p.sleep_seconds = sleep;
  p.render = false;
  return p;
}

}  // namespace power
}  // namespace kdash
