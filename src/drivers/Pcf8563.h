#pragma once
// PCF8563 RTC: BCD helpers + register encode/decode (portable) and
// a thin Wire adapter (device-only). I2C: SDA=19, SCL=20, addr 0x51.
// Time is kept as UTC (the official example uses local time —
// documented difference). VL (seconds bit7) => time untrusted.
#include <stdint.h>

namespace kdash {
namespace pcf {

inline constexpr uint8_t kAddr = 0x51;

// BCD conversion. DecValid rejects non-BCD nibbles (0x5A etc).
bool EncBcd(int v, uint8_t& out);  // 0..99
bool DecBcd(uint8_t b, int& out);

// 7 data registers 0x02..0x08 (sec..year) <-> epoch.
// Encode writes UTC broken-down time; century_bit carries bit7 of month
// (years 2000..2099). Decode validates ranges; VL sets *vl.
struct Regs {
  uint8_t sec = 0, min = 0, hour = 0, day = 0, wday = 0, month = 0, year = 0;
};
bool EncodeRegs(int year, int mon, int day, int hour, int min, int sec,
                int wday, Regs& out);
bool DecodeRegs(const Regs& r, bool& vl, int& year, int& mon, int& day,
                int& hour, int& min, int& sec, int& wday);
// Century (month bit7): 0 -> 2000s, 1 -> 1900s (official RTC_PCF8563.ino,
// PCF8563 datasheet). Years outside 1900..2099 are rejected.

}  // namespace pcf
}  // namespace kdash
