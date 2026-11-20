#pragma once
// Codec: explicit little-endian field writer/reader with bounds checks.
// Portable. No memcpy struct dumps (padding/compiler differences).
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace codec {

class Writer {
 public:
  Writer(uint8_t* buf, size_t cap) : buf_(buf), cap_(cap), pos_(0) {}
  bool u8(uint8_t v);
  bool u16(uint16_t v);
  bool u32(uint32_t v);
  bool i32(int32_t v);
  bool i64(int64_t v);
  bool bytes(const uint8_t* data, size_t len);
  bool str(const char* s, size_t max_len);  // u16 len + bytes, no NUL
  size_t pos() const { return pos_; }
  bool ok() const { return ok_; }

 private:
  uint8_t* buf_;
  size_t cap_, pos_;
  bool ok_ = true;
};

class Reader {
 public:
  Reader(const uint8_t* buf, size_t len) : buf_(buf), len_(len), pos_(0) {}
  bool u8(uint8_t& v);
  bool u16(uint16_t& v);
  bool u32(uint32_t& v);
  bool i32(int32_t& v);
  bool i64(int64_t& v);
  bool bytes(const uint8_t*& out, size_t len);
  // Reads u16 len + bytes into out (NUL-terminated). Fails when len > max_len
  // or buffer ends early. out_cap must be max_len+1 or larger.
  bool str(char* out, size_t out_cap, size_t max_len);
  size_t pos() const { return pos_; }
  size_t remaining() const { return pos_ <= len_ ? len_ - pos_ : 0; }
  bool ok() const { return ok_; }
  bool done() const { return ok_ && pos_ == len_; }

 private:
  const uint8_t* buf_;
  size_t len_, pos_;
  bool ok_ = true;
};

}  // namespace codec
}  // namespace kdash
