#pragma once
// Key-value storage abstraction (NVS on device, file/memory on host).
// Writes must be atomic (tmp+rename or dual-slot+CRC); secrets are opaque.
#include <stddef.h>
#include <stdint.h>

namespace kdash {

class IStorage {
 public:
  virtual ~IStorage() = default;
  virtual bool read(const char* key, uint8_t* out, size_t out_len,
                    size_t& out_actual) = 0;
  virtual bool write_atomic(const char* key, const uint8_t* data,
                            size_t len) = 0;
  virtual bool erase(const char* key) = 0;
};

class ISensor {
 public:
  virtual ~ISensor() = default;
  // Returns false on sensor failure (caller keeps last_known_good + badge).
  virtual bool read_indoor(float& temp_c, float& humidity_pct) = 0;
};

}  // namespace kdash
