#pragma once
// SHT4x codec (T08): CRC-8 (poly 0x31, init 0xFF) + 6-byte decode.
// T = -45 + 175*raw/65535, RH = -6 + 125*raw/65535 clamped to 0..100.
// Command 0xFD (high precision); ~10ms wait. Portable.
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace sht {

inline constexpr uint8_t kCmdMeasureHp = 0xFD;

uint8_t Crc8(const uint8_t* data, size_t len);

// Decodes 6 bytes [T_msb,T_lsb,crc,RH_msb,RH_lsb,crc]. False on CRC error.
bool Decode(const uint8_t bytes[6], float& temp_c, float& humidity_pct);

}  // namespace sht
}  // namespace kdash
