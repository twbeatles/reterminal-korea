#include "FinanceParsers.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../domain/Freshness.h"
#include "ResultCodes.h"

namespace kdash {
namespace fin {
namespace {

constexpr const char* kFscSource = "금융위원회";
constexpr const char* kKisSource = "한국투자증권";

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

bool ParseKrw(const char* text, int64_t& out) {
  out = 0;
  if (!text) return false;
  char buf[20];  // <= 18 digits => no int64 overflow
  size_t bl = 0;
  for (const char* p = text; *p; p++) {
    if (*p == ',' || *p == ' ' || *p == '\t') continue;
    if (bl + 1 >= sizeof(buf)) return false;
    buf[bl++] = *p;
  }
  buf[bl] = '\0';
  if (bl == 0 || buf[0] == '-') return false;
  for (size_t i = 0; i < bl; i++) {
    if (!isdigit((unsigned char)buf[i])) return false;
    out = out * 10 + (buf[i] - '0');
  }
  return out > 0;  // 0원 종가/현재가는 실값이 아님 (결측 취급)
}

void FillQuote(DataPoint<StockQuote>& q, const char* symbol,
               const char* market, int64_t price, QuoteKind kind,
               int64_t price_at, bool price_at_verified) {
  q.has_value = true;
  strncpy(q.value.symbol, symbol ? symbol : "", sizeof(q.value.symbol) - 1);
  strncpy(q.value.market, market ? market : "", sizeof(q.value.market) - 1);
  q.value.price_krw = price;
  q.value.kind = kind;
  q.value.price_at.epoch_utc = price_at;
  q.value.price_at.verified = price_at_verified;
  q.observed_at = q.value.price_at;
}

}  // namespace

FscOut ParseFsc(const FscRow* rows, size_t n, const char* result_code,
                bool body_is_xml, const char* symbol, const char* market,
                const FetchContext& ctx) {
  FscOut out;
  out.quote.source = kFscSource;
  out.quote.fetched_at = DeviceTs(ctx);
  out.outcome = codes::DataGoKr(result_code);
  if (body_is_xml && out.outcome == Outcome::Ok)
    out.outcome = Outcome::ProviderError;
  if (out.outcome != Outcome::Ok) {
    out.quote.validity = out.outcome == Outcome::NoData
                             ? Validity::Missing
                             : Validity::ProviderError;
    return out;
  }
  out.quote.validity = Validity::Missing;
  int64_t best_day = -1, best_price = 0;
  for (size_t i = 0; rows && i < n; i++) {
    if (rows[i].srtn_cd && symbol && !Eq(rows[i].srtn_cd, symbol)) continue;
    int64_t price = 0;
    if (!ParseKrw(rows[i].clpr, price)) continue;
    int64_t day = KstStrToUtc(rows[i].bas_dt, "0000");
    if (day < 0 || day <= best_day) continue;
    best_day = day;
    best_price = price;
  }
  if (best_day < 0) {
    out.outcome = Outcome::NoData;
    return out;
  }
  FillQuote(out.quote, symbol, market, best_price, QuoteKind::PriorClose,
            best_day, true);  // FSC-01: 거래일 기준, 실시간 오인 금지
  out.quote.validity = ClassifyFreshness(best_day, ctx.now_utc, kFscTtlSec,
                                         ctx.clock_verified);
  out.outcome = Outcome::Ok;
  return out;
}

bool ParseScaled2(const char* text, int64_t& out) {
  out = 0;
  if (!text) return false;
  int64_t whole = 0, frac = 0;
  int digits = 0, fdigits = 0, third = 0;
  bool dot = false;
  for (const char* p = text; *p; p++) {
    if (*p == ',' || *p == ' ') continue;
    if (*p == '.' && !dot) { dot = true; continue; }
    if (!isdigit((unsigned char)*p)) return false;
    if (!dot) {
      whole = whole * 10 + (*p - '0');
      if (++digits > 9) return false;
    } else if (fdigits < 2) {
      frac = frac * 10 + (*p - '0'); fdigits++;
    } else if (fdigits == 2 && !third) {
      third = *p - '0' + 1;  // remember the 3rd digit (+1 so 0 means unset)
    }
  }
  if (!digits && !fdigits) return false;
  if (fdigits == 1) frac *= 10;
  int64_t v = whole * 100 + frac + (third >= 6 ? 1 : 0);
  if (v <= 0) return false;
  out = v;
  return true;
}

KisIndexOut ParseKisIndex(const KisIndexBody& body, const char* name,
                          const FetchContext& ctx) {
  KisIndexOut out;
  out.quote.source = kKisSource;
  out.quote.fetched_at = DeviceTs(ctx);
  out.outcome = codes::Kis(body.http_status, body.rt_cd, body.msg_cd);
  out.token_invalid = codes::KisTokenInvalid(body.http_status, body.msg_cd);
  if (out.outcome != Outcome::Ok) {
    out.quote.validity = Validity::ProviderError;
    return out;
  }
  int64_t v = 0;
  if (!ParseScaled2(body.bstp_nmix_prpr, v)) {
    out.quote.validity = Validity::Missing;
    out.outcome = Outcome::NoData;
    return out;
  }
  out.quote.has_value = true;
  strncpy(out.quote.value.name, name ? name : "", sizeof(out.quote.value.name) - 1);
  out.quote.value.value_x100 = v;
  out.quote.value.at.epoch_utc = ctx.fetched_utc;
  out.quote.value.at.verified = ctx.clock_verified;
  out.quote.observed_at = out.quote.value.at;
  out.quote.validity = ctx.clock_verified ? Validity::Valid : Validity::Stale;
  int32_t bp = 0;
  if (ParseChangeBp(body.bstp_nmix_prdy_ctrt, body.prdy_vrss_sign, bp)) {
    out.quote.value.has_change = true;
    out.quote.value.change_bp = bp;
  }
  return out;
}

bool ParseChangeBp(const char* text, const char* sign_code, int32_t& out_bp) {
  out_bp = 0;
  if (!text || !*text) return false;
  const char* p = text;
  bool neg = false;
  if (*p == '-' || *p == '+') { neg = *p == '-'; p++; }
  if (!isdigit((unsigned char)*p)) return false;
  int64_t whole = 0;
  for (; isdigit((unsigned char)*p); p++) {
    whole = whole * 10 + (*p - '0');
    if (whole > 1000) return false;
  }
  int64_t frac = 0;  // two decimals, third digit rounds half up
  if (*p == '.') {
    p++;
    int digits = 0, third = 0;
    for (; isdigit((unsigned char)*p); p++, digits++) {
      if (digits < 2) frac = frac * 10 + (*p - '0');
      else if (digits == 2) third = *p - '0';
    }
    if (digits == 0) return false;
    if (digits == 1) frac *= 10;
    if (third >= 5) frac++;
  }
  if (*p) return false;
  int64_t bp = whole * 100 + frac;
  if (bp > 10000) return false;
  if (sign_code && sign_code[0] && !sign_code[1]) {
    const char c = sign_code[0];
    if (c == '4' || c == '5') neg = true;
    else if (c == '1' || c == '2') neg = false;
    else if (c == '3') { neg = false; }
  }
  out_bp = (int32_t)(neg ? -bp : bp);
  return true;
}

KisOut ParseKis(const KisQuoteBody& body, const char* symbol,
                const char* market, const FetchContext& ctx) {
  KisOut out;
  out.quote.source = kKisSource;
  out.quote.fetched_at = DeviceTs(ctx);
  out.outcome = codes::Kis(body.http_status, body.rt_cd, body.msg_cd);
  out.token_invalid = codes::KisTokenInvalid(body.http_status, body.msg_cd);
  if (out.outcome != Outcome::Ok) {
    out.quote.validity = Validity::ProviderError;
    return out;
  }
  int64_t price = 0;
  if (!ParseKrw(body.stck_prpr, price)) {
    out.quote.validity = Validity::Missing;
    out.outcome = Outcome::NoData;
    return out;
  }
  // REST 호출 시점 현재가: price_at == device receive time (its trust ==
  // device clock trust). Not a tick stream.
  FillQuote(out.quote, symbol, market, price, QuoteKind::IntradayQuote,
            ctx.fetched_utc, ctx.clock_verified);
  out.quote.validity = ctx.clock_verified ? Validity::Valid : Validity::Stale;
  // 전일 대비율은 선택 정보: 없거나 형식이 이상하면 표시하지 않을 뿐 현재가는 유지한다.
  int32_t bp = 0;
  if (ParseChangeBp(body.prdy_ctrt, body.prdy_vrss_sign, bp)) {
    out.quote.value.has_change = true;
    out.quote.value.change_bp = bp;
  }
  return out;
}

int64_t KisTokenExpiry(int http_status, const char* access_token,
                       long expires_in_sec, const FetchContext& ctx) {
  if (http_status != 200 || !access_token || strlen(access_token) < 16)
    return -1;
  if (expires_in_sec <= kKisTokenSafetyMarginSec) return -1;
  return ctx.fetched_utc + expires_in_sec - kKisTokenSafetyMarginSec;
}

bool FscDue(int64_t now_utc, int64_t last_success_utc) {
  KstBroken k = BreakKst(now_utc);
  if (k.weekday == 0 || k.weekday == 6) return false;
  int64_t boundary = KstToUtc(k.year, k.month, k.day, 13, 0);  // 당일 13:00 KST
  // 성공 이력이 없을 때만 13시 전에도 1회 허용: 조회는 14일 창의 최신 종가를 쓰므로
  // 직전 영업일 값이 온다(ADR 0017). 이력이 있으면 종전처럼 13시 이후에만 갱신.
  if (now_utc < boundary) return last_success_utc <= 0;
  return last_success_utc < boundary;
}

bool IsKrxOpen(int64_t epoch_utc) {
  KstBroken k = BreakKst(epoch_utc);
  if (k.weekday == 0 || k.weekday == 6) return false;
  int mins = k.hour * 60 + k.minute;
  return mins >= 9 * 60 && mins < 15 * 60 + 30;  // 09:00–15:30
}

bool IsValidSymbol(const char* s) {
  if (!s || strlen(s) != 6) return false;
  for (int i = 0; i < 6; i++) {
    if (!isdigit((unsigned char)s[i])) return false;
  }
  return true;
}

bool IsValidMarket(const char* m) {
  return Eq(m, "KRX") || Eq(m, "NXT") || Eq(m, "통합");
}

}  // namespace fin
}  // namespace kdash
