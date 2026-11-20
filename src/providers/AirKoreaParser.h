#pragma once
// AirKorea (에어코리아 대기오염정보 ArpltnInforInqireSvc) — 측정소별 실시간.
// "-" / "" / "null" / 공백 / 점검 문구는 Missing이며 절대 0으로 변환 금지
// (AIR-01). dataTime "YYYY-MM-DD 24:00" is the KST midnight of the next day.
#include <stddef.h>

#include "../../include/Config.h"
#include "../domain/Types.h"

namespace kdash {
namespace air {

inline constexpr int64_t kAirTtlSec = 3 * 3600;  // 측정 1시간 주기 + 발표 지연 여유

struct Row {
  const char* pm10_value = nullptr;  // "45", "-" when missing
  const char* pm25_value = nullptr;  // "18", "-" when missing
  const char* data_time = nullptr;   // "2026-11-01 14:00" KST or null
  const char* station = nullptr;     // stationName echo, or null when absent
};

struct AirOut {
  DataPoint<int> pm10;
  DataPoint<int> pm25;
  char station[kMaxStationLen + 1] = {0};  // owned copy (no input pointers)
  int64_t data_time_utc = -1;  // parsed dataTime as UTC, -1 if unparseable
  Outcome outcome = Outcome::ProviderError;
};

// Station selection (AIR-02): a row whose stationName is present and differs
// from the configured station is never used. The per-station operation may
// omit stationName (U-06); such rows are accepted because the request itself
// was scoped to station_filter. Select the newest parseable value independently
// per pollutant, regardless of row order, and retain its own observed_at.
AirOut Parse(const Row* rows, size_t n, const char* result_code,
             bool body_is_xml, const char* station_filter,
             const FetchContext& ctx);

bool IsMissingToken(const char* v);
int64_t ParseDataTimeKst(const char* dt);  // -> UTC, -1 on fail ("24:00" ok)

}  // namespace air
}  // namespace kdash
