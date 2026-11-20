#pragma once
// Domain core: Validity / QuoteKind / Timestamp / DataPoint.
// Portable C++17, no Arduino dependency — unit-testable on host and device.

#include <stdint.h>

namespace kdash {

// AGENTS.md: NaN / 0 / "-" must never be confused. Absence is explicit.
enum class Validity : uint8_t {
  Valid = 0,        // fresh value inside its TTL
  Missing,          // provider answered, field absent or "-" (never 0)
  ProviderError,    // transport/HTTP/schema/business-code failure
  Stale,            // value kept past TTL, shown with age badge
  NotConfigured,    // key/location/watchlist missing -> '설정 필요'
};

// Provider-level outcome of one response (separate from per-field Validity).
// The scheduler uses it: AuthError => no automatic retry until settings
// change (docs/04), QuotaExceeded/RateLimited => long backoff, NoData =>
// provider answered "nothing yet" (e.g. 미발표 회차, holiday).
enum class Outcome : uint8_t {
  Ok = 0,
  NoData,
  AuthError,
  QuotaExceeded,
  RateLimited,
  ProviderError,
};

// AGENTS.md: the time nature of every quote must be explicit in model AND UI.
enum class QuoteKind : uint8_t {
  Unknown = 0,
  OfficialDailyFx,  // EXIM 고시환율 (daily fixing, NOT live)
  PriorClose,       // FSC 전일 종가 (as of 거래일, shown after 13:00 next day)
  IntradayQuote,    // KIS REST snapshot at call time (NOT a tick stream)
};

struct Timestamp {
  int64_t epoch_utc = 0;
  // Source times (observed_at) are verified when parsed from the provider's
  // own fields. Device times (fetched_at) are verified only when the device
  // clock is (FetchContext::clock_verified) — never hard-coded true.
  bool verified = false;
};

// Everything a parser needs to know about "now" — passed explicitly so no
// parser invents clock trust (ISSUE-008).
struct FetchContext {
  int64_t fetched_utc = 0;     // device clock at receive time
  int64_t now_utc = 0;         // device clock for freshness decisions
  bool clock_verified = false;  // IClock::is_verified()
};

template <typename T>
struct DataPoint {
  bool has_value = false;
  T value{};
  Validity validity = Validity::NotConfigured;
  Timestamp observed_at{};  // source's own observation/price reference time
  Timestamp fetched_at{};   // when this device received it
  const char* source = "";  // static literal only, e.g. "기상청"
};

struct FxRate {
  char currency_unit[16] = {0};  // "USD" or "JPY(100)" — unit is part of value
  int64_t won_scaled = 0;  // fixed-point: real_rate = won_scaled / 10^scale
  int32_t scale = 2;
  QuoteKind quote_kind = QuoteKind::OfficialDailyFx;
};

struct StockQuote {
  char symbol[8] = {0};
  char market[16] = {0};  // "KRX"/"NXT"/"통합"(UTF-8 6 bytes)
  int64_t price_krw = 0;
  QuoteKind kind = QuoteKind::PriorClose;
  Timestamp price_at{};
  bool has_change = false;  // 전일 대비율을 원본(KIS prdy_ctrt)에서 받았을 때만 true
  int32_t change_bp = 0;    // 전일 대비율, 0.01% 단위(123 = +1.23%)
};

// 지수(KOSPI 등): 값은 소수 2자리를 x100 정수로 보관해 부동소수 오차를 피한다.
struct IndexQuote {
  char name[12] = {0};      // "KOSPI"
  int64_t value_x100 = 0;   // 5371.68 -> 537168
  bool has_change = false;
  int32_t change_bp = 0;    // 전일 대비율, 0.01% 단위
  Timestamp at{};           // 수신 시각(REST 스냅샷, 체결 스트림 아님)
};

inline Timestamp DeviceTs(const FetchContext& c) {
  Timestamp t;
  t.epoch_utc = c.fetched_utc;
  t.verified = c.clock_verified;
  return t;
}

inline Timestamp SourceTs(int64_t epoch_utc) {
  Timestamp t;
  t.epoch_utc = epoch_utc;
  t.verified = epoch_utc > 0;
  return t;
}

const char* ToString(Outcome o);
const char* ToString(Validity v);

}  // namespace kdash
