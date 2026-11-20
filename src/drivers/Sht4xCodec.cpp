#include "Sht4xCodec.h"

namespace kdash {
namespace sht {

uint8_t Crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

bool Decode(const uint8_t bytes[6], float& temp_c, float& humidity_pct) {
  if (!bytes) return false;
  if (Crc8(bytes, 2) != bytes[2]) return false;
  if (Crc8(bytes + 3, 2) != bytes[5]) return false;
  uint16_t raw_t = (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
  uint16_t raw_rh = (uint16_t)(((uint16_t)bytes[3] << 8) | bytes[4]);
  temp_c = -45.0f + 175.0f * (float)raw_t / 65535.0f;
  humidity_pct = -6.0f + 125.0f * (float)raw_rh / 65535.0f;
  if (humidity_pct < 0) humidity_pct = 0;
  if (humidity_pct > 100) humidity_pct = 100;
  return true;
}

}  // namespace sht
}  // namespace kdash
