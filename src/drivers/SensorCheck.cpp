#include "SensorCheck.h"

namespace kdash {
namespace drivers {

SensorCheck CheckSensorRange(float temp_c, float humidity_pct) {
  if (temp_c != temp_c || humidity_pct != humidity_pct)
    return SensorCheck::NoData;  // NaN => 결측 (0으로 표기 금지)
  if (temp_c < kTempAbsMin || temp_c > kTempAbsMax) return SensorCheck::OutOfRange;
  if (humidity_pct < 0.0f || humidity_pct > 100.0f) return SensorCheck::OutOfRange;
  return SensorCheck::Ok;
}

bool IsIndoorPlausible(float temp_c) {
  return temp_c >= kTempIndoorMin && temp_c <= kTempIndoorMax;
}

}  // namespace drivers
}  // namespace kdash
