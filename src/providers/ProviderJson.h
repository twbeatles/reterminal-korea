#pragma once
// Raw HTTP body -> provider parser input (ArduinoJson 7, filtered).
// Portable: builds on ESP32 and on host (native tests feed fixture bodies).
// Handles the real-world response shapes documented in docs/03 + 2026-11
// probes of the published samples:
//  - data.go.kr envelope {"response":{"header":{..},"body":{"items":..}}};
//    `items.item` may be an array OR a single object; AirKorea uses
//    `body.items` as an array directly.
//  - HTTP 200 with an XML error body (<returnReasonCode>30</...>) => the
//    code is extracted so auth/quota errors surface as such (KMA-02).
//  - HTML/maintenance/garbage body => ProviderError, never Ok.
//  - Numbers or strings for the same field (obsrValue 21.5 vs "21.5").
//  - EXIM top-level array with lowercase or uppercase keys, RESULT number or
//    string.
//  - KIS {"rt_cd":"0","msg_cd":..,"output":{..}} and token responses.
// Output structs own no pointers into the body (body may be freed after).
#include <stddef.h>
#include <stdint.h>

#include "AirKoreaParser.h"
#include "EximParser.h"
#include "FinanceParsers.h"
#include "KmaParser.h"

namespace kdash {
namespace pj {

kma::NowOut KmaUltraNow(const char* body, size_t len, int base_yyyymmdd,
                        int base_hhmm, const FetchContext& ctx,
                        int expected_nx = -1, int expected_ny = -1);
kma::ForecastOut KmaVillage(const char* body, size_t len, int base_yyyymmdd,
                            int base_hhmm, const FetchContext& ctx,
                            int expected_nx = -1, int expected_ny = -1);
air::AirOut AirKorea(const char* body, size_t len, const char* station,
                     const FetchContext& ctx);
exim::EximOut Exim(const char* body, size_t len, int searchdate_yyyymmdd,
                   const FetchContext& ctx);
fin::FscOut Fsc(const char* body, size_t len, const char* symbol,
                const char* market, const FetchContext& ctx);
fin::KisIndexOut KisIndex(int http_status, const char* body, size_t len,
                          const char* name, const FetchContext& ctx);

fin::KisOut Kis(int http_status, const char* body, size_t len,
                const char* symbol, const char* market,
                const FetchContext& ctx);

// /oauth2/tokenP. On success copies the token into token_out (caller-owned,
// never logged) and returns the expiry (UTC); otherwise returns -1 and sets
// outcome (RateLimited for EGW00133 "1분당 1회", AuthError for bad keys).
int64_t KisToken(int http_status, const char* body, size_t len,
                 const FetchContext& ctx, char* token_out, size_t token_cap,
                 Outcome& outcome);

}  // namespace pj
}  // namespace kdash
