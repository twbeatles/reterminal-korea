#include "KmaParser.h"

#include <ctype.h>
#include <string.h>

#include "../domain/Freshness.h"
#include "ResultCodes.h"

namespace kdash {
namespace kma {
namespace {

constexpr const char* kSource = "기상청";

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

template <typename T>
DataPoint<T> Empty(Validity v, const FetchContext& ctx) {
  DataPoint<T> d;
  d.validity = v;
  d.source = kSource;
  d.fetched_at = DeviceTs(ctx);
  return d;
}

Outcome Resolve(const char* result_code, bool body_is_xml) {
  Outcome o = codes::DataGoKr(result_code);
  if (body_is_xml && o == Outcome::Ok) o = Outcome::ProviderError;
  return o;
}

// Failure validity for a non-Ok outcome: "no data yet" is Missing, the rest
// is a provider error (never 0, never silently valid).
Validity FailureValidity(Outcome o) {
  return o == Outcome::NoData ? Validity::Missing : Validity::ProviderError;
}

template <typename T>
void Fill(DataPoint<T>& d, T v, int64_t observed, Validity validity) {
  d.has_value = true;
  d.value = v;
  d.validity = validity;
  d.observed_at = SourceTs(observed);
}

void PrevDay(int& yy, int& mo, int& dd) {
  int64_t days = EpochFromCivil(yy, mo, dd) / 86400 - 1;
  CivilFromEpochDays(days, yy, mo, dd);
}

}  // namespace

bool IsMissingToken(const char* v) {
  if (!v) return true;
  while (*v && isspace((unsigned char)*v)) v++;
  if (*v == '\0') return true;
  if (Eq(v, "-") || Eq(v, "null") || Eq(v, "NULL")) return true;
  if (strcmp(v, "강수없음") == 0) return true;  // RN1/PCP no-rain word
  return false;
}

double ParseDecimal(const char* v, bool& ok) {
  ok = false;
  if (IsMissingToken(v)) return 0.0;
  while (isspace((unsigned char)*v)) v++;
  double sign = 1.0, acc = 0.0, frac = 0.0, div = 1.0;
  const char* p = v;
  if (*p == '+' || *p == '-') {
    if (*p == '-') sign = -1.0;
    p++;
  }
  bool any = false;
  while (*p && *p != '.' && !isspace((unsigned char)*p)) {
    if (!isdigit((unsigned char)*p)) return 0.0;
    acc = acc * 10 + (*p - '0');
    any = true;
    p++;
  }
  if (*p == '.') {
    p++;
    while (*p && !isspace((unsigned char)*p)) {
      if (!isdigit((unsigned char)*p)) return 0.0;
      frac = frac * 10 + (*p - '0');
      div *= 10;
      any = true;
      p++;
    }
  }
  while (isspace((unsigned char)*p)) p++;
  if (*p != '\0' || !any) return 0.0;
  const double r = sign * (acc + frac / div);
  // KMA guide: values >= +900 or <= -900 are missing-value sentinels.
  if (r >= 900.0 || r <= -900.0) return 0.0;
  ok = true;
  return r;
}

bool SelectUltraNowBase(int64_t now_utc, int& yyyymmdd, int& hhmm) {
  // 정시 생성(HH00), requestable from HH:kUltraNowDelayMin. Before that, use
  // the previous hour, borrowing across midnight (KMA-03).
  KstBroken k = BreakKst(now_utc);
  int hh = k.hour, yy = k.year, mo = k.month, dd = k.day;
  if (k.minute < kUltraNowDelayMin) {
    hh -= 1;
    if (hh < 0) {
      hh = 23;
      PrevDay(yy, mo, dd);
    }
  }
  yyyymmdd = yy * 10000 + mo * 100 + dd;
  hhmm = hh * 100;
  return true;
}

bool SelectVillageBase(int64_t now_utc, int& yyyymmdd, int& hhmm) {
  static const int kSlots[] = {2, 5, 8, 11, 14, 17, 20, 23};
  KstBroken k = BreakKst(now_utc);
  int slot = -1;
  for (int s : kSlots) {
    if (k.hour > s || (k.hour == s && k.minute >= kVillageDelayMin)) slot = s;
  }
  int yy = k.year, mo = k.month, dd = k.day;
  if (slot < 0) {  // before 02:10 => yesterday's 23:00 issue (midnight rule)
    slot = 23;
    PrevDay(yy, mo, dd);
  }
  yyyymmdd = yy * 10000 + mo * 100 + dd;
  hhmm = slot * 100;
  return true;
}

int64_t UltraNowBaseUtc(int64_t now_utc) {
  int d = 0, t = 0;
  SelectUltraNowBase(now_utc, d, t);
  return KstYmdHmToUtc(d, t);
}

int64_t VillageBaseUtc(int64_t now_utc) {
  int d = 0, t = 0;
  SelectVillageBase(now_utc, d, t);
  return KstYmdHmToUtc(d, t);
}

NowOut ParseUltraNow(const RawItem* items, size_t n, const char* result_code,
                     bool body_is_xml, int base_yyyymmdd, int base_hhmm,
                     const FetchContext& ctx) {
  NowOut out;
  out.outcome = Resolve(result_code, body_is_xml);
  out.base_utc = KstYmdHmToUtc(base_yyyymmdd, base_hhmm);
  if (out.outcome != Outcome::Ok) {
    Validity v = FailureValidity(out.outcome);
    out.temp_c = Empty<double>(v, ctx);
    out.humidity = Empty<double>(v, ctx);
    out.rain_mm = Empty<double>(v, ctx);
    return out;  // XML error / serviceKey error => not Ok (KMA-02)
  }
  out.temp_c = Empty<double>(Validity::Missing, ctx);
  out.humidity = Empty<double>(Validity::Missing, ctx);
  out.rain_mm = Empty<double>(Validity::Missing, ctx);
  if (n == 0 || !items) {
    out.outcome = Outcome::NoData;
    return out;
  }
  const Validity fresh = ClassifyFreshness(out.base_utc, ctx.now_utc,
                                           kUltraNowTtlSec, ctx.clock_verified);
  for (size_t i = 0; i < n; i++) {
    DataPoint<double>* dst = nullptr;
    if (Eq(items[i].category, "T1H")) dst = &out.temp_c;
    else if (Eq(items[i].category, "REH")) dst = &out.humidity;
    else if (Eq(items[i].category, "RN1")) dst = &out.rain_mm;
    if (!dst || dst->has_value) continue;  // first occurrence wins
    bool ok = false;
    double v = ParseDecimal(items[i].value, ok);
    if (ok) Fill(*dst, v, out.base_utc, fresh);  // "-" stays Missing
  }
  return out;
}

ForecastOut ParseVillage(const RawItem* items, size_t n,
                         const char* result_code, bool body_is_xml,
                         int base_yyyymmdd, int base_hhmm,
                         const FetchContext& ctx) {
  ForecastOut out;
  out.outcome = Resolve(result_code, body_is_xml);
  out.base_utc = KstYmdHmToUtc(base_yyyymmdd, base_hhmm);
  if (out.outcome != Outcome::Ok) {
    Validity v = FailureValidity(out.outcome);
    out.tmp = Empty<double>(v, ctx);
    out.pop = Empty<int>(v, ctx);
    out.pty = Empty<int>(v, ctx);
    out.sky = Empty<int>(v, ctx);
    return out;
  }
  out.tmp = Empty<double>(Validity::Missing, ctx);
  out.pop = Empty<int>(Validity::Missing, ctx);
  out.pty = Empty<int>(Validity::Missing, ctx);
  out.sky = Empty<int>(Validity::Missing, ctx);

  // Target slot: earliest forecast hour >= the current hour (ISSUE-001).
  // Items repeat per (fcstDate, fcstTime); never "last one wins".
  const int64_t hour_floor = ctx.now_utc - (((ctx.now_utc % 3600) + 3600) % 3600);
  int64_t target = -1;
  for (size_t i = 0; items && i < n; i++) {
    int64_t t = KstStrToUtc(items[i].fcst_date, items[i].fcst_time);
    if (t < 0 || t < hour_floor) continue;
    if (target < 0 || t < target) target = t;
  }
  if (target < 0) {
    out.outcome = Outcome::NoData;  // nothing at/after now in this response
    return out;
  }
  out.fcst_utc = target;
  const Validity fresh = ClassifyFreshness(out.base_utc, ctx.now_utc,
                                           kVillageTtlSec, ctx.clock_verified);
  for (size_t i = 0; i < n; i++) {
    if (KstStrToUtc(items[i].fcst_date, items[i].fcst_time) != target) continue;
    bool ok = false;
    double v = ParseDecimal(items[i].value, ok);
    if (!ok) continue;
    if (Eq(items[i].category, "TMP") && !out.tmp.has_value) {
      Fill(out.tmp, v, out.base_utc, fresh);
    } else if (Eq(items[i].category, "POP") && !out.pop.has_value) {
      Fill(out.pop, (int)v, out.base_utc, fresh);
    } else if (Eq(items[i].category, "PTY") && !out.pty.has_value) {
      Fill(out.pty, (int)v, out.base_utc, fresh);
    } else if (Eq(items[i].category, "SKY") && !out.sky.has_value) {
      Fill(out.sky, (int)v, out.base_utc, fresh);
    }
  }
  return out;
}

}  // namespace kma
}  // namespace kdash
