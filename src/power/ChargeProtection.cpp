#include "ChargeProtection.h"
#include "BatteryPolicy.h"

namespace kdash { namespace charge {
namespace {
bool Set(IRegisters& port, uint8_t reg, uint8_t& current, uint8_t mask, uint8_t bits) {
  uint8_t next = (current & (uint8_t)~mask) | bits;
  if (reg == 1) next &= (uint8_t)~0x40;  // WD_RST is a command, never replay it
  if (current == next) return true;
  uint8_t readback = 0;
  if (!port.Write(reg, next) || !port.Read(reg, readback) ||
      (readback & (uint8_t)(reg == 1 ? 0xBF : 0xFF)) != next) return false;
  current=readback;
  return true;
}
}
State Protection::Tick(IRegisters& port, bool enabled, int mv) {
  confirmed_ = Confirmed{};
  uint8_t id=0, cfg=0, timer=0, bus=0, status=0;
  auto fail = [&]() { high_ = low_ = 0; return state_ = State::Fault; };
  if (!port.Read(0x0B,id)) return fail();
  if (!Supported(id)) { high_ = low_ = 0; return state_ = State::Unsupported; }
  if (!port.Read(1,cfg) || !port.Read(5,timer) || !port.Read(0x0A,bus)) return fail();
  if (cfg & 0x20) return fail();  // OTG active: do not change any config
  if (!enabled) {
    high_ = low_ = 0; initialized_ = false;
    if (!Set(port,1,cfg,0x10,0x10)) return fail();
    if (!port.Read(8,status)) return fail();
    confirmed_={true,cfg,timer,status};
    return state_ = State::Off;
  }
  if (!(bus & 0x80)) { high_ = low_ = 0; return state_ = State::Unknown; }
  if (mv < 2500 || mv > 4500) return fail();
  if (!initialized_) { limited_ = !(cfg & 0x10); initialized_ = true; }
  const int pct = battery::PercentMv(mv);
  high_ = pct >= 80 ? (high_ < 3 ? high_+1 : 3) : 0;
  low_ = pct <= 75 ? (low_ < 3 ? low_+1 : 3) : 0;
  if (high_ >= 3) limited_ = true;
  if (low_ >= 3) limited_ = false;
  // Disable only the I2C watchdog; EN_TERM, EN_TIMER and temperature
  // protection bits are preserved. Long blocking calls cannot undo the limit.
  if (!Set(port,5,timer,0x30,0) ||
      !Set(port,1,cfg,0x10,limited_ ? 0 : 0x10)) return fail();
  if (!port.Read(8,status)) return fail();
  confirmed_={true,cfg,timer,status};
  return state_ = limited_ ? State::Limited : State::Allowed;
}
bool PrepareForSleep(IRegisters& port) {
  uint8_t id=0, bus=0, cfg=0;
  if (!port.Read(0x0B,id)) return false;
  if (!Supported(id)) return true;  // unsupported hardware stays untouched
  if (!port.Read(0x0A,bus) || !port.Read(1,cfg)) return false;
  if (bus & 0x80) return true;  // USB reattached: preserve the active limit
  if (cfg & 0x20) return false;
  return Set(port,1,cfg,0x10,0x10);
}
const char* Label(State s) {
  switch (s) {
    case State::Off: return "보호 꺼짐";
    case State::Unsupported: return "보호 미지원";
    case State::Allowed: return "80% 목표 · 충전 허용";
    case State::Limited: return "80% 목표 · 충전 중단";
    default: return "보호 확인 필요";
  }
}
} }
