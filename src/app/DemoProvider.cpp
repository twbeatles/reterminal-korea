#include "DemoProvider.h"

#include <string.h>

namespace kdash {
namespace demo {

// Synthetic-source labels: every demo value carries DEMO so demo data can
// never be mistaken for live provider data (AGENTS.md, F-01).
constexpr const char* kKmaDemo = "기상청(DEMO)";
constexpr const char* kAirDemo = "에어코리아(DEMO)";
constexpr const char* kEximDemo = "한국수출입은행(DEMO)";

static void Dbl(DataPoint<double>& d, double v, const char* src,
                int64_t now_utc) {
  d.has_value = true;
  d.value = v;
  d.validity = Validity::Valid;
  d.source = src;
  d.observed_at = SourceTs(now_utc);
  d.fetched_at = SourceTs(now_utc);
}

static void IntD(DataPoint<int>& d, int v, const char* src, int64_t now_utc) {
  d.has_value = true;
  d.value = v;
  d.validity = Validity::Valid;
  d.source = src;
  d.observed_at = SourceTs(now_utc);
  d.fetched_at = SourceTs(now_utc);
}

void FillDemo(app::Snapshot& snap, int64_t now_utc) {
  snap = app::Snapshot();
  snap.has_now = true;
  snap.now.base_utc = now_utc;
  snap.now.outcome = Outcome::Ok;
  Dbl(snap.now.temp_c, 21.5, kKmaDemo, now_utc);
  Dbl(snap.now.humidity, 55.0, kKmaDemo, now_utc);
  Dbl(snap.now.rain_mm, 0.0, kKmaDemo, now_utc);
  snap.has_fc = true;
  snap.fc.base_utc = now_utc;
  snap.fc.fcst_utc = now_utc + 3600;
  snap.fc.outcome = Outcome::Ok;
  Dbl(snap.fc.tmp, 22.0, kKmaDemo, now_utc);
  IntD(snap.fc.pop, 60, kKmaDemo, now_utc);
  IntD(snap.fc.pty, 0, kKmaDemo, now_utc);
  IntD(snap.fc.sky, 4, kKmaDemo, now_utc);
  snap.has_air = true;
  snap.air.data_time_utc = now_utc;
  snap.air.outcome = Outcome::Ok;
  strncpy(snap.air.station, "종로구", sizeof(snap.air.station) - 1);
  IntD(snap.air.pm10, 18, kAirDemo, now_utc);
  IntD(snap.air.pm25, 9, kAirDemo, now_utc);
  snap.has_fx = true;
  snap.fx.fixing_date_utc = now_utc;
  snap.fx.outcome = Outcome::Ok;
  snap.fx.usd.has_value = true;
  snap.fx.usd.validity = Validity::Valid;
  snap.fx.usd.source = kEximDemo;
  strncpy(snap.fx.usd.value.currency_unit, "USD",
          sizeof(snap.fx.usd.value.currency_unit) - 1);
  snap.fx.usd.value.won_scaled = 138450;
  snap.fx.usd.value.scale = 2;
  snap.fx.jpy100.has_value = true;
  snap.fx.jpy100.validity = Validity::Valid;
  snap.fx.jpy100.source = kEximDemo;
  strncpy(snap.fx.jpy100.value.currency_unit, "JPY(100)",
          sizeof(snap.fx.jpy100.value.currency_unit) - 1);
  snap.fx.jpy100.value.won_scaled = 92050;
  snap.fx.jpy100.value.scale = 2;
}

}  // namespace demo
}  // namespace kdash
