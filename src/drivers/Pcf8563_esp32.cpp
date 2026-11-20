#include "Pcf8563_esp32.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Wire.h>

#include "../domain/Freshness.h"
#include "Pcf8563.h"

namespace kdash {
namespace pcf {
namespace {
constexpr int kSdaPin = 19;
constexpr int kSclPin = 20;
constexpr uint8_t kRegCtrl1 = 0x00;
constexpr uint8_t kRegCtrl2 = 0x01;
constexpr uint8_t kRegClkout = 0x0D;

bool WriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
}  // namespace

bool PcfInit() {
  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(400000);
  Wire.beginTransmission(kAddr);
  if (Wire.endTransmission() != 0) return false;  // probe
  if (!WriteReg(kRegCtrl1, 0x00)) return false;   // STOP=0 -> run
  if (!WriteReg(kRegCtrl2, 0x00)) return false;   // clear alarm/timer flags
  if (!WriteReg(kRegClkout, 0x00)) return false;  // CLKOUT off (power)
  return true;
}

bool ReadRtcUtc(int64_t& epoch_utc, bool& vl) {
  Wire.begin(kSdaPin, kSclPin);
  Wire.beginTransmission(kAddr);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)kAddr, 7) != 7) return false;
  Regs r;
  r.sec = Wire.read();
  r.min = Wire.read();
  r.hour = Wire.read();
  r.day = Wire.read();
  r.wday = Wire.read();
  r.month = Wire.read();
  r.year = Wire.read();
  int yy = 0, mo = 0, dd = 0, hh = 0, mi = 0, ss = 0, wd = 0;
  if (!DecodeRegs(r, vl, yy, mo, dd, hh, mi, ss, wd)) return false;
  epoch_utc = MakeUtc(yy, mo, dd, hh, mi) + ss;
  return true;
}

bool WriteRtcUtc(int64_t epoch_utc) {
  // UTC civil fields via floor division (RTC stores UTC, T03 3-2).
  int64_t days = epoch_utc >= 0 ? epoch_utc / 86400
                                : -((-epoch_utc + 86399) / 86400);
  int64_t sod = epoch_utc - days * 86400;
  int yy = 0, mo = 0, dd = 0;
  CivilFromEpochDays(days, yy, mo, dd);
  int wday = (int)((days + 4) % 7);  // 1970-01-01 = Thursday
  if (wday < 0) wday += 7;
  Regs r;
  if (!EncodeRegs(yy, mo, dd, (int)(sod / 3600), (int)((sod % 3600) / 60),
                  (int)(sod % 60), wday, r))
    return false;
  Wire.begin(kSdaPin, kSclPin);
  Wire.beginTransmission(kAddr);
  Wire.write(0x02);
  Wire.write(r.sec);
  Wire.write(r.min);
  Wire.write(r.hour);
  Wire.write(r.day);
  Wire.write(r.wday);
  Wire.write(r.month);
  Wire.write(r.year);
  return Wire.endTransmission() == 0;
}

}  // namespace pcf
}  // namespace kdash
#else
// Host/CI guard: I2C needs the ESP32 toolchain.
#endif
