#pragma once
// EXIM (한국수출입은행 현재환율 API) — parsing + business-day fallback planning.
// Host: oapi.koreaexim.go.kr .../exchangeJSON?authkey&searchdate&data=AP01.
// Labels: '고시 환율'. USD = KRW/1USD, JPY(100) = KRW/100JPY — unit is part
// of the value and always shown (EXIM-01).
// Empty array => holiday / not yet fixed => walk back up to 7 calendar days,
// one request per day (EXIM-02). A RESULT other than 1 (auth/quota/data-code
// error) is an ERROR and stops the walk-back (ISSUE-005).
#include <stddef.h>
#include <stdint.h>

#include "../domain/Types.h"

namespace kdash {
namespace exim {

struct Entry {
  const char* cur_unit = nullptr;    // "USD", "JPY(100)", ...
  const char* deal_bas_r = nullptr;  // "1,384.50" (천단위 구분 포함 가능)
  int result = 0;                    // RESULT as number (JSON number or "1")
};

struct EximOut {
  DataPoint<FxRate> usd;
  DataPoint<FxRate> jpy100;
  int64_t fixing_date_utc = 0;  // 기준일 (KST 자정, UTC로 환산)
  bool empty_response = false;  // true => caller should try previous day
  int error_result = 0;         // first RESULT != 1 seen (0 when none)
  Outcome outcome = Outcome::ProviderError;
};

// Parses one response already split into entries. `searchdate_yyyymmdd` is
// the requested 기준일. Values are Valid only while the 기준일 is within
// kFixingTtlSec of now (고시환율 is a daily value; weekends/holidays keep
// the last fixing which is still the official one).
inline constexpr int64_t kFixingTtlSec = 4 * 24 * 3600;
EximOut Parse(const Entry* entries, size_t n, int searchdate_yyyymmdd,
              const FetchContext& ctx);

// "1,384.50" -> won_scaled/scale (fixed point, scale=2). false on
// missing/unparseable input (caller keeps Missing, never 0).
bool ParseQuoted(const char* text, int64_t& won_scaled, int32_t& scale);

// Candidate searchdates (KST yyyymmdd) for at most `max_days` calendar days
// back from `start_yyyymmdd`.
size_t PlanSearchDates(int start_yyyymmdd, int max_days, int* out, size_t cap);

// "1" / " 1 " -> 1; non-numeric -> 0 (treated as error, never success).
int ParseResultText(const char* text);

bool IsJpyUnit(const char* cur_unit);
bool IsUsdUnit(const char* cur_unit);

}  // namespace exim
}  // namespace kdash
