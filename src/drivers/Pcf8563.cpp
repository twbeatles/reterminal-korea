#include "Pcf8563.h"

namespace kdash {
namespace pcf {

bool EncBcd(int v, uint8_t& out) {
  if (v < 0 || v > 99) return false;
  out = (uint8_t)(((v / 10) << 4) | (v % 10));
  return true;
}

bool DecBcd(uint8_t b, int& out) {
  int hi = (b >> 4) & 0xF, lo = b & 0xF;
  if (hi > 9 || lo > 9) return false;
  out = hi * 10 + lo;
  return true;
}

bool EncodeRegs(int year, int mon, int day, int hour, int min, int sec,
                int wday, Regs& out) {
  if (year < 2000 || year > 2099) return false;
  if (mon < 1 || mon > 12 || day < 1 || day > 31) return false;
  if (hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59)
    return false;
  if (wday < 0 || wday > 6) return false;
  uint8_t b = 0;
  if (!EncBcd(sec, b)) return false;
  out.sec = (uint8_t)(b & 0x7F);  // VL=0 on write
  if (!EncBcd(min, b)) return false;
  out.min = (uint8_t)(b & 0x7F);
  if (!EncBcd(hour, b)) return false;
  out.hour = (uint8_t)(b & 0x3F);
  if (!EncBcd(day, b)) return false;
  out.day = (uint8_t)(b & 0x3F);
  out.wday = (uint8_t)(wday & 0x07);
  if (!EncBcd(mon, b)) return false;
  out.month = (uint8_t)(b & 0x1F);  // century bit 7 = 0 for 2000s
  if (!EncBcd(year % 100, b)) return false;
  out.year = b;
  return true;
}

bool DecodeRegs(const Regs& r, bool& vl, int& year, int& mon, int& day,
                int& hour, int& min, int& sec, int& wday) {
  vl = (r.sec & 0x80) != 0;
  int v = 0;
  if (!DecBcd((uint8_t)(r.sec & 0x7F), v) || v > 59) return false;
  sec = v;
  if (!DecBcd((uint8_t)(r.min & 0x7F), v) || v > 59) return false;
  min = v;
  if (!DecBcd((uint8_t)(r.hour & 0x3F), v) || v > 23) return false;
  hour = v;
  if (!DecBcd((uint8_t)(r.day & 0x3F), v) || v < 1 || v > 31) return false;
  day = v;
  wday = r.wday & 0x07;
  if (wday > 6) return false;
  bool century = (r.month & 0x80) != 0;
  if (!DecBcd((uint8_t)(r.month & 0x1F), v) || v < 1 || v > 12) return false;
  mon = v;
  if (!DecBcd(r.year, v)) return false;
  year = (century ? 1900 : 2000) + v;  // C=1 -> 1900s, C=0 -> 2000s
  if (year < 1900 || year > 2099) return false;
  return true;
}

}  // namespace pcf
}  // namespace kdash
