#pragma once
#include <stdint.h>

namespace kdash { namespace charge {
enum class State : uint8_t { Off, Unknown, Unsupported, Allowed, Limited, Fault };
class IRegisters {
 public:
  virtual ~IRegisters() = default;
  virtual bool Read(uint8_t reg, uint8_t& value) = 0;
  virtual bool Write(uint8_t reg, uint8_t value) = 0;
};
// SY6974B Rev.0.9 REG0B: PN[6:3]=1000, reserved[2]=0, DEV_REV=00.
inline bool Supported(uint8_t id) { return id == 0x40; }
struct Confirmed {
  bool valid = false;
  uint8_t cfg = 0, timer = 0, status = 0;
};
class Protection {
 public:
  State Tick(IRegisters& port, bool enabled, int mv);
  State state() const { return state_; }
  const Confirmed& confirmed() const { return confirmed_; }
 private:
  State state_ = State::Unknown;
  uint8_t high_ = 0, low_ = 0;
  bool initialized_ = false, limited_ = false;
  Confirmed confirmed_;
};
const char* Label(State state);
// Restore autonomous charging before battery sleep; no writes with USB/OTG/unknown ID.
bool PrepareForSleep(IRegisters& port);
} }
