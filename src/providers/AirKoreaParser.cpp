#include "AirKoreaParser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../domain/Freshness.h"
#include "ResultCodes.h"

namespace kdash {
namespace air {
namespace {

constexpr const char* kSource = "에어코리아";

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

bool ParseInt(const char* v, int& out) {
  if (!v) return false;
  while (*v && isspace((unsigned char)*v)) v++;
  if (*v == '+') v++;
  // "-" 단독/빈값/음수/점검 문구 => 결측.
  if (!isdigit((unsigned char)*v)) return false;
  long acc = 0;
  while (isdigit((unsigned char)*v)) {
    acc = acc * 10 + (*v - '0');
    if (acc > 100000) return false;  // implausible concentration
    v++;
  }
  while (*v && isspace((unsigned char)*v)) v++;
  if (*v != '\0') return false;
  out = (int)acc;
  return true;
}

bool StationMatches(const Row& r, const char* filter) {
  if (!filter || !*filter) return true;
  if (!r.station || !*r.station) return true;  // per-station op may omit it
  return strcmp(r.station, filter) == 0;
}

}  // namespace

bool IsMissingToken(const char* v) {
  if (!v) return true;
  while (*v && isspace((unsigned char)*v)) v++;
  if (*v == '\0') return true;
  return Eq(v, "-") || Eq(v, "null") || Eq(v, "NULL");
}

int64_t ParseDataTimeKst(const char* dt) {
  // "2026-11-01 14:00" (KST) -> UTC epoch. "24:00" == next day 00:00.
  if (!dt) return -1;
  int y, mo, d, h, mi;
  char tail;
  if (sscanf(dt, "%4d-%2d-%2d %2d:%2d%c", &y, &mo, &d, &h, &mi, &tail) != 5)
    return -1;
  if (y < 2000 || !IsRealDate(y, mo, d) || h < 0 || h > 24 || mi < 0 ||
      mi > 59 || (h == 24 && mi != 0))
    return -1;
  return KstToUtc(y, mo, d, h, mi);
}

AirOut Parse(const Row* rows, size_t n, const char* result_code,
             bool body_is_xml, const char* station_filter,
             const FetchContext& ctx) {
  AirOut out;
  out.pm10.source = kSource;
  out.pm25.source = kSource;
  out.pm10.fetched_at = DeviceTs(ctx);
  out.pm25.fetched_at = DeviceTs(ctx);
  if (station_filter) {
    strncpy(out.station, station_filter, sizeof(out.station) - 1);
  }
  out.outcome = codes::DataGoKr(result_code);
  if (body_is_xml && out.outcome == Outcome::Ok)
    out.outcome = Outcome::ProviderError;
  if (out.outcome != Outcome::Ok) {
    const Validity v = out.outcome == Outcome::NoData ? Validity::Missing
                                                      : Validity::ProviderError;
    out.pm10.validity = v;
    out.pm25.validity = v;
    return out;
  }
  out.pm10.validity = Validity::Missing;
  out.pm25.validity = Validity::Missing;
  if (!rows || n == 0) {
    out.outcome = Outcome::NoData;
    return out;
  }
  bool any_station_row = false;
  struct Slot {
    DataPoint<int>* dst;
    const char* Row::*field;
  } slots[] = {{&out.pm10, &Row::pm10_value}, {&out.pm25, &Row::pm25_value}};
  for (size_t i = 0; i < n; i++) {
    if (!StationMatches(rows[i], station_filter)) continue;
    any_station_row = true;
    const int64_t t = ParseDataTimeKst(rows[i].data_time);
    if (t > out.data_time_utc) out.data_time_utc = t;
    for (Slot& s : slots) {
      if (s.dst->has_value && t <= s.dst->observed_at.epoch_utc) continue;
      int v = 0;
      if (!ParseInt(rows[i].*(s.field), v)) continue;  // AIR-01: never 0
      s.dst->has_value = true;
      s.dst->value = v;
      s.dst->observed_at = SourceTs(t > 0 ? t : 0);
      // Same rule for PM10 and PM2.5; unparseable/future time => Stale.
      s.dst->validity = ClassifyFreshness(t, ctx.now_utc, kAirTtlSec,
                                          ctx.clock_verified);
    }
  }
  if (!any_station_row) {
    out.outcome = Outcome::NoData;  // AIR-02: configured station absent
  }
  return out;
}

}  // namespace air
}  // namespace kdash
