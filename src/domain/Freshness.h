#pragma once
// Freshness + KST helpers. Internal time is Unix UTC; KST strings are UI-only.
// Portable (no Arduino), mirrored by host/kdash/models.py.
#include <stdint.h>

#include "Types.h"

namespace kdash {

inline constexpr int32_t kKstOffsetSec = 9 * 3600;

struct KstBroken {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, weekday = 0;  // 0=Sun
};

// Days-from-civil (Howard Hinnant) -> epoch days; avoids libc timezone.
int64_t EpochFromCivil(int y, int m, int d);
void CivilFromEpochDays(int64_t z, int& y, int& m, int& d);
// True calendar date (02-30 etc. rejected by civil round-trip).
bool IsRealDate(int y, int mo, int d);
KstBroken BreakKst(int64_t epoch_utc);
int64_t MakeUtc(int y, int mo, int d, int h, int mi);
// KST wall-clock -> UTC epoch (h may be 24 => next day 00:00, AirKorea style).
int64_t KstToUtc(int y, int mo, int d, int h, int mi);
// yyyymmdd + hhmm (KST, as provider fields) -> UTC epoch, -1 when malformed.
int64_t KstYmdHmToUtc(int yyyymmdd, int hhmm);
// "YYYYMMDD" / "HHMM" strings (exact digits) -> UTC epoch, -1 when malformed.
int64_t KstStrToUtc(const char* yyyymmdd, const char* hhmm);

// Saturday/Sunday in KST (public-holiday calendars are provider-specific and
// layered on top; never claim Mon-Fri == business day, see 06).
bool IsKstWeekend(int64_t epoch_utc);

// UTC 15:00 == KST next-day 00:00 boundary (PWR/time regression anchor).
bool IsKstMidnightBoundary(int64_t epoch_utc);

// Stale policy: value older than ttl_sec (observed_at vs now) is Stale,
// never silently Valid. Unverified clock => caller shows '시간 확인 필요'.
Validity ClassifyFreshness(int64_t observed_utc, int64_t now_utc,
                           int64_t ttl_sec, bool clock_verified);

}  // namespace kdash
