#pragma once
// Format: number/time text rules (T04a, docs/05). Pure functions, portable.
// All timestamps are KST strings; "실시간" wording is forbidden for
// fixing/prior-close values.
#include <stddef.h>
#include <stdint.h>

#include "icons/IconData.h"

namespace kdash {
namespace fmt {

// "21.5°C" / "--" when missing. Returns chars written (no NUL counted).
size_t TempC(double v, bool missing, char* out, size_t cap);
size_t Percent(int v, bool missing, char* out, size_t cap);  // "55%"
size_t RainMm(double v, bool missing, char* out, size_t cap);  // "0mm"/"--"
size_t Pm(int v, bool missing, const char* grade, char* out,
          size_t cap);  // "18 ㎍/㎥ 보통"
const char* Pm10Grade(int v);
const char* Pm25Grade(int v);
const char* SkyText(int code);  // 1 맑음 3 구름많음 4 흐림
const char* PtyText(int code);  // 0 없음 1 비 2 비/눈 3 눈 4 소나기
// "1,384.50" (thousands + 2 decimals from won_scaled/scale=2).
size_t FxRate(int64_t won_scaled, int32_t scale, char* out, size_t cap);
// "78,500" integer with commas.
size_t Krw(int64_t price, char* out, size_t cap);
// 소수 2자리 x100 정수 -> "5,371.68" (지수 값).
size_t Index2(int64_t value_x100, char* out, size_t cap);
// 0.01% 단위 -> "+1.23%" / "-0.51%" / "0.00%".
size_t ChangePct(int32_t bp, char* out, size_t cap);
// KST "HH:MM" or "MM-DD HH:MM" when the date differs from now.
size_t KstTime(int64_t epoch_utc, int64_t now_utc, char* out, size_t cap);
// "고시 기준일 10-30(금)" — date + weekday, never "실시간".
size_t FixingDay(int64_t fixing_utc, char* out, size_t cap);
// Weather icon from KMA codes (-1 = missing). PTY (precipitation) wins over
// SKY; 소나기/빗방울 share the rain icon, the text label tells them apart.
ui::IconId WeatherIconFor(int sky, int pty);
// Header "11-02(월) 14:30" (KST, always with date + weekday).
size_t HeaderDateTime(int64_t now_utc, char* out, size_t cap);
// Battery icon tier: USB -> charging, mv<0 -> None, battery::IsLowPower ->
// low, else full/mid at 60% of the official curve (no % is displayed).
ui::IconId BatteryIconFor(int battery_mv, bool usb);
// "2시간 전" / "N분 전" / "N일 전".
size_t Age(int64_t observed_utc, int64_t now_utc, char* out, size_t cap);

}  // namespace fmt
}  // namespace kdash
