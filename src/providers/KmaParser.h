#pragma once
// KMA (기상청 단기예보 조회서비스 VilageFcstInfoService_2.0) parsers.
// Release rules (data.go.kr 15084084, 2026-10-19 수정본 확인):
//  - 초단기실황 getUltraSrtNcst: base_time 정시(HH00) "06시 발표(정시단위)".
//    API 제공 지연은 페이지에 명시 없음 → 출발값 40분 (U-05, 실키 대조 필요).
//  - 단기예보 getVilageFcst: 02/05/08/11/14/17/20/23시 발표, +10분 출발값.
// Portable, ArduinoJson-free core: the JSON adapter (ProviderJson) feeds
// already-extracted item arrays. Items' strings are borrowed for the call
// only; outputs own no pointers into the input.
#include <stddef.h>

#include "../domain/Types.h"

namespace kdash {
namespace kma {

inline constexpr int kUltraNowDelayMin = 40;   // U-05 출발값
inline constexpr int kVillageDelayMin = 10;    // U-05 출발값
inline constexpr int64_t kUltraNowTtlSec = 2 * 3600;   // 실황 기준시각 허용 경과
inline constexpr int64_t kVillageTtlSec = 12 * 3600;   // 예보 발표 허용 경과
// numOfRows for getVilageFcst: items are delivered per forecast hour
// (~12 categories/hour), so 4 hours of slots cover "next slot >= now"
// from a base up to 3h10m old while keeping the body << kBodyMaxBytes.
inline constexpr int kVillageNumRows = 72;
inline constexpr int kUltraNowNumRows = 16;   // 8 categories + margin

// Latest already-released 회차 at now_utc (never a future 회차, KMA-03).
bool SelectUltraNowBase(int64_t now_utc, int& base_yyyymmdd, int& base_hhmm);
bool SelectVillageBase(int64_t now_utc, int& base_yyyymmdd, int& base_hhmm);
// UTC time at which the 회차 that SelectXxxBase would return next becomes
// requestable (for release-driven scheduling).
int64_t UltraNowBaseUtc(int64_t now_utc);
int64_t VillageBaseUtc(int64_t now_utc);

// One item as delivered ("T1H"/"21"). fcst_* only for forecasts.
struct RawItem {
  const char* category = nullptr;
  const char* value = nullptr;      // obsrValue / fcstValue raw string
  const char* fcst_date = nullptr;  // "YYYYMMDD" or null
  const char* fcst_time = nullptr;  // "HHMM" or null
};

struct NowOut {
  DataPoint<double> temp_c;      // T1H
  DataPoint<double> humidity;    // REH (%)
  DataPoint<double> rain_mm;     // RN1 ("-"/"강수없음" => Missing, never 0)
  int64_t base_utc = 0;          // 관측 기준시각 (== observed_at)
  Outcome outcome = Outcome::ProviderError;
};

struct ForecastOut {
  DataPoint<double> tmp;  // TMP at the selected slot
  DataPoint<int> pop;     // POP %
  DataPoint<int> pty;     // 강수형태 코드
  DataPoint<int> sky;     // 하늘상태 코드
  int64_t fcst_utc = 0;   // selected forecast slot (earliest >= current hour)
  int64_t base_utc = 0;   // 발표시각 (== observed_at)
  Outcome outcome = Outcome::ProviderError;
};

// result_code: header.resultCode (JSON) or cmmMsgHeader.returnReasonCode
// (XML error body). body_is_xml: true when the body was XML (a JSON request
// answered with XML is never Ok, KMA-02). base_*: the 회차 that was requested.
NowOut ParseUltraNow(const RawItem* items, size_t n, const char* result_code,
                     bool body_is_xml, int base_yyyymmdd, int base_hhmm,
                     const FetchContext& ctx);
ForecastOut ParseVillage(const RawItem* items, size_t n,
                         const char* result_code, bool body_is_xml,
                         int base_yyyymmdd, int base_hhmm,
                         const FetchContext& ctx);

bool IsMissingToken(const char* v);  // "-", "", "null", "강수없음", 공백
double ParseDecimal(const char* v, bool& ok);

}  // namespace kma
}  // namespace kdash
