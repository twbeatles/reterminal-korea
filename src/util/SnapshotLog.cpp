#include "SnapshotLog.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>

#include "../boot/SerialLog.h"
#include "../domain/Freshness.h"
#include "../domain/Types.h"

namespace kdash {
namespace debug {
namespace {

template <typename T>
void PrintPoint(const char* label, const DataPoint<T>& d, const char* unit) {
  if (d.has_value) {
    KDASH_LOG.printf("  %-12s %10.2f %-4s %s\n", label, (double)d.value, unit,
                  ToString(d.validity));
  } else {
    KDASH_LOG.printf("  %-12s %10s %-4s %s\n", label, "--", unit,
                  ToString(d.validity));
  }
}

}  // namespace

void PrintSnapshot(const app::Snapshot& s) {
  KDASH_LOG.println("snapshot:");
  if (s.has_now) {
    KstBroken b = BreakKst(s.now.base_utc);
    KDASH_LOG.printf("  KMA 실황 기준 %02d-%02d %02d:%02d KST\n", b.month, b.day, b.hour, b.minute);
    PrintPoint("기온", s.now.temp_c, "C");
    PrintPoint("습도", s.now.humidity, "%");
    PrintPoint("강수량", s.now.rain_mm, "mm");
  }
  if (s.has_fc) {
    KstBroken b = BreakKst(s.fc.fcst_utc);
    KDASH_LOG.printf("  KMA 예보 %02d-%02d %02d:00 KST\n", b.month, b.day, b.hour);
    PrintPoint("예보기온", s.fc.tmp, "C");
    PrintPoint("강수확률", s.fc.pop, "%");
  }
  if (s.has_air) {
    KstBroken b = BreakKst(s.air.data_time_utc);
    KDASH_LOG.printf("  에어코리아 측정 %02d-%02d %02d:00 KST\n", b.month, b.day, b.hour);
    PrintPoint("PM10", s.air.pm10, "ug");
    PrintPoint("PM2.5", s.air.pm25, "ug");
  }
  if (s.has_fx) {
    KstBroken b = BreakKst(s.fx.fixing_date_utc);
    KDASH_LOG.printf("  고시환율 기준일 %04d-%02d-%02d\n", b.year, b.month, b.day);
    if (s.fx.usd.has_value)
      KDASH_LOG.printf("  USD          %10.2f KRW  %s\n", s.fx.usd.value.won_scaled / 100.0, ToString(s.fx.usd.validity));
    if (s.fx.jpy100.has_value)
      KDASH_LOG.printf("  JPY(100)     %10.2f KRW  %s\n", s.fx.jpy100.value.won_scaled / 100.0, ToString(s.fx.jpy100.validity));
  }
  for (size_t i = 0; i < kMaxWatchlist; i++) {
    if (s.has_fsc[i])
      KDASH_LOG.printf("  전일종가 %s %lld %s\n", s.fsc[i].quote.value.symbol,
                    (long long)s.fsc[i].quote.value.price_krw, ToString(s.fsc[i].quote.validity));
    if (s.has_kis[i])
      KDASH_LOG.printf("  현재가(REST) %s %lld %s\n", s.kis[i].quote.value.symbol,
                    (long long)s.kis[i].quote.value.price_krw, ToString(s.kis[i].quote.validity));
  }
  if (s.has_kospi)
    KDASH_LOG.printf("  지수(REST) %s %lld.%02d chg_bp=%d %s\n", s.kospi.quote.value.name,
                    (long long)(s.kospi.quote.value.value_x100 / 100),
                    (int)(s.kospi.quote.value.value_x100 % 100),
                    s.kospi.quote.value.has_change ? (int)s.kospi.quote.value.change_bp : 0,
                    ToString(s.kospi.quote.validity));
}

}  // namespace debug
}  // namespace kdash
#else
// Host/CI guard: UART sink needs the ESP32 toolchain.
#endif
