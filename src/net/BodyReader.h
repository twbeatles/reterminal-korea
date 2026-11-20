#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../../include/IHttpClient.h"

namespace kdash { namespace net {
// Port over the raw HTTP body stream; production and fault tests use the same loop.
class IBodyStream {
 public:
  virtual ~IBodyStream() = default;
  virtual int available() = 0;
  virtual bool connected() = 0;
  virtual int read(uint8_t* out, size_t count) = 0;
  virtual uint32_t milliseconds() = 0;
  virtual void idle() = 0;
};
struct BodyReadResult {
  HttpError error = HttpError::Transport;
  size_t size = 0;
};
// out must have capacity+1 bytes. Only a complete body returns None.
BodyReadResult ReadBody(IBodyStream& stream, int declared_length, char* out,
                        size_t capacity, uint32_t timeout_ms, bool chunked = false);
} }  // namespace kdash::net
