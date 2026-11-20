#pragma once
// Provider business-code -> Outcome mapping (docs/03 "HTTP 200 != API 성공").
// Code tables (source: data.go.kr OpenAPI 공통 오류코드 / EXIM 환율 API 안내 /
// KIS open-trading-api 샘플). Codes not listed map to ProviderError — never
// to Ok and never to a "holiday/no data" path by default.
#include "../domain/Types.h"

namespace kdash {
namespace codes {

// data.go.kr gateway + KMA/AirKorea/FSC header.resultCode or XML
// cmmMsgHeader.returnReasonCode ("00" ok, "03" NODATA, 20/21/30/31/32/33
// auth/key, 22 quota). nullptr/"" => ProviderError (schema failure).
Outcome DataGoKr(const char* result_code);

// EXIM per-row RESULT: 1 ok, 2 data-code error, 3 auth-code error,
// 4 daily limit. Unknown => ProviderError (실키 대조 전 보수적 분류).
Outcome Exim(int result);

// KIS: HTTP status + rt_cd ("0" ok) + msg_cd (EGW00123/EGW00121 token
// expired/invalid, EGW00201 초당 거래건수 초과, EGW00133 토큰 발급 1분 제한).
Outcome Kis(int http_status, const char* rt_cd, const char* msg_cd);

// True when the KIS token must be re-issued (expired/invalid).
bool KisTokenInvalid(int http_status, const char* msg_cd);

// Content sniffing: data.go.kr returns XML error bodies with HTTP 200.
bool LooksLikeXml(const char* body);

// Extracts the text of the first <tag>...</tag> into out (bounded). Used only
// for the tiny data.go.kr XML error envelope, never for data parsing.
bool XmlTagText(const char* body, const char* tag, char* out, unsigned out_cap);

}  // namespace codes
}  // namespace kdash
