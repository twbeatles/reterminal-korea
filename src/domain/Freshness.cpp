#include "Freshness.h"

#include <string.h>

namespace kdash {

int64_t EpochFromCivil(int y, int m, int d) {
  int64_t yy = y;
  yy -= (m <= 2) ? 1 : 0;
  const int64_t era = (yy >= 0 ? yy : yy - 399) / 400;
  const int64_t yoe = yy - era * 400;
  const int64_t mp = (m + 9) % 12;
  const int64_t doy = (153 * mp + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (era * 146097 + doe - 719468) * 86400;
}

void CivilFromEpochDays(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = (int)(yoe + era * 400);
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y += (m <= 2) ? 1 : 0;
}

bool IsRealDate(int y, int mo, int d) {
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
  int ry = 0, rm = 0, rd = 0;
  CivilFromEpochDays(EpochFromCivil(y, mo, d) / 86400, ry, rm, rd);
  return ry == y && rm == mo && rd == d;
}

KstBroken BreakKst(int64_t epoch_utc) {
  KstBroken b;
  int64_t kst = epoch_utc + kKstOffsetSec;
  int64_t days = kst / 86400;
  int64_t rem = kst % 86400;
  if (rem < 0) {
    rem += 86400;
    days -= 1;
  }
  int y, m, d;
  CivilFromEpochDays(days, y, m, d);
  b.year = y;
  b.month = m;
  b.day = d;
  b.hour = (int)(rem / 3600);
  b.minute = (int)((rem % 3600) / 60);
  b.weekday = (int)(((days % 7) + 7 + 4) % 7);  // 1970-01-01 was Thursday
  return b;
}

int64_t MakeUtc(int y, int mo, int d, int h, int mi) {
  return EpochFromCivil(y, mo, d) + h * 3600 + mi * 60;
}

int64_t KstToUtc(int y, int mo, int d, int h, int mi) {
  return MakeUtc(y, mo, d, h, mi) - kKstOffsetSec;
}

int64_t KstYmdHmToUtc(int yyyymmdd, int hhmm) {
  const int y = yyyymmdd / 10000, mo = (yyyymmdd / 100) % 100,
            d = yyyymmdd % 100, h = hhmm / 100, mi = hhmm % 100;
  if (y < 2000 || y > 2200 || !IsRealDate(y, mo, d) || h < 0 || h > 24 ||
      mi < 0 || mi > 59 || (h == 24 && mi != 0))
    return -1;
  return KstToUtc(y, mo, d, h, mi);
}

static bool DigitsN(const char* s, size_t n, int& out) {
  if (!s || strlen(s) != n) return false;
  out = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    out = out * 10 + (s[i] - '0');
  }
  return true;
}

int64_t KstStrToUtc(const char* yyyymmdd, const char* hhmm) {
  int ymd = 0, hm = 0;
  if (!DigitsN(yyyymmdd, 8, ymd) || !DigitsN(hhmm, 4, hm)) return -1;
  return KstYmdHmToUtc(ymd, hm);
}

bool IsKstWeekend(int64_t epoch_utc) {
  int wd = BreakKst(epoch_utc).weekday;
  return wd == 0 || wd == 6;
}

bool IsKstMidnightBoundary(int64_t epoch_utc) {
  KstBroken b = BreakKst(epoch_utc);
  return b.hour == 0 && b.minute == 0;
}

Validity ClassifyFreshness(int64_t observed_utc, int64_t now_utc,
                           int64_t ttl_sec, bool clock_verified) {
  if (!clock_verified) return Validity::Stale;  // '시간 확인 필요' path
  if (observed_utc <= 0 || observed_utc > now_utc + 300) return Validity::Stale;
  return (now_utc - observed_utc) <= ttl_sec ? Validity::Valid : Validity::Stale;
}

}  // namespace kdash

namespace kdash {

const char* ToString(Outcome o) {
  switch (o) {
    case Outcome::Ok: return "OK";
    case Outcome::NoData: return "NO_DATA";
    case Outcome::AuthError: return "AUTH_ERROR";
    case Outcome::QuotaExceeded: return "QUOTA_EXCEEDED";
    case Outcome::RateLimited: return "RATE_LIMITED";
    case Outcome::ProviderError: return "PROVIDER_ERROR";
  }
  return "UNKNOWN";
}

const char* ToString(Validity v) {
  switch (v) {
    case Validity::Valid: return "VALID";
    case Validity::Missing: return "MISSING";
    case Validity::ProviderError: return "PROVIDER_ERROR";
    case Validity::Stale: return "STALE";
    case Validity::NotConfigured: return "NOT_CONFIGURED";
  }
  return "UNKNOWN";
}

}  // namespace kdash
