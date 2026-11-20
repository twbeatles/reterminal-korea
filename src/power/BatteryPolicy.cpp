#include "BatteryPolicy.h"

#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace battery {

State Classify(int mv) {
  if (mv < 0) return State::Unknown;
  if (mv < kCriticalMv) return State::Critical;
  if (mv < kLowMv) return State::Low;
  return State::Normal;
}

bool IsLowPower(int mv) {
  State s = Classify(mv);
  return s == State::Low || s == State::Critical;
}

int MeanOf(const int* mv, size_t n) {
  if (!mv || n == 0) return -1;
  long sum = 0;
  for (size_t i = 0; i < n; i++) {
    if (mv[i] < 0) return -1;
    sum += mv[i];
  }
  return (int)(sum / (long)n);
}

// Official E1001/E1002 % -> mV table (index = %, value = mV).
// Source: SenseCraft_HMI boards/reterminal_e1001/config.h @8d97d891.
const int16_t kCurveMv[101] = {
    2795, 2795, 2990, 3107, 3189, 3252, 3300, 3334, 3354, 3367, 3378, 3389,
    3399, 3409, 3418, 3426, 3434, 3442, 3449, 3457, 3464, 3471, 3478, 3485,
    3491, 3498, 3504, 3510, 3516, 3522, 3528, 3534, 3540, 3546, 3552, 3558,
    3564, 3569, 3575, 3581, 3587, 3592, 3598, 3603, 3609, 3614, 3620, 3625,
    3631, 3636, 3642, 3647, 3653, 3658, 3664, 3669, 3675, 3680, 3686, 3691,
    3697, 3703, 3709, 3715, 3721, 3727, 3733, 3739, 3746, 3752, 3759, 3765,
    3772, 3779, 3786, 3793, 3801, 3808, 3816, 3824, 3832, 3840, 3849, 3858,
    3867, 3876, 3886, 3896, 3906, 3916, 3927, 3984, 3995, 4007, 4019, 4032,
    4045, 4059, 4074, 4090, 4111,
};

int PercentMv(int mv) {
  if (mv <= kCurveMv[0]) return 0;
  if (mv >= kCurveMv[100]) return 100;
  for (int p = 0; p < 100; p++) {
    if (mv < kCurveMv[p + 1]) {
      int lo = kCurveMv[p], hi = kCurveMv[p + 1];
      if (hi <= lo) return p;
      // Round to nearest percent within the segment.
      return p + (1000 * (mv - lo) / (hi - lo) + 500) / 1000;
    }
  }
  return 100;
}

}  // namespace battery
}  // namespace kdash
