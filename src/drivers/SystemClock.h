#pragma once
// SystemClock: IClock over PCF8563 + NTP (T03). Device I2C/NTP parts are
// compiled on ESP32 only; the policy glue stays portable.
#include "../../include/IClock.h"

namespace kdash {

class SystemClock : public IClock {
 public:
  explicit SystemClock(int64_t build_epoch) : build_(build_epoch) {}
  // Portable state updates (tested on host):
  void NoteRtc(bool ok, bool vl, int64_t epoch);
  void NoteNtp(bool ok, int64_t epoch);
  bool WantsRtcWrite() const { return write_rtc_; }

  bool now_utc(int64_t& out_epoch) override;
  bool is_verified() const override { return verified_; }

 private:
  int64_t build_ = 0;
  bool rtc_ok_ = false, rtc_vl_ = true;
  int64_t rtc_epoch_ = 0;
  bool ntp_ok_ = false;
  int64_t ntp_epoch_ = 0;
  bool verified_ = false;
  int64_t epoch_ = 0;
  bool write_rtc_ = false;
};

}  // namespace kdash
