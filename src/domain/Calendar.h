#pragma once
// Holiday/market calendar (T09). Two yearly bitmaps (this + next year).
// bit (d-1) of bitmap[m-1] == public holiday. Unknown years stay Unknown
// (weekends only + "일정 미확인" label, never Mon-Fri == business day).
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace cal {

enum class DayKind : uint8_t {
  BusinessDay = 0,
  Weekend,
  PublicHoliday,
  MarketClosed,  // KRX extra closure (e.g. 12/31) from the yearly table
  Unknown,
};

struct YearTable {
  int year = 0;
  uint32_t bitmap[12] = {};         // public holidays (특일 API)
  uint32_t market_closed[12] = {};  // KRX extra closures (연도별 상수표)
  bool loaded = false;
  bool market_verified = false;  // full-year KRX exceptions verified separately
  int64_t fetched_utc = 0;
};

void Clear(YearTable& t, int year);
void SetHoliday(YearTable& t, int mon, int day, bool on = true);
void SetMarketClosed(YearTable& t, int mon, int day, bool on = true);
bool IsHoliday(const YearTable& t, int mon, int day);
bool IsMarketClosed(const YearTable& t, int mon, int day);

DayKind ClassifyKstDay(int64_t utc, const YearTable* tables, size_t n);
bool IsKrxOpen(int64_t utc, const YearTable* tables, size_t n);
int64_t NextBusinessDayUtc(int64_t utc, const YearTable* tables, size_t n);
void AlignYears(YearTable (&tables)[2], int current_year);
const char* MarketStateText(int64_t utc, bool clock_verified,
                            const YearTable* tables, size_t n);

}  // namespace cal
}  // namespace kdash
