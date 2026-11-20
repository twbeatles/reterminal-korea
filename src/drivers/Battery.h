#pragma once
// Battery ADC driver (official battery_gauge collectSamples): enable GPIO21
// HIGH, 5ms settle, 10x analogReadMilliVolts(GPIO1, 12-bit, 11dB) 25ms apart,
// enable LOW, plain mean, divider x2. Device-only; percent mapping lives in
// BatteryPolicy.
#include <stdint.h>

namespace kdash {

void BatteryBegin();
// Returns battery mV (already x2), or -1 on invalid reading.
int ReadBatteryMv();

}  // namespace kdash
