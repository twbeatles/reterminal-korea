#pragma once
// Provider endpoint builders (docs/03). Exact documented URL shapes;
// API keys are NEVER logged. Encoding policy (03 공통 규정):
//  - data.go.kr serviceKey: NormalizeServiceKey() accepts BOTH forms the
//    portal issues — "Encoding" keys (already %XX-encoded) are inserted
//    verbatim, "Decoding" keys (raw base64 with + / =) are encoded exactly
//    once. Never double-encoded (G-6).
//  - stationName 등 한글 파라미터: EncodeOnce()로 정확히 한 번만 인코딩.
//    입력에 '%'가 있으면 이중 인코딩 의심으로 거부한다.
// AirKorea/FSC/KIS operation paths are docs/03 values not yet confirmed with
// an authenticated call (U-06/U-08/U-09) — kept in this one file so a portal
// change is a one-line fix.
#include <stddef.h>

namespace kdash {
namespace endpoints {

// Percent-encodes UTF-8 bytes except unreserved chars. Returns false when
// `in` contains '%' (double-encode guard) or output overflows.
bool EncodeOnce(const char* in, char* out, size_t out_cap);

// data.go.kr key in either portal form -> URL-safe form (see header note).
// false: empty, whitespace/control chars, malformed %XX, overflow.
bool NormalizeServiceKey(const char* key, char* out, size_t out_cap);

// KMA 초단기실황 / 단기예보.
bool KmaUltraUrl(const char* key, int base_date, int base_time, int nx, int ny,
                 int num_rows, char* out, size_t out_cap);
bool KmaVillageUrl(const char* key, int base_date, int base_time, int nx,
                   int ny, int num_rows, char* out, size_t out_cap);

// AirKorea 측정소별 실시간 — operation path 후보 (U-06 검증 전까지 Candidate).
bool AirKoreaCandidateUrl(const char* key, const char* station,
                          char* out, size_t out_cap);

// EXIM 고시환율. searchdate: YYYYMMDD. 구 host(www.) 사용 금지.
bool EximUrl(const char* authkey, int searchdate, char* out, size_t out_cap);

// FSC 주식시세 (getStockPriceInfo), rows for one 6-digit code with
// basDt >= begin_yyyymmdd (bounded window; parser picks the latest day).
bool FscPriceUrl(const char* key, const char* symbol, int begin_yyyymmdd,
                 char* out, size_t out_cap);

// KIS 실전 REST (docs/03 E). Market: "KRX"->J, "NXT"->NX, "통합"->UN.
inline constexpr const char* kKisBase = "https://openapi.koreainvestment.com:9443";
inline constexpr const char* kKisQuoteTrId = "FHKST01010100";
const char* KisMarketCode(const char* market);  // nullptr when unknown
// KIS 업종지수 현재가(inquire-index-price). code: "0001"=코스피. tr_id FHPUP02100000.
bool KisIndexUrl(const char* code, char* out, size_t out_cap);

bool KisPriceUrl(const char* symbol, const char* market, char* out,
                 size_t out_cap);
bool KisTokenUrl(char* out, size_t out_cap);
// {"grant_type":"client_credentials","appkey":"..","appsecret":".."}
// Keys must be printable ASCII without quotes/backslashes (else false).
bool KisTokenBody(const char* app_key, const char* app_secret, char* out,
                  size_t out_cap);

}  // namespace endpoints
}  // namespace kdash
