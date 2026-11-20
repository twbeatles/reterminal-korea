#pragma once
// PCF8563 ESP32 adapter (T03 step 3): Wire read/write of UTC time.
// Portable codec lives in Pcf8563.h; this file is device-only.
#include <stdint.h>

namespace kdash {
namespace pcf {

// One-time init at boot (official RTC_PCF8563 rtcInit): probe, clear STOP
// (CTRL1=0, oscillator runs), clear alarm/timer flags (CTRL2=0), disable
// CLKOUT (saves power). Shared I2C bus runs at 400 kHz (official).
bool PcfInit();
// Reads RTC registers via Wire. Returns false on I2C failure.
// vl: VL flag (true => time untrusted). epoch: UTC seconds.
bool ReadRtcUtc(int64_t& epoch_utc, bool& vl);
// Writes UTC time to the RTC (NTP success + >2s skew path).
bool WriteRtcUtc(int64_t epoch_utc);

}  // namespace pcf
}  // namespace kdash
