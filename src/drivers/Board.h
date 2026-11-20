#pragma once
// Board/driver contracts. Pin maps, display power-enable, battery ADC scale,
// and button wake GPIOs MUST come from the official example headers
// (hardware/board_e1001.md) — no memorized values here (AGENTS.md, U-02).
// This file only fixes the interface the rest of the firmware programs to.
#include <stdint.h>

#include "../../include/IClock.h"
#include "../../include/IDisplay.h"
#include "../../include/IHttpClient.h"
#include "../../include/IStorage.h"

namespace kdash {
namespace drivers {

class Board {
 public:
  virtual ~Board() = default;
  virtual void early_init() = 0;  // clocks, serial, reset-reason capture
  virtual void log_boot_nosecrets() = 0;  // fw version/sha, heap/PSRAM, RTC — 비밀정보 없음
  virtual IClock& clock() = 0;
  virtual IDisplay& display() = 0;
  virtual IStorage& nvs() = 0;
  virtual ISensor& sht40() = 0;
  virtual float battery_v() = 0;       // raw volts; % mapping needs discharge curve
  virtual bool usb_present() = 0;      // false-safe: 불확실하면 배터리 스케줄
  virtual int button_event() = 0;      // 0 none, 1/2/3 short prev/next/refresh
  virtual bool button_longpress() = 0;  // setup portal entry (long != click)
};

}  // namespace drivers
}  // namespace kdash
