#pragma once
// P5 금융 (읽기 전용) — FSC 전일 종가 + KIS 현재가 스냅샷.
// READ ONLY: 주문/체결/계좌/잔고 API와 그 코드는 어떤 파일에도 import 금지.
// Endpoint shapes live in src/net/Endpoints.* (docs/03 D/E examples, U-08/
// U-09: 실키 대조 전 출발값).
#include <stddef.h>

#include "../domain/Types.h"

namespace kdash {
namespace fin {

// --- FSC (금융위원회_주식시세정보 getStockPriceInfo) ---------------------
// 다음 영업일 13시 이후 제공. 장중 가격으로 표시 금지 (FSC-01).
struct FscRow {
  const char* bas_dt = nullptr;   // 기준 거래일 "YYYYMMDD"
  const char* srtn_cd = nullptr;  // 단축코드 "005930" (null when absent)
  const char* clpr = nullptr;     // 종가 ("12,345" 허용)
};

struct FscOut {
  DataPoint<StockQuote> quote;  // kind == PriorClose, price_at == 거래일
  Outcome outcome = Outcome::ProviderError;
};

inline constexpr int64_t kFscTtlSec = 5 * 24 * 3600;  // 연휴 포함 최근 거래일

// result_code: response.header.resultCode. Picks the row matching `symbol`
// with the latest bas_dt (never "first row wins").
FscOut ParseFsc(const FscRow* rows, size_t n, const char* result_code,
                bool body_is_xml, const char* symbol, const char* market,
                const FetchContext& ctx);

// --- KIS (inquire-price, REST 현재가 스냅샷, 체결 스트림 아님) ---------------
struct KisQuoteBody {
  int http_status = 0;
  const char* rt_cd = nullptr;      // "0" success
  const char* msg_cd = nullptr;     // e.g. "MCA00000", "EGW00123"
  const char* stck_prpr = nullptr;  // output.stck_prpr 현재가
  const char* prdy_ctrt = nullptr;  // output.prdy_ctrt 전일 대비율(%)
  const char* prdy_vrss_sign = nullptr;  // 1상한 2상승 3보합 4하한 5하락
};

// "1.23" / "-0.51" / "+2" -> basis points. 숫자 형식이 아니거나 |값| > 100%면 false.
// sign_code(1,2 상승 / 3 보합 / 4,5 하락)가 있으면 부호는 그것을 따른다(크기만 문자열에서).
bool ParseChangeBp(const char* text, const char* sign_code, int32_t& out_bp);

// 소수 2자리 이하의 양수 문자열(쉼표 허용)을 x100 정수로. 0 이하·비숫자는 false.
bool ParseScaled2(const char* text, int64_t& out_x100);

struct KisIndexBody {
  int http_status = 0;
  const char* rt_cd = nullptr;
  const char* msg_cd = nullptr;
  const char* bstp_nmix_prpr = nullptr;       // output.bstp_nmix_prpr 현재지수
  const char* bstp_nmix_prdy_ctrt = nullptr;  // 전일 대비율(%)
  const char* prdy_vrss_sign = nullptr;       // 1상한 2상승 3보합 4하한 5하락
};

struct KisIndexOut {
  DataPoint<IndexQuote> quote;
  bool token_invalid = false;
  Outcome outcome = Outcome::ProviderError;
};

KisIndexOut ParseKisIndex(const KisIndexBody& body, const char* name,
                          const FetchContext& ctx);

struct KisOut {
  DataPoint<StockQuote> quote;  // kind == IntradayQuote, price_at == 수신시각
  bool token_invalid = false;   // 재발급 1회만 (KIS-01)
  Outcome outcome = Outcome::ProviderError;
};

inline constexpr int64_t kKisTtlSec = 30 * 60;

KisOut ParseKis(const KisQuoteBody& body, const char* symbol,
                const char* market, const FetchContext& ctx);

// Token response (/oauth2/tokenP). Token text stays in caller-owned memory
// and is NEVER logged. Returns expiry UTC (fetched + expires_in - margin) or
// -1 when the body is not a usable token.
inline constexpr int64_t kKisTokenSafetyMarginSec = 10 * 60;
int64_t KisTokenExpiry(int http_status, const char* access_token,
                       long expires_in_sec, const FetchContext& ctx);

// KRX 정규장 09:00–15:30 KST, 월–금 (공휴일 달력은 별도 계층; 단순 주말만
// 여기서 제외하고 실제 휴장 표시는 Calendar에서 판정).
bool IsKrxOpen(int64_t epoch_utc);

// FSC fetch gate: 평일 13:00 KST 이후 1회 (출발값, U-09 실문서 대조 전).
// Calendar-free window primitive: public holidays are NOT checked here.
// Production gating must go through the calendar layer (Coordinator
// FscWindow/NextFscWindow); calling FscDue directly skips holiday handling.
bool FscDue(int64_t now_utc, int64_t last_success_utc);

// Watchlist: 6자리 숫자 + 시장 whitelist ("KRX","NXT","통합").
bool IsValidSymbol(const char* symbol);
bool IsValidMarket(const char* market);

}  // namespace fin
}  // namespace kdash
