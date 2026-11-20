#pragma once
// HTTP client abstraction with an explicit error taxonomy.
// AGENTS.md: HTTPS certificate verification is mandatory — setInsecure() FORBIDDEN.

#include <stddef.h>
#include <stdint.h>

namespace kdash {

enum class HttpError {
  None = 0,
  Dns,
  Tls,        // incl. expired/mismatched cert and unverified-clock TLS failure
  Timeout,
  HttpStatus,  // non-2xx (carries status code)
  TooLarge,    // body exceeded kBodyMaxBytes
  Transport,   // connection reset etc.
};

struct HttpResponse {
  HttpError error = HttpError::None;
  int status = 0;  // HTTP status when known, else 0
  const char* content_type = nullptr;  // response Content-Type (may be null)
  const char* body = nullptr;          // NUL-terminated body (may be null)
  size_t body_len = 0;
  const char* retry_after = nullptr;  // raw Retry-After header value or null
};

class IHttpClient {
 public:
  virtual ~IHttpClient() = default;
  // Optional capabilities shared by real transport and deterministic host fakes.
  virtual bool ready() const { return true; }
  virtual void SetDeadline(uint32_t) {}
  // Exactly one attempt with kHttpTimeoutMs. No implicit retries here —
  // retry/backoff budgets live in the caller (Schedule/Coordinator).
  // `ca_bundle` selects the pinned root-CA set; must never be bypassed.
  virtual HttpResponse get(const char* url, const char* const* headers,
                           size_t header_count, const char* ca_bundle) = 0;
  virtual HttpResponse post(const char* url, const char* const* headers,
                            size_t header_count, const char* body,
                            const char* ca_bundle) = 0;
};

}  // namespace kdash
