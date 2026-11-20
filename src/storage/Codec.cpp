#include "Codec.h"

#include <string.h>

namespace kdash {
namespace codec {

bool Writer::u8(uint8_t v) {
  if (!ok_ || pos_ + 1 > cap_) {
    ok_ = false;
    return false;
  }
  buf_[pos_++] = v;
  return true;
}
bool Writer::u16(uint16_t v) {
  if (!ok_ || pos_ + 2 > cap_) {
    ok_ = false;
    return false;
  }
  buf_[pos_++] = (uint8_t)(v & 0xFF);
  buf_[pos_++] = (uint8_t)((v >> 8) & 0xFF);
  return true;
}
bool Writer::u32(uint32_t v) {
  for (int i = 0; i < 4; i++) {
    if (!u8((uint8_t)((v >> (8 * i)) & 0xFF))) return false;
  }
  return true;
}
bool Writer::i32(int32_t v) { return u32((uint32_t)v); }
bool Writer::i64(int64_t v) {
  uint64_t u = (uint64_t)v;
  for (int i = 0; i < 8; i++) {
    if (!u8((uint8_t)((u >> (8 * i)) & 0xFF))) return false;
  }
  return true;
}
bool Writer::bytes(const uint8_t* data, size_t len) {
  if (!ok_ || pos_ + len > cap_) {
    ok_ = false;
    return false;
  }
  if (len) memcpy(buf_ + pos_, data, len);
  pos_ += len;
  return true;
}
bool Writer::str(const char* s, size_t max_len) {
  if (!s) s = "";
  size_t n = strlen(s);
  if (n > max_len || n > 0xFFFF) {
    ok_ = false;
    return false;
  }
  return u16((uint16_t)n) && bytes((const uint8_t*)s, n);
}

bool Reader::u8(uint8_t& v) {
  if (!ok_ || pos_ + 1 > len_) {
    ok_ = false;
    return false;
  }
  v = buf_[pos_++];
  return true;
}
bool Reader::u16(uint16_t& v) {
  uint8_t a, b;
  if (!u8(a) || !u8(b)) return false;
  v = (uint16_t)(a | ((uint16_t)b << 8));
  return true;
}
bool Reader::u32(uint32_t& v) {
  v = 0;
  for (int i = 0; i < 4; i++) {
    uint8_t b;
    if (!u8(b)) return false;
    v |= ((uint32_t)b << (8 * i));
  }
  return true;
}
bool Reader::i32(int32_t& v) {
  uint32_t u;
  if (!u32(u)) return false;
  v = (int32_t)u;
  return true;
}
bool Reader::i64(int64_t& v) {
  uint64_t u = 0;
  for (int i = 0; i < 8; i++) {
    uint8_t b;
    if (!u8(b)) return false;
    u |= ((uint64_t)b << (8 * i));
  }
  v = (int64_t)u;
  return true;
}
bool Reader::bytes(const uint8_t*& out, size_t len) {
  if (!ok_ || pos_ + len > len_) {
    ok_ = false;
    out = nullptr;
    return false;
  }
  out = buf_ + pos_;
  pos_ += len;
  return true;
}
bool Reader::str(char* out, size_t out_cap, size_t max_len) {
  uint16_t n = 0;
  if (!u16(n)) return false;
  if (n > max_len || pos_ + n > len_) {
    ok_ = false;
    return false;
  }
  if (!out || out_cap < (size_t)n + 1) {
    ok_ = false;
    return false;
  }
  if (n) memcpy(out, buf_ + pos_, n);
  out[n] = '\0';
  pos_ += n;
  return true;
}

}  // namespace codec
}  // namespace kdash
