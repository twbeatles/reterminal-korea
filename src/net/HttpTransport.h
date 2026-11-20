#pragma once
// ESP32 HTTPS transport implementing IHttpClient (device-only).
// Applies NetPolicy: allowlisted host+port only, pinned root CAs
// (CaBundle.h), no redirects, one attempt, kHttpTimeoutMs, kBodyMaxBytes cap.
// setInsecure() is FORBIDDEN (TLS-01, SEC-01 scan).
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include "../../include/IHttpClient.h"
#include <stdint.h>

namespace kdash {
namespace net {

class EspHttpClient : public IHttpClient {
 public:
  EspHttpClient();
  ~EspHttpClient() override;
  HttpResponse get(const char* url, const char* const* headers,
                   size_t header_count, const char* ca_bundle) override;
  HttpResponse post(const char* url, const char* const* headers,
                    size_t header_count, const char* body,
                    const char* ca_bundle) override;
  bool ready() const override { return body_ != nullptr; }
  void SetDeadline(uint32_t deadline_ms) override { deadline_ = deadline_ms; bounded_ = true; }

 private:
  HttpResponse Request(const char* method, const char* url,
                       const char* const* headers, size_t header_count,
                       const char* body, const char* ca_bundle);
  char* body_ = nullptr;  // kBodyMaxBytes + 1, PSRAM when available
  char content_type_[96] = {0};
  char retry_after_[48] = {0};
  uint32_t deadline_ = 0;
  bool bounded_ = false;
};

}  // namespace net
}  // namespace kdash
#endif
