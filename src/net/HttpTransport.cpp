// ESP32 HTTPS adapter — applies NetPolicy, never bypasses TLS.
// Builds only with the Arduino-ESP32 toolchain (`pio run -e reterminal_e1001`).
// Live behaviour (TLS handshake, timeouts) is verified on hardware only.
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include "HttpTransport.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <string.h>

#include "../../include/Config.h"
#include "NetPolicy.h"
#include "BodyReader.h"

namespace kdash {
namespace net {
namespace {

HttpError MapError(int code) {
  switch (code) {
    case HTTPC_ERROR_READ_TIMEOUT:
      return HttpError::Timeout;
    case HTTPC_ERROR_CONNECTION_REFUSED:
    case HTTPC_ERROR_NOT_CONNECTED:
      // Connection/TLS failures are indistinguishable at this API level; the
      // most common cause with a pinned CA is a handshake/cert/time failure.
      return HttpError::Tls;
    default:
      return HttpError::Transport;
  }
}

// "Name: value" -> addHeader(Name, value). Malformed entries are skipped.
void AddHeader(HTTPClient& http, const char* line) {
  const char* colon = line ? strchr(line, ':') : nullptr;
  if (!colon || colon == line) return;
  String name(line);
  name.remove(colon - line);
  const char* v = colon + 1;
  while (*v == ' ') v++;
  http.addHeader(name, String(v));
}

class EspBodyStream : public IBodyStream {
 public:
  explicit EspBodyStream(NetworkClient* client) : client_(client) {}
  int available() override { return client_ ? client_->available() : 0; }
  bool connected() override { return client_ && client_->connected(); }
  int read(uint8_t* out, size_t count) override {
    return client_ ? client_->read(out, count) : -1;
  }
  uint32_t milliseconds() override { return millis(); }
  void idle() override { delay(2); }
 private:
  NetworkClient* client_;
};

}  // namespace

EspHttpClient::EspHttpClient() {
  const size_t cap = (size_t)kBodyMaxBytes + 1;
  body_ = (char*)(psramFound() ? ps_malloc(cap) : malloc(cap));
}

EspHttpClient::~EspHttpClient() { free(body_); }

HttpResponse EspHttpClient::get(const char* url, const char* const* headers,
                                size_t header_count, const char* ca_bundle) {
  return Request("GET", url, headers, header_count, nullptr, ca_bundle);
}

HttpResponse EspHttpClient::post(const char* url, const char* const* headers,
                                 size_t header_count, const char* body,
                                 const char* ca_bundle) {
  return Request("POST", url, headers, header_count, body ? body : "",
                 ca_bundle);
}

HttpResponse EspHttpClient::Request(const char* method, const char* url,
                                    const char* const* headers,
                                    size_t header_count, const char* body,
                                    const char* ca_bundle) {
  HttpResponse r;
  r.error = HttpError::Transport;
  content_type_[0] = retry_after_[0] = '\0';
  if (!body_ || !ca_bundle || !*ca_bundle || !HostAllowed(url)) return r;

  const int32_t left = bounded_ ? (int32_t)(deadline_ - millis()) : kHttpTimeoutMs;
  if (left <= 0) { r.error = HttpError::Timeout; return r; }
  const uint32_t timeout = (uint32_t)left < (uint32_t)kHttpTimeoutMs
                               ? (uint32_t)left : (uint32_t)kHttpTimeoutMs;

  NetworkClientSecure client;
  client.setCACert(ca_bundle);  // pinned roots; verification mandatory
  client.setTimeout(timeout / 1000 + 1);
  client.setHandshakeTimeout(timeout / 1000 + 1);
  HTTPClient http;
  http.setReuse(false);
  http.useHTTP10(true);  // Servers may still respond with chunked framing.
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setConnectTimeout(timeout);
  http.setTimeout(timeout);
  if (!http.begin(client, String(url))) return r;
  static const char* kCollect[] = {"Content-Type", "Retry-After", "Transfer-Encoding", "Content-Encoding"};
  http.collectHeaders(kCollect, 4);
  for (size_t i = 0; headers && i < header_count; i++) AddHeader(http, headers[i]);

  const int code =
      body ? http.sendRequest(method, String(body)) : http.sendRequest(method);
  if (code <= 0) {
    r.error = MapError(code);
    http.end();
    return r;
  }
  r.status = code;
  strncpy(content_type_, http.header("Content-Type").c_str(),
          sizeof(content_type_) - 1);
  strncpy(retry_after_, http.header("Retry-After").c_str(),
          sizeof(retry_after_) - 1);
  r.content_type = content_type_;
  r.retry_after = retry_after_[0] ? retry_after_ : nullptr;

  const int declared = http.getSize();  // -1 when unknown
  String transfer = http.header("Transfer-Encoding"); transfer.trim();
  String encoding = http.header("Content-Encoding"); encoding.trim();
  const bool chunked = transfer.equalsIgnoreCase("chunked");
  if ((transfer.length() && !chunked && !transfer.equalsIgnoreCase("identity")) ||
      (encoding.length() && !encoding.equalsIgnoreCase("identity"))) {
    http.end(); return r;  // Unsupported coding: never hand compressed/framed bytes to JSON.
  }
  if (declared > kBodyMaxBytes) {
    r.error = HttpError::TooLarge;  // NET-01: abort before reading
    http.end();
    return r;
  }
  EspBodyStream stream(http.getStreamPtr());
  const int32_t body_left = bounded_ ? (int32_t)(deadline_ - millis()) : timeout;
  if (body_left <= 0) { r.error = HttpError::Timeout; http.end(); return r; }
  const BodyReadResult body_result = ReadBody(stream, declared, body_,
                                             kBodyMaxBytes, (uint32_t)body_left < timeout
                                                             ? (uint32_t)body_left : timeout,
                                             chunked);
  http.end();
  r.error = body_result.error;
  if (r.error != HttpError::None) return r;
  r.body = body_;
  r.body_len = body_result.size;
  return r;
}

}  // namespace net
}  // namespace kdash
#endif
