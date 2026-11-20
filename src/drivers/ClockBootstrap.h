#pragma once
#include "../domain/ClockPolicy.h"

namespace kdash { namespace clock {
class IClockPlatform {
 public:
  virtual ~IClockPlatform() = default;
  virtual bool ReadRtc(int64_t& epoch, bool& voltage_low) = 0;
  // Current running system UTC after actual NTP notification, not a sample to reapply.
  virtual bool TrustedNtpUtc(int64_t& epoch) = 0;
  // On success the platform has already applied NTP (including subsecond precision).
  virtual bool SyncNtp(int64_t& epoch) = 0;
  virtual bool ApplyUtc(int64_t epoch) = 0;
  virtual bool WriteRtc(int64_t epoch) = 0;
};
// Apply a verified RTC before attempting NTP so TLS may proceed when NTP is blocked.
Decision BootstrapClock(IClockPlatform& platform, int64_t minimum_epoch,
                        bool attempt_ntp);
} }  // namespace kdash::clock
