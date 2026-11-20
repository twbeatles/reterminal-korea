#pragma once
// Battery policy (T08): mV -> state. Percent display is FORBIDDEN until a
// measured discharge curve exists (U-17). Portable.
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace battery {

enum class State : uint8_t { Normal = 0, Low, Critical, Unknown };

// Thresholds derived from the official E1001 discharge curve below:
// Low = 20% (3464mV), Critical = 5% (3252mV).
inline constexpr int kLowMv = 3464;
inline constexpr int kCriticalMv = 3252;

State Classify(int mv);
bool IsLowPower(int mv);  // Low or Critical

// Battery % from the official E1001/E1002 discharge curve
// (SenseCraft_HMI boards/reterminal_e1001/config.h @8d97d891, 0..100%).
// Piecewise-linear interpolation; clamps outside 2795..4111mV.
int PercentMv(int mv);

// ADC sampling contract (official battery_gauge collectSamples/sampleAverageMv):
// 10 samples, 25ms apart, plain mean (no min/max exclusion), 5ms settle after
// enable. Kept as constants so host tests pin the official behavior.
inline constexpr int kAdcSamples = 10;
inline constexpr int kAdcIntervalMs = 25;
inline constexpr int kAdcSettleMs = 10;  // Seeed E1001 wiki tip (ADR 0032)

// Plain mean of n samples (official sampleAverageMv semantics).
int MeanOf(const int* mv, size_t n);

}  // namespace battery
}  // namespace kdash
