#include "BodyReader.h"

namespace kdash { namespace net {
namespace {
// No body-dependent allocation. A single deadline covers framing and payload.
class ChunkReader {
 public:
  ChunkReader(IBodyStream& stream, uint32_t timeout)
      : s(stream), start(s.milliseconds()), limit(timeout) {}
  HttpError error = HttpError::Transport;
  bool Byte(uint8_t& out) { return Bytes(&out, 1); }
  bool Bytes(uint8_t* out, size_t count) {
    while (count) {
      if ((uint32_t)(s.milliseconds() - start) >= limit) {
        error = HttpError::Timeout; return false;
      }
      const int available = s.available();
      if (available <= 0) {
        if (!s.connected()) return false;
        s.idle(); continue;
      }
      const size_t want = (size_t)available < count ? (size_t)available : count;
      const int got = s.read(out, want);
      if (got < 0 || (size_t)got > want) return false;
      if (!got) { s.idle(); continue; }
      count -= (size_t)got; out += got;
    }
    return true;
  }
  bool CrLf() {
    uint8_t a, b;
    return Byte(a) && a == '\r' && Byte(b) && b == '\n';
  }
  bool Line(char* out, size_t cap) {
    size_t n = 0;
    uint8_t c;
    while (Byte(c)) {
      if (c == '\r') {
        if (!Byte(c) || c != '\n') return false;
        out[n] = '\0'; return true;
      }
      if ((c < 32 && c != '\t') || c > 126 || n + 1 >= cap) return false;
      out[n++] = (char)c;
    }
    return false;
  }
 private:
  IBodyStream& s;
  uint32_t start, limit;
};

BodyReadResult ReadChunks(IBodyStream& s, char* out, size_t cap, uint32_t timeout) {
  BodyReadResult result;
  if (!out) return result;
  out[0] = '\0';
  ChunkReader reader(s, timeout);
  char line[256];
  // Framing also has a total byte bound, preventing tiny-chunk/extension abuse.
  size_t framing = 0;
  for (;;) {
    if (!reader.Line(line, sizeof(line))) break;
    size_t len = 0;
    while (line[len]) ++len;
    framing += len + 2;
    if (framing > 8192) break;
    size_t chunk = 0, digits = 0;
    for (; line[digits] && line[digits] != ';'; ++digits) {
      const char c = line[digits];
      const int digit = c >= '0' && c <= '9' ? c - '0' :
                        c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                        c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (digit < 0) return result;
      if (chunk > (SIZE_MAX - (size_t)digit) / 16) {
        result.error = HttpError::TooLarge; return result;
      }
      chunk = chunk * 16 + (size_t)digit;
    }
    if (!digits) break;
    if (chunk > cap - result.size) { result.error = HttpError::TooLarge; return result; }
    if (!chunk) {
      // Consume bounded trailers through their terminating blank line. They do
      // not override status/headers already used for authorization or parsing.
      for (;;) {
        if (!reader.Line(line, sizeof(line))) goto incomplete;
        size_t n = 0;
        bool colon = false;
        for (; line[n]; ++n) if (n && line[n] == ':') colon = true;
        framing += n + 2;
        if (framing > 8192) goto incomplete;
        if (!n) { result.error = HttpError::None; return result; }
        if (!colon || line[0] == ' ' || line[0] == '\t') goto incomplete;
      }
    }
    if (!reader.Bytes((uint8_t*)out + result.size, chunk)) break;
    result.size += chunk;
    out[result.size] = '\0';
    framing += 2;
    if (framing > 8192 || !reader.CrLf()) break;
  }
incomplete:
  // A partial chunk must never be exposed as a successful JSON body.
  out[result.size] = '\0';
  result.error = reader.error;
  return result;
}
}  // namespace

BodyReadResult ReadBody(IBodyStream& s, int declared, char* out, size_t cap,
                        uint32_t timeout, bool chunked) {
  if (chunked) return ReadChunks(s, out, cap, timeout);
  BodyReadResult r;
  if (!out || declared < -1) return r;
  out[0] = '\0';
  if (declared >= 0 && (size_t)declared > cap) {
    r.error = HttpError::TooLarge;
    return r;
  }
  const uint32_t start = s.milliseconds();
  for (;;) {
    if (declared >= 0 && r.size == (size_t)declared) {
      r.error = HttpError::None;
      break;
    }
    const int available = s.available();
    if (available <= 0 && !s.connected()) {
      r.error = declared < 0 ? HttpError::None : HttpError::Transport;
      break;
    }
    if ((uint32_t)(s.milliseconds() - start) >= timeout) {
      r.error = HttpError::Timeout;
      break;
    }
    if (available <= 0) { s.idle(); continue; }
    if (r.size == cap) { r.error = HttpError::TooLarge; break; }
    size_t want = cap - r.size;
    if ((size_t)available < want) want = (size_t)available;
    if (declared >= 0 && (size_t)declared - r.size < want)
      want = (size_t)declared - r.size;
    const int got = s.read((uint8_t*)out + r.size, want);
    if (got < 0 || (size_t)got > want) { r.error = HttpError::Transport; break; }
    r.size += (size_t)got;
    if (got == 0) s.idle();
  }
  out[r.size] = '\0';
  return r;
}
} }  // namespace kdash::net
