#pragma once
// Network policy enforcement (docs/04): one attempt per call, body cap,
// host+port allowlist, no redirects, Retry-After parsing, TLS-never-bypass.
// Portable policy core; the ESP32 adapter (HttpTransport.cpp) applies it to
// NetworkClientSecure with pinned root CAs. setInsecure() is FORBIDDEN
// there (TLS-01).
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace net {

struct AllowedHost {
  const char* host;
  uint16_t port;
};

// Verified 2026-11-02 with `openssl s_client` (chains complete, see
// src/net/CaBundle.h for the pinned roots).
inline constexpr AllowedHost kAllowHosts[] = {
    {"apis.data.go.kr", 443},               // KMA + AirKorea + FSC
    {"oapi.koreaexim.go.kr", 443},          // EXIM (신규 host; www 금지)
    {"openapi.koreainvestment.com", 9443},  // KIS 실전 REST + oauth2 (P5)
};

// https:// only, exact host (case-insensitive) AND port match, no userinfo
// ('@'), no backslashes, host chars [A-Za-z0-9.-] only (ISSUE-009).
bool HostAllowed(const char* url);
bool UsesHttps(const char* url);

// Retry-After: delta-seconds or HTTP-date (latter => -1, caller falls back to
// backoff; keeps this layer libc-timezone free). Returns seconds or -1.
int ParseRetryAfter(const char* value);

// Body budget: false => abort before parse (NET-01), no reset loop.
bool BodySizeAllowed(size_t bytes, size_t cap);

}  // namespace net
}  // namespace kdash
