#include "ClockBootstrap.h"

namespace kdash { namespace clock {
Decision BootstrapClock(IClockPlatform& p, int64_t minimum, bool attempt_ntp) {
  int64_t rtc = 0, ntp = 0;
  bool vl = true;
  const bool rtc_ok = p.ReadRtc(rtc, vl);
  bool ntp_ok = p.TrustedNtpUtc(ntp);
  Decision d = Decide(rtc_ok, vl, rtc, ntp_ok, ntp, minimum);
  // TrustedNtpUtc is the already-running system clock, not a new sample.
  // SNTP also applies its own update; never round either down to whole seconds.
  if (d.verified && !ntp_ok && !p.ApplyUtc(d.epoch)) return Decision{};
  int64_t synced = 0;
  if (attempt_ntp && p.SyncNtp(synced) && synced >= minimum) {
    ntp_ok = true;
    ntp = synced;
    d = Decide(rtc_ok, vl, rtc, ntp_ok, ntp, minimum);
    // SyncNtp guarantees the system clock has already been updated.
    if (d.action == Action::WriteRtc) p.WriteRtc(d.epoch);
  }
  if (!d.verified) return d;
  return d;
}
} }  // namespace kdash::clock
