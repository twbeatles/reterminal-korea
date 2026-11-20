#include "LanPolicy.h"

#include "../power/BatteryPolicy.h"

namespace kdash {
namespace portal {

LanGate DecideLanGate(bool window_expired, bool closed, bool connected) {
  if (window_expired || closed) return LanGate::Close;
  return connected ? LanGate::Serve : LanGate::WaitLink;
}

bool LanBlockingShouldExit(uint32_t elapsed_ms, int battery_mv) {
  if (elapsed_ms >= kLanBlockingMaxMs) return true;
  return battery_mv >= 0 && battery::IsLowPower(battery_mv);
}

}  // namespace portal
}  // namespace kdash
