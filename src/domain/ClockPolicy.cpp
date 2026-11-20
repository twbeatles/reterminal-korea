#include "ClockPolicy.h"

namespace kdash {
namespace clock {

Decision Decide(bool rtc_ok, bool rtc_vl, int64_t rtc_epoch, bool ntp_ok,
                int64_t ntp_epoch, int64_t build_epoch) {
  Decision d;
  if (ntp_ok && ntp_epoch >= build_epoch) {
    d.epoch = ntp_epoch;
    d.verified = true;
    d.source = Source::Ntp;
    if (rtc_ok) {
      int64_t diff = rtc_epoch > ntp_epoch ? rtc_epoch - ntp_epoch
                                           : ntp_epoch - rtc_epoch;
      if (rtc_vl || diff > 2) d.action = Action::WriteRtc;
    } else {
      d.action = Action::WriteRtc;
    }
    return d;
  }
  if (rtc_ok && !rtc_vl && rtc_epoch >= build_epoch) {
    d.epoch = rtc_epoch;
    d.verified = true;
    d.source = Source::Rtc;
    return d;
  }
  d.epoch = 0;
  d.verified = false;
  d.source = Source::None;
  return d;
}

}  // namespace clock
}  // namespace kdash
