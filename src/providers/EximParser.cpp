#include "EximParser.h"

#include <ctype.h>
#include <string.h>

#include "../domain/Freshness.h"
#include "ResultCodes.h"

namespace kdash {
namespace exim {
namespace {

constexpr const char* kSource = "한국수출입은행";

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

void FillFx(DataPoint<FxRate>& d, const char* unit, int64_t ws, int32_t sc,
            int64_t fixing_utc, Validity v) {
  d.has_value = true;
  strncpy(d.value.currency_unit, unit, sizeof(d.value.currency_unit) - 1);
  d.value.won_scaled = ws;
  d.value.scale = sc;
  d.value.quote_kind = QuoteKind::OfficialDailyFx;
  d.validity = v;
  d.observed_at = SourceTs(fixing_utc);
}

}  // namespace

bool IsJpyUnit(const char* u) { return Eq(u, "JPY(100)"); }
bool IsUsdUnit(const char* u) { return Eq(u, "USD"); }

int ParseResultText(const char* t) {
  if (!t) return 0;
  while (isspace((unsigned char)*t)) t++;
  int v = 0;
  bool any = false;
  while (isdigit((unsigned char)*t) && v < 1000) {
    v = v * 10 + (*t - '0');
    any = true;
    t++;
  }
  while (isspace((unsigned char)*t)) t++;
  return (any && *t == '\0') ? v : 0;
}

bool ParseQuoted(const char* text, int64_t& won_scaled, int32_t& scale) {
  // "1,384.50" -> 138450 (scale 2). Spaces and thousand separators accepted;
  // "-", "", and non-numeric text => false (Missing).
  won_scaled = 0;
  scale = 2;
  if (!text) return false;
  char buf[24];  // 15 integer digits max => no int64 overflow
  size_t bl = 0;
  for (const char* p = text; *p; p++) {
    if (*p == ',' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      continue;
    if (bl + 1 >= sizeof(buf)) return false;
    buf[bl++] = *p;
  }
  buf[bl] = '\0';
  if (bl == 0 || buf[0] == '-') return false;
  int dots = 0;
  for (size_t i = 0; i < bl; i++) {
    if (buf[i] == '.') {
      if (++dots > 1) return false;
    } else if (!isdigit((unsigned char)buf[i])) {
      return false;
    }
  }
  const char* dot = strchr(buf, '.');
  const char* int_end = dot ? dot : buf + bl;
  if (int_end - buf > 15) return false;
  int64_t int_part = 0;
  for (const char* q = buf; q < int_end; q++) int_part = int_part * 10 + (*q - '0');
  int64_t frac = 0;
  int frac_digits = 0;
  if (dot) {
    for (const char* q = dot + 1; *q && frac_digits < 2; q++) {
      frac = frac * 10 + (*q - '0');
      frac_digits++;
    }  // digits beyond 0.01원 are truncated (no float anywhere)
  }
  while (frac_digits < 2) {
    frac *= 10;
    frac_digits++;
  }
  won_scaled = int_part * 100 + frac;
  return won_scaled > 0;  // a 0원 fixing is never a real value
}

EximOut Parse(const Entry* entries, size_t n, int searchdate_yyyymmdd,
              const FetchContext& ctx) {
  EximOut out;
  out.fixing_date_utc = KstYmdHmToUtc(searchdate_yyyymmdd, 0);
  DataPoint<FxRate>* both[] = {&out.usd, &out.jpy100};
  for (DataPoint<FxRate>* d : both) {
    d->source = kSource;
    d->validity = Validity::Missing;
    d->fetched_at = DeviceTs(ctx);
  }
  if (!entries || n == 0) {
    out.empty_response = true;  // holiday / not fixed yet => walk back
    out.outcome = Outcome::NoData;
    return out;
  }
  for (size_t i = 0; i < n && !out.error_result; i++) {
    // Non-numeric/absent RESULT (parsed as 0) is reported as -1 (error).
    if (entries[i].result != 1)
      out.error_result = entries[i].result ? entries[i].result : -1;
  }
  if (out.error_result) {  // auth / quota / data-code => error, no walk-back
    out.outcome = codes::Exim(out.error_result);
    if (out.outcome == Outcome::Ok) out.outcome = Outcome::ProviderError;
    out.usd.validity = Validity::ProviderError;
    out.jpy100.validity = Validity::ProviderError;
    return out;
  }
  const Validity fresh = ClassifyFreshness(out.fixing_date_utc, ctx.now_utc,
                                           kFixingTtlSec, ctx.clock_verified);
  for (size_t i = 0; i < n; i++) {
    int64_t ws = 0;
    int32_t sc = 2;
    if (IsUsdUnit(entries[i].cur_unit) && !out.usd.has_value &&
        ParseQuoted(entries[i].deal_bas_r, ws, sc)) {
      FillFx(out.usd, "USD", ws, sc, out.fixing_date_utc, fresh);
    } else if (IsJpyUnit(entries[i].cur_unit) && !out.jpy100.has_value &&
               ParseQuoted(entries[i].deal_bas_r, ws, sc)) {
      FillFx(out.jpy100, "JPY(100)", ws, sc, out.fixing_date_utc, fresh);
    }
  }
  if (!out.usd.has_value && !out.jpy100.has_value) {
    // RESULT=1 rows but no usable USD/JPY value (e.g. pre-fixing blanks):
    // treat like "not fixed for this day" and walk back.
    out.empty_response = true;
    out.outcome = Outcome::NoData;
    return out;
  }
  out.outcome = Outcome::Ok;
  return out;
}

size_t PlanSearchDates(int start_yyyymmdd, int max_days, int* out,
                       size_t cap) {
  if (!out || cap == 0 || max_days <= 0) return 0;
  int y = start_yyyymmdd / 10000;
  int m = (start_yyyymmdd / 100) % 100;
  int d = start_yyyymmdd % 100;
  int64_t days = EpochFromCivil(y, m, d) / 86400;
  size_t n = 0;
  for (int i = 0; i < max_days && n < cap; i++) {
    int yy, mm, dd;
    CivilFromEpochDays(days - i, yy, mm, dd);
    out[n++] = yy * 10000 + mm * 100 + dd;
  }
  return n;
}

}  // namespace exim
}  // namespace kdash
