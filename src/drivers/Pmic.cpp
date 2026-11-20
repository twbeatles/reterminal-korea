#include "Pmic.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <Wire.h>
#include "../boot/SerialLog.h"
#include "Battery.h"
#include "../power/BatteryPolicy.h"

namespace kdash {
namespace pmic {
namespace {
constexpr int kSda1 = 39;
constexpr int kScl1 = 40;
bool initialized = false;
bool usb_known = false;
void Init() {
  if (initialized) return;
  initialized = Wire1.begin(kSda1, kScl1, 400000);
  Wire1.setTimeOut(50);
}
bool ReadReg(uint8_t reg, uint8_t& value) {
  Init();
  if (!initialized) return false;
  Wire1.beginTransmission(kAddr);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0 || Wire1.requestFrom((int)kAddr, 1) != 1) {
    Wire1.end(); initialized = false;  // bounded reinit on the next poll
    return false;
  }
  value = (uint8_t)Wire1.read();
  return true;
}
class Registers final : public charge::IRegisters {
 public:
  bool Read(uint8_t reg, uint8_t& value) override { return ReadReg(reg,value); }
  bool Write(uint8_t reg, uint8_t value) override {
    Init();
    if (!initialized) return false;
    Wire1.beginTransmission(kAddr);
    Wire1.write(reg); Wire1.write(value);
    return Wire1.endTransmission() == 0;
  }
};
}  // namespace

void Diagnose() {
  uint8_t id=0, cfg=0, vreg=0, timer=0, status=0, bus=0;
  const bool ok = ReadReg(0x0B,id) && ReadReg(0x01,cfg) && ReadReg(0x04,vreg) &&
                  ReadReg(0x05,timer) && ReadReg(0x08,status) && ReadReg(0x0A,bus);
  KDASH_LOG.printf("pmic: probe_ok=%d id=%02x cfg=%02x vreg=%02x timer=%02x status=%02x bus=%02x\n",
                   (int)ok,id,cfg,vreg,timer,status,bus);
}

bool UsbAttached() {
  static bool sampled = false, usb = false;
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (sampled && (uint32_t)(now-last) < 1000) return usb;
  const bool first = !sampled, old_known = usb_known, old_usb = usb;
  last = now; sampled = true;
  uint8_t value=0;
  usb_known = ReadReg(kRegBusGood,value);
  usb = usb_known && UsbAttachedFromReg0A(value);
  if (first || old_known != usb_known || old_usb != usb)
    KDASH_LOG.printf("power: usb_known=%d usb=%d\n", (int)usb_known, (int)usb);
  return usb;
}
bool UsbKnown() { return usb_known; }
bool PrepareForSleep() {
  Registers regs;
  const bool ok = charge::PrepareForSleep(regs);
  KDASH_LOG.printf("charge: sleep_restore=%d\n", (int)ok);
  return ok;
}

charge::State MaintainCharge(bool enabled) {
  static charge::Protection policy;
  static bool sampled = false, last_enabled = false;
  static uint32_t last_ms = 0;
  const uint32_t now = millis();
  if (sampled && enabled == last_enabled && (uint32_t)(now-last_ms) < 30000)
    return policy.state();
  last_ms=now; sampled=true; last_enabled=enabled;
  Registers regs;
  const int mv = ReadBatteryMv();
  const charge::State state = policy.Tick(regs,enabled,mv);
  const charge::Confirmed& confirmed = policy.confirmed();
  KDASH_LOG.printf("charge: enabled=%d state=%u mv=%d estimated_pct=%d readback=%d cfg=%02x timer=%02x status=%02x\n",
      (int)enabled,(unsigned)state,mv,mv>=0?battery::PercentMv(mv):-1,
      (int)confirmed.valid,confirmed.cfg,confirmed.timer,confirmed.status);
  return state;
}

}  // namespace pmic
}  // namespace kdash
#else
// Host/CI guard: PMIC needs the ESP32 toolchain.
#endif
