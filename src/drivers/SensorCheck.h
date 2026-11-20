#pragma once
// SHT40 indoor reading plausibility (M1). Out-of-range => Missing, never a
// wild value on screen. Ranges: sensor -40..125C / 0..100%; indoor sanity
// window -10..50C applied by the coordinator for the 생활 page.
#include "../domain/Types.h"

namespace kdash {
namespace drivers {

inline constexpr float kTempAbsMin = -40.0f;
inline constexpr float kTempAbsMax = 125.0f;
inline constexpr float kTempIndoorMin = -10.0f;
inline constexpr float kTempIndoorMax = 50.0f;

enum class SensorCheck : uint8_t { Ok = 0, OutOfRange, NoData };

// Absolute sensor range gate (wiring/library failure => NoData/Missing).
SensorCheck CheckSensorRange(float temp_c, float humidity_pct);

// Indoor plausibility for display (동파·찜질방 오해 방지 — 범위를 벗어나면
// '확인 필요' 배지와 함께 Missing 처리, 임의 클리핑 금지).
bool IsIndoorPlausible(float temp_c);

}  // namespace drivers
}  // namespace kdash
