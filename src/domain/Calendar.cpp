#include "Calendar.h"

#include "Freshness.h"
#include "../providers/FinanceParsers.h"

namespace kdash {
namespace cal {

void Clear(YearTable& t, int year) {
  t.year = year;
  for (int i = 0; i < 12; i++) t.bitmap[i] = 0;
  for (int i = 0; i < 12; i++) t.market_closed[i] = 0;
  t.loaded = false;
  t.market_verified = false;
  t.fetched_utc = 0;
}

void SetHoliday(YearTable& t, int mon, int day, bool on) {
  if (mon < 1 || mon > 12 || day < 1 || day > 31) return;
  if (on)
    t.bitmap[mon - 1] |= (1u << (day - 1));
  else
    t.bitmap[mon - 1] &= ~(1u << (day - 1));
}

void SetMarketClosed(YearTable& t, int mon, int day, bool on) {
  if (mon < 1 || mon > 12 || day < 1 || day > 31) return;
  if (on)
    t.market_closed[mon - 1] |= (1u << (day - 1));
  else
    t.market_closed[mon - 1] &= ~(1u << (day - 1));
}

bool IsHoliday(const YearTable& t, int mon, int day) {
  if (mon < 1 || mon > 12 || day < 1 || day > 31) return false;
  return (t.bitmap[mon - 1] & (1u << (day - 1))) != 0;
}

bool IsMarketClosed(const YearTable& t, int mon, int day) {
  if (mon < 1 || mon > 12 || day < 1 || day > 31) return false;
  return (t.market_closed[mon - 1] & (1u << (day - 1))) != 0;
}

static const YearTable* Find(const YearTable* tables, size_t n, int year) {
  if (!tables) return nullptr;
  for (size_t i = 0; i < n; i++) {
    if (tables[i].loaded && tables[i].year == year) return &tables[i];
  }
  return nullptr;
}

DayKind ClassifyKstDay(int64_t utc, const YearTable* tables, size_t n) {
  KstBroken k = BreakKst(utc);
  bool weekend = IsKstWeekend(utc);
  const YearTable* t = Find(tables, n, k.year);
  if (!t) {
    // No table for this year: weekends known, weekdays Unknown.
    return weekend ? DayKind::Weekend : DayKind::Unknown;
  }
  if (IsMarketClosed(*t, k.month, k.day)) return DayKind::MarketClosed;
  if (IsHoliday(*t, k.month, k.day)) {
    return weekend ? DayKind::Weekend : DayKind::PublicHoliday;
  }
  if (weekend) return DayKind::Weekend;
  return DayKind::BusinessDay;
}

bool IsKrxOpen(int64_t utc, const YearTable* tables, size_t n) {
  if (!fin::IsKrxOpen(utc)) return false;
  DayKind k = ClassifyKstDay(utc, tables, n);
  // Tables only close extra days; Unknown years keep the legacy behavior.
  if (k == DayKind::PublicHoliday || k == DayKind::MarketClosed) return false;
  return true;
}

int64_t NextBusinessDayUtc(int64_t utc, const YearTable* tables, size_t n) {
  // Next KST midnight whose day classifies as BusinessDay (bounded 366).
  for (int d = 0; d < 366; d++) {
    int64_t t = utc + (int64_t)d * 86400;
    KstBroken k = BreakKst(t);
    int64_t mid = KstToUtc(k.year, k.month, k.day, 24, 0);
    if (mid < utc) continue;
    if (ClassifyKstDay(mid, tables, n) == DayKind::BusinessDay) return mid;
  }
  return INT64_MAX;
}

void AlignYears(YearTable (&tables)[2], int year) {
  YearTable aligned[2];
  for (int i=0;i<2;i++) {
    Clear(aligned[i],year+i);
    for (const YearTable& existing : tables)
      if (existing.year==year+i) { aligned[i]=existing; break; }
  }
  tables[0]=aligned[0]; tables[1]=aligned[1];
}

const char* MarketStateText(int64_t utc, bool verified,
                            const YearTable* tables, size_t n) {
  if (!verified) return "시간 미확인";
  const DayKind kind=ClassifyKstDay(utc,tables,n);
  if (kind==DayKind::Weekend || kind==DayKind::PublicHoliday ||
      kind==DayKind::MarketClosed) return "휴장";
  const YearTable* t=Find(tables,n,BreakKst(utc).year);
  if (!t) return "일정 미확인";
  // Public holidays do not prove special exchange sessions/closures. Describe
  // the regular time window without claiming that trading is actually open.
  if (!t->market_verified)
    return fin::IsKrxOpen(utc) ? "정규장 시간" : "정규장 외";
  if (BreakKst(utc).hour < 9) return "장시작 전";
  return IsKrxOpen(utc,tables,n) ? "장중" : "장마감";
}

}  // namespace cal
}  // namespace kdash
