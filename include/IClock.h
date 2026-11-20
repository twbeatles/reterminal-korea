#pragma once
// Clock abstraction — production uses NTP/RTC, tests inject fake time.
#include <stdint.h>

namespace kdash {

class IClock {
 public:
  virtual ~IClock() = default;
  // Seconds since Unix epoch (UTC). Returns false when time is not verified
  // (RTC invalid + NTP unreachable): callers must show '시간 확인 필요'.
  virtual bool now_utc(int64_t& out_epoch) = 0;
  virtual bool is_verified() const = 0;
};

}  // namespace kdash
