#pragma once
// NvsStorage: IStorage over ESP32 NVS (namespace "kdash").
// Device-only; host builds never compile this file.
#include "../../include/IStorage.h"

namespace kdash {

class NvsStorage : public IStorage {
 public:
  bool begin();  // nvs_flash_init once; false on fatal init error
  bool read(const char* key, uint8_t* out, size_t out_len,
            size_t& out_actual) override;
  bool write_atomic(const char* key, const uint8_t* data,
                    size_t len) override;
  bool erase(const char* key) override;
};

}  // namespace kdash
