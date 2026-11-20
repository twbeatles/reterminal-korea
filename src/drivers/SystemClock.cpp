#include "SystemClock.h"

#include "../domain/ClockPolicy.h"

namespace kdash {

void SystemClock::NoteRtc(bool ok, bool vl, int64_t epoch) {
  rtc_ok_ = ok;
  rtc_vl_ = vl;
  rtc_epoch_ = epoch;
  clock::Decision d =
      clock::Decide(rtc_ok_, rtc_vl_, rtc_epoch_, ntp_ok_, ntp_epoch_, build_);
  verified_ = d.verified;
  epoch_ = d.epoch;
  write_rtc_ = d.action == clock::Action::WriteRtc;
}

void SystemClock::NoteNtp(bool ok, int64_t epoch) {
  ntp_ok_ = ok;
  ntp_epoch_ = epoch;
  clock::Decision d =
      clock::Decide(rtc_ok_, rtc_vl_, rtc_epoch_, ntp_ok_, ntp_epoch_, build_);
  verified_ = d.verified;
  epoch_ = d.epoch;
  write_rtc_ = d.action == clock::Action::WriteRtc;
}

bool SystemClock::now_utc(int64_t& out_epoch) {
  if (!verified_) return false;
  out_epoch = epoch_;
  return true;
}

}  // namespace kdash
