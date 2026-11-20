#include "Battery.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>

#include "../power/BatteryPolicy.h"

namespace kdash {
namespace {
constexpr int kAdcPin = 1;
constexpr int kEnablePin = 21;
}  // namespace

void BatteryBegin() {
  pinMode(kEnablePin, OUTPUT);
  digitalWrite(kEnablePin, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(kAdcPin, ADC_11db);
}

int ReadBatteryMv() {
  // Official collectSamples semantics: enable HIGH, 5ms settle, 10 samples
  // 25ms apart (ADC-side mV), enable LOW, plain mean, then divider x2.
  digitalWrite(kEnablePin, HIGH);
  delay(battery::kAdcSettleMs);
  int mv[battery::kAdcSamples];
  for (int i = 0; i < battery::kAdcSamples; i++) {
    mv[i] = analogReadMilliVolts(kAdcPin);
    if (i + 1 < battery::kAdcSamples) delay(battery::kAdcIntervalMs);
  }
  digitalWrite(kEnablePin, LOW);
  int avg = battery::MeanOf(mv, battery::kAdcSamples);
  return avg < 0 ? -1 : avg * 2;
}

}  // namespace kdash
#else
// Host/CI guard: ADC needs the ESP32 toolchain.
#endif
