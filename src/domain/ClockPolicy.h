#pragma once
// ClockPolicy: RTC + NTP trust decision (T03 3-1). Portable.
// - NTP ok && ntp >= build => NTP (WRITE_RTC when RTC differs >2s)
// - else RTC ok && !VL && rtc >= build => RTC
// - else unverified (callers must not fetch HTTPS, show "시간 확인 필요")
// RTC is stored as UTC (KST only at display).
#include <stdint.h>

namespace kdash {
namespace clock {

enum class Source : uint8_t { Ntp = 0, Rtc, None };
enum class Action : uint8_t { None = 0, WriteRtc };

struct Decision {
  int64_t epoch = 0;
  bool verified = false;
  Source source = Source::None;
  Action action = Action::None;
};

Decision Decide(bool rtc_ok, bool rtc_vl, int64_t rtc_epoch, bool ntp_ok,
                int64_t ntp_epoch, int64_t build_epoch);

}  // namespace clock
}  // namespace kdash
