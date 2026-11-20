#include "ViewModel.h"

#include <stdio.h>
#include <string.h>

#include "Format.h"
#include "../domain/Freshness.h"
#include "../domain/HolidayNames.h"
#include "../domain/SolarTerms.h"
#include "../domain/StockNames.h"
#include "../power/BatteryPolicy.h"

namespace kdash {
namespace vm {
namespace {

void SetCell(Cell& c, const char* label, const char* value, const char* unit,
             const char* sub, Badge b) {
  strncpy(c.label, label ? label : "", sizeof(c.label) - 1);
  strncpy(c.value, value ? value : "", sizeof(c.value) - 1);
  strncpy(c.unit, unit ? unit : "", sizeof(c.unit) - 1);
  strncpy(c.sub, sub ? sub : "", sizeof(c.sub) - 1);
  c.badge = b;
}

Badge BadgeFor(Validity v, bool configured, bool has_value,
               bool auth_blocked) {
  if (!configured) return Badge::NeedSetup;
  if (auth_blocked) return Badge::Check;
  switch (v) {
    case Validity::Valid: return has_value ? Badge::None : Badge::None;
    case Validity::Stale: return Badge::Stale;
    case Validity::Missing: return Badge::None;
    case Validity::ProviderError: return has_value ? Badge::LastGood : Badge::Check;
    case Validity::NotConfigured: return Badge::NeedSetup;
  }
  return Badge::None;
}

Badge ProviderBadge(const app::AppState& st, sched::Provider p,
                    Validity validity, bool configured, bool has_value) {
  const auto& status = st.providers[(int)p];
  if (!configured || status.auth_blocked)
    return BadgeFor(validity, configured, has_value, status.auth_blocked);
  // Value age and collection outcome are independent. Equality is needed
  // for attempts made within the same clock second as the preceding success.
  if (status.last_attempt_utc > 0 &&
      status.last_attempt_utc >= status.last_success_utc &&
      status.last_outcome != Outcome::Ok)
    return has_value ? Badge::LastGood : Badge::Check;
  return BadgeFor(validity, configured, has_value, false);
}

void FillHeader(Header& h, const app::AppState& st, int64_t now_utc,
                bool clock_verified, bool wifi_ok) {
  strncpy(h.place, st.settings.location.display_name, sizeof(h.place) - 1);
  fmt::HeaderDateTime(now_utc, h.datetime, sizeof(h.datetime));
  h.wifi_ok = wifi_ok;
  h.time_ok = clock_verified;
}

}  // namespace

const char* BadgeText(Badge b) {
  switch (b) {
    case Badge::Stale: return "오래된 정보";
    case Badge::LastGood: return "마지막 정상값";
    case Badge::NeedSetup: return "설정 필요";
    case Badge::Check: return "확인 필요";
    case Badge::Demo: return "DEMO";
    default: return "";
  }
}

LifePage BuildLife(const app::Snapshot& snap, const app::AppState& st,
                   int64_t now_utc, bool clock_verified, bool wifi_ok,
                   bool demo) {
  LifePage p;
  FillHeader(p.header, st, now_utc, clock_verified, wifi_ok);
  p.demo = demo;
  char v[48], sub[64];

  bool cfg_kma = app::ProviderConfigured(st.settings, sched::Provider::KmaNow);
  // Temp
  if (!cfg_kma) {
    SetCell(p.temp, "기온", "--", "°C", "", demo ? Badge::Demo : Badge::NeedSetup);
  } else if (!snap.has_now) {
    SetCell(p.temp, "기온", "--", "°C", "",
            demo ? Badge::Demo : Badge::Check);
  } else {
    const auto& d = snap.now.temp_c;
    fmt::TempC(d.value, !d.has_value || d.validity == Validity::Missing, v,
               sizeof(v));
    fmt::KstTime(d.observed_at.epoch_utc, now_utc, sub, sizeof(sub));
    char line[64];
    snprintf(line, sizeof(line), "관측 %.14s", sub);
    Badge b = ProviderBadge(st, sched::Provider::KmaNow, d.validity, true, d.has_value);
    if (d.validity == Validity::Stale) {
      char age[32];
      fmt::Age(d.observed_at.epoch_utc, now_utc, age, sizeof(age));
      snprintf(line, sizeof(line), "관측 %.14s · %.28s", sub, age);
    }
    if (demo) b = Badge::Demo;
    SetCell(p.temp, "기온", v, "°C", line, b);
  }
  // Humidity
  if (!cfg_kma || !snap.has_now) {
    SetCell(p.humidity, "습도", "--", "%", "",
            !cfg_kma ? Badge::NeedSetup : demo ? Badge::Demo : Badge::Check);
  } else {
    const auto& d = snap.now.humidity;
    bool miss = !d.has_value || d.validity == Validity::Missing;
    fmt::Percent((int)d.value, miss, v, sizeof(v));
    Badge b = ProviderBadge(st, sched::Provider::KmaNow, d.validity, true, d.has_value);
    if (demo) b = Badge::Demo;
    SetCell(p.humidity, "습도", v, "%", "", b);
  }
  // Rain
  if (!cfg_kma || !snap.has_now) {
    SetCell(p.rain, "강수량", "--", "mm", "",
            !cfg_kma ? Badge::NeedSetup : demo ? Badge::Demo : Badge::Check);
  } else {
    const auto& d = snap.now.rain_mm;
    bool miss = !d.has_value || d.validity == Validity::Missing;
    fmt::RainMm(d.value, miss, v, sizeof(v));
    Badge b = ProviderBadge(st, sched::Provider::KmaNow, d.validity, true, d.has_value);
    if (demo) b = Badge::Demo;
    SetCell(p.rain, "강수량", v, "", "", b);
  }
  // Forecast
  bool cfg_fc =
      app::ProviderConfigured(st.settings, sched::Provider::KmaForecast);
  if (!cfg_fc || !snap.has_fc) {
    SetCell(p.forecast, "예보", "--", "", "",
            !cfg_fc ? Badge::NeedSetup : demo ? Badge::Demo : Badge::Check);
  } else {
    const auto& t = snap.fc.tmp;
    const auto& pop = snap.fc.pop;
    char tv[48], pv[48];
    fmt::TempC(t.value, !t.has_value || t.validity == Validity::Missing, tv,
               sizeof(tv));
    fmt::Percent(pop.value, !pop.has_value || pop.validity == Validity::Missing,
                 pv, sizeof(pv));
    fmt::KstTime(snap.fc.fcst_utc, now_utc, sub, sizeof(sub));
    char value[48], line[64];
    const int pty = snap.fc.pty.has_value ? snap.fc.pty.value : -1;
    const char* sky = pty > 0 ? fmt::PtyText(pty)
                              : fmt::SkyText(snap.fc.sky.has_value ? snap.fc.sky.value : -1);
    snprintf(value, sizeof(value), "%.16s · 강수확률 %.8s", tv, pv);
    snprintf(line, sizeof(line), "예보 %.14s · %.24s", sub, sky);
    Badge b = ProviderBadge(st, sched::Provider::KmaForecast, t.validity, true, t.has_value);
    if (demo) b = Badge::Demo;
    SetCell(p.forecast, "예보", value, "", line, b);
  }
  // PM
  bool cfg_air =
      app::ProviderConfigured(st.settings, sched::Provider::AirKorea);
  if (!cfg_air || !snap.has_air) {
    const Badge b = !cfg_air ? Badge::NeedSetup : demo ? Badge::Demo : Badge::Check;
    SetCell(p.pm10, "미세먼지", "--", "", "", b);
    SetCell(p.pm25, "초미세먼지", "--", "", "", b);
    snprintf(p.air_sub, sizeof(p.air_sub), "%s",
             !cfg_air ? "에어코리아 키·측정소 설정 필요" : "대기질 수신 전");
  } else {
    const auto& d10 = snap.air.pm10;
    const auto& d25 = snap.air.pm25;
    bool m10 = !d10.has_value || d10.validity == Validity::Missing;
    bool m25 = !d25.has_value || d25.validity == Validity::Missing;
    fmt::Pm(d10.value, m10, fmt::Pm10Grade(d10.value), v, sizeof(v));
    Badge b10 = ProviderBadge(st, sched::Provider::AirKorea, d10.validity, true, d10.has_value);
    if (demo) b10 = Badge::Demo;
    SetCell(p.pm10, "미세먼지", v, "", "", b10);
    fmt::Pm(d25.value, m25, fmt::Pm25Grade(d25.value), v, sizeof(v));
    Badge b25 = ProviderBadge(st, sched::Provider::AirKorea, d25.validity, true, d25.has_value);
    if (demo) b25 = Badge::Demo;
    SetCell(p.pm25, "초미세먼지", v, "", "", b25);
    const DataPoint<int>* points[] = {&d10, &d25};
    Cell* cells[] = {&p.pm10, &p.pm25};
    for (int i = 0; i < 2; ++i) {
      if (!points[i]->has_value || points[i]->observed_at.epoch_utc <= 0) continue;
      char at[16];
      fmt::KstTime(points[i]->observed_at.epoch_utc, now_utc, at, sizeof(at));
      snprintf(cells[i]->sub, sizeof(cells[i]->sub), "관측 %s", at);
      if (!demo && points[i]->observed_at.epoch_utc < snap.air.data_time_utc &&
          cells[i]->badge == Badge::None) cells[i]->badge = Badge::LastGood;
    }
    // PM source time (dataTime) and station, so the numbers are never timeless.
    const char* station = snap.air.station[0] ? snap.air.station
                                              : st.settings.location.air_station;
    const int64_t at = snap.air.data_time_utc > 0 ? snap.air.data_time_utc
                                                  : d10.observed_at.epoch_utc;
    char t[16] = "--";
    if (at > 0) fmt::KstTime(at, now_utc, t, sizeof(t));
    if (d10.has_value && d25.has_value &&
        d10.observed_at.epoch_utc != d25.observed_at.epoch_utc)
      snprintf(p.air_sub, sizeof(p.air_sub), "측정소 %.24s", station);
    else
      snprintf(p.air_sub, sizeof(p.air_sub), "측정소 %.24s · 측정 %s", station, t);
  }
  // Sky/PTY
  if (!cfg_fc || !snap.has_fc) {
    SetCell(p.sky, "하늘", "--", "", "", Badge::None);
  } else {
    const char* s = fmt::SkyText(snap.fc.sky.has_value ? snap.fc.sky.value : -1);
    const char* q = fmt::PtyText(snap.fc.pty.has_value ? snap.fc.pty.value : -1);
    snprintf(v, sizeof(v), "%s · %s", s, q);
    SetCell(p.sky, "하늘", v, "", "", demo ? Badge::Demo : Badge::None);
  }
  // Hero weather icon (forecast slot; labelled with its source time).
  if (cfg_fc && snap.has_fc) {
    const auto& sk = snap.fc.sky;
    const auto& pt = snap.fc.pty;
    const int sky = sk.has_value && sk.validity != Validity::Missing ? sk.value : -1;
    const int pty = pt.has_value && pt.validity != Validity::Missing ? pt.value : -1;
    p.now_icon = fmt::WeatherIconFor(sky, pty);
    char at[16];
    fmt::KstTime(snap.fc.fcst_utc, now_utc, at, sizeof(at));
    snprintf(p.now_text, sizeof(p.now_text), "%s · 예보 %s",
             pty > 0 ? fmt::PtyText(pty) : fmt::SkyText(sky), at);
    p.forecast.icon = p.now_icon;
  } else {
    p.now_icon = ui::IconId::Unknown;
    snprintf(p.now_text, sizeof(p.now_text), "하늘 정보 없음");
    p.forecast.icon = ui::IconId::Unknown;
  }
  p.humidity.icon = ui::IconId::Humidity;
  p.rain.icon = ui::IconId::Umbrella;
  p.pm10.icon = p.pm25.icon = ui::IconId::Air;
  // Indoor (local sensor, no provider config gate)
  {
    if (snap.indoor_c.has_value || snap.indoor_rh.has_value) {
      char tv[48],hv[48];
      fmt::TempC(snap.indoor_c.value, !snap.indoor_c.has_value, tv, sizeof(tv));
      fmt::Percent((int)snap.indoor_rh.value, !snap.indoor_rh.has_value, hv, sizeof(hv));
      snprintf(v, sizeof(v), "%.22s / %.20s", tv, hv);
      const auto& status=st.providers[(int)sched::Provider::Sht40];
      Badge b=BadgeFor(snap.indoor_c.validity,true,snap.indoor_c.has_value,false);
      if (status.last_outcome != Outcome::Ok) b=Badge::LastGood;
      if (demo) b=Badge::Demo;
      fmt::KstTime(snap.indoor_c.observed_at.epoch_utc,now_utc,sub,sizeof(sub));
      SetCell(p.indoor,"실내",v,"",sub,b);
    } else if (st.sensor_present)
      SetCell(p.indoor, "실내", "--", "", "센서 대기",
              demo ? Badge::Demo : Badge::None);
    else
      SetCell(p.indoor, "실내", "--", "", "센서 없음",
              demo ? Badge::Demo : Badge::None);
  }
  p.indoor.icon = ui::IconId::Home;
  return p;
}

MarketPage BuildMarket(const app::Snapshot& snap, const app::AppState& st,
                       int64_t now_utc, bool clock_verified, bool wifi_ok,
                       const char* market_state, bool demo) {
  MarketPage p;
  FillHeader(p.header, st, now_utc, clock_verified, wifi_ok);
  p.demo = demo;
  strncpy(p.state, market_state ? market_state : "", sizeof(p.state) - 1);
  p.n = 0;
  char v[32], sub[64];
  // FX rows
  bool cfg_fx = app::ProviderConfigured(st.settings, sched::Provider::Exim);
  if (snap.has_fx && p.n < 4) {
    MarketRow& r = p.rows[p.n++];
    strncpy(r.label, "USD", sizeof(r.label) - 1);
    strncpy(r.pair, "KRW / USD", sizeof(r.pair) - 1);
    r.icon = ui::IconId::Usd;
    strncpy(r.kind, "고시", sizeof(r.kind) - 1);
    bool miss = !snap.fx.usd.has_value ||
                snap.fx.usd.validity == Validity::Missing;
    if (!cfg_fx) {
      strncpy(r.value, "--", sizeof(r.value) - 1);
      r.badge = Badge::NeedSetup;
    } else if (miss && snap.fx.usd.validity != Validity::Stale) {
      strncpy(r.value, "--", sizeof(r.value) - 1);
      r.badge = demo ? Badge::Demo : Badge::None;
    } else {
      fmt::FxRate(snap.fx.usd.value.won_scaled, snap.fx.usd.value.scale, v,
                  sizeof(v));
      strncpy(r.value, v, sizeof(r.value) - 1);
      r.badge = ProviderBadge(st, sched::Provider::Exim, snap.fx.usd.validity, true, snap.fx.usd.has_value);
      if (demo) r.badge = Badge::Demo;
    }
    fmt::FixingDay(snap.fx.fixing_date_utc, sub, sizeof(sub));
    strncpy(r.sub, sub, sizeof(r.sub) - 1);
  }
  if (snap.has_fx && p.n < 4) {
    MarketRow& r = p.rows[p.n++];
    strncpy(r.label, "JPY(100)", sizeof(r.label) - 1);
    strncpy(r.pair, "KRW / 100JPY", sizeof(r.pair) - 1);
    r.icon = ui::IconId::Jpy;
    strncpy(r.kind, "고시", sizeof(r.kind) - 1);
    bool miss = !snap.fx.jpy100.has_value ||
                snap.fx.jpy100.validity == Validity::Missing;
    if (!cfg_fx) {
      strncpy(r.value, "--", sizeof(r.value) - 1);
      r.badge = Badge::NeedSetup;
    } else if (miss && snap.fx.jpy100.validity != Validity::Stale) {
      strncpy(r.value, "--", sizeof(r.value) - 1);
      r.badge = demo ? Badge::Demo : Badge::None;
    } else {
      fmt::FxRate(snap.fx.jpy100.value.won_scaled,
                  snap.fx.jpy100.value.scale, v, sizeof(v));
      strncpy(r.value, v, sizeof(r.value) - 1);
      r.badge = ProviderBadge(st, sched::Provider::Exim, snap.fx.jpy100.validity,
                              true, snap.fx.jpy100.has_value);
      if (demo) r.badge = Badge::Demo;
    }
    fmt::FixingDay(snap.fx.fixing_date_utc, sub, sizeof(sub));
    strncpy(r.sub, sub, sizeof(r.sub) - 1);
  }
  p.fx_count=p.n;
  const bool cfg_kis = app::ProviderConfigured(st.settings, sched::Provider::KisPrice);
  const bool cfg_fsc = app::ProviderConfigured(st.settings, sched::Provider::FscClose);
  // Live/delayed REST and prior-close values retain distinct labels.
  for (size_t i = 0; i < st.settings.watch_count && i < kMaxWatchlist &&
                     p.n < kMaxWatchlist + 2; i++) {
    MarketRow& r = p.rows[p.n++];
    const char* sym = st.settings.watchlist[i].symbol;
    strncpy(r.label, sym, sizeof(r.label) - 1);
    if (const char* nm = stocks::NameForCode(sym))
      strncpy(r.name, nm, sizeof(r.name) - 1);
    r.icon = ui::IconId::Chart;
    const bool has_close = snap.has_fsc[i] && snap.fsc[i].quote.has_value;
    const bool has_live = snap.has_kis[i] && snap.kis[i].quote.has_value;
    const bool fresher_close = has_close && has_live &&
        snap.kis[i].quote.validity != Validity::Valid &&
        snap.fsc[i].quote.validity == Validity::Valid &&
        snap.fsc[i].quote.observed_at.epoch_utc > snap.kis[i].quote.observed_at.epoch_utc;
    if (has_live && !fresher_close) {
      strncpy(r.kind, "현재가", sizeof(r.kind) - 1);
      fmt::Krw(snap.kis[i].quote.value.price_krw, v, sizeof(v));
      strncpy(r.value, v, sizeof(r.value) - 1);
      fmt::KstTime(snap.kis[i].quote.value.price_at.epoch_utc, now_utc, sub,
                   sizeof(sub));
      char line[64];
      if (has_close) {
        // 금융위원회 종가를 현재가와 함께 보인다(기준일 명시: 실시간과 혼동 금지, AGENTS).
        char close[32];
        fmt::Krw(snap.fsc[i].quote.value.price_krw, close, sizeof(close));
        const KstBroken kc = BreakKst(snap.fsc[i].quote.value.price_at.epoch_utc);
        snprintf(line, sizeof(line), "%.8s · 종가 %s (%02d-%02d)", sub, close, kc.month, kc.day);
      } else {
        snprintf(line, sizeof(line), "%.14s 조회 · REST", sub);
      }
      strncpy(r.sub, line, sizeof(r.sub) - 1);
      if (snap.kis[i].quote.value.has_change)
        fmt::ChangePct(snap.kis[i].quote.value.change_bp, r.chg, sizeof(r.chg));
      r.badge = ProviderBadge(st, sched::Provider::KisPrice, snap.kis[i].quote.validity, true, true);
      if (r.badge == Badge::None && (st.symbol_fail_mask[(int)sched::Provider::KisPrice] & (1u << i)))
        r.badge = Badge::LastGood;  // 다종목 중 이 종목만 직전 패스에 실패한 경우(G-8)
    } else if (has_close) {
      strncpy(r.kind, "전일 종가", sizeof(r.kind) - 1);
      fmt::Krw(snap.fsc[i].quote.value.price_krw, v, sizeof(v));
      strncpy(r.value, v, sizeof(r.value) - 1);
      KstBroken k = BreakKst(snap.fsc[i].quote.value.price_at.epoch_utc);
      snprintf(sub, sizeof(sub), "기준일 %02d-%02d", k.month, k.day);
      strncpy(r.sub, sub, sizeof(r.sub) - 1);
      r.badge = ProviderBadge(st, sched::Provider::FscClose, snap.fsc[i].quote.validity, true, true);
      if (r.badge == Badge::None && (st.symbol_fail_mask[(int)sched::Provider::FscClose] & (1u << i)))
        r.badge = Badge::LastGood;
    } else {
      strncpy(r.kind, "전일 종가", sizeof(r.kind) - 1);
      strncpy(r.value, "--", sizeof(r.value) - 1);
      r.badge = demo ? Badge::Demo : ((cfg_kis || cfg_fsc) ? Badge::Check : Badge::NeedSetup);
      r.sub[0] = '\0';
    }
    if (demo) r.badge = Badge::Demo;
  }
  if (snap.has_kospi && snap.kospi.quote.has_value) {
    const IndexQuote& q = snap.kospi.quote.value;
    p.has_index = true;
    strncpy(p.index_label, "코스피", sizeof(p.index_label) - 1);
    fmt::Index2(q.value_x100, p.index_value, sizeof(p.index_value));
    if (q.has_change) fmt::ChangePct(q.change_bp, p.index_chg, sizeof(p.index_chg));
    fmt::KstTime(q.at.epoch_utc, now_utc, sub, sizeof(sub));
    snprintf(p.index_sub, sizeof(p.index_sub), "%.14s 조회 · REST", sub);
    p.index_badge = ProviderBadge(st, sched::Provider::KisPrice,
                                  snap.kospi.quote.validity, true, true);
    if (demo) p.index_badge = Badge::Demo;
  }
  return p;
}

namespace {
int DaysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return m == 2 && leap ? 29 : kDays[m - 1];
}
int64_t KstDayNumber(int y, int m, int d) { return KstToUtc(y, m, d, 0, 0) / 86400; }
const cal::YearTable* TableFor(const app::AppState& st, int year) {
  for (const cal::YearTable& t : st.calendars)
    if (t.year == year && t.loaded) return &t;
  return nullptr;
}
}  // namespace

CalendarPage BuildCalendar(const app::AppState& st, int64_t now_utc,
                           bool clock_verified, bool wifi_ok, bool demo) {
  static const char* const kWeekday[] = {"일요일", "월요일", "화요일", "수요일",
                                         "목요일", "금요일", "토요일"};
  CalendarPage p;
  FillHeader(p.header, st, now_utc, clock_verified, wifi_ok);
  p.demo = demo;
  strncpy(p.next_holiday, "공휴일 정보 없음", sizeof(p.next_holiday) - 1);
  strncpy(p.next_term, "절기 정보 없음", sizeof(p.next_term) - 1);
  if (!clock_verified) return p;  // never draw a "today" from an untrusted clock
  const KstBroken now = BreakKst(now_utc);
  p.valid = true;
  p.year = now.year;
  p.month = now.month;
  p.today = now.day;
  p.days = (uint8_t)DaysInMonth(now.year, now.month);
  p.first_wday = (uint8_t)BreakKst(KstToUtc(now.year, now.month, 1, 12, 0)).weekday;
  snprintf(p.title, sizeof(p.title), "%d년 %d월", now.year, now.month);
  snprintf(p.day_big, sizeof(p.day_big), "%d", now.day);
  strncpy(p.weekday, kWeekday[now.weekday % 7], sizeof(p.weekday) - 1);
  p.tables_ok = TableFor(st, now.year) != nullptr;

  // Month grid flags.
  const cal::YearTable* t = TableFor(st, now.year);
  for (int d = 1; d <= p.days; d++) {
    uint8_t f = 0;
    if (t && cal::IsHoliday(*t, now.month, d)) f |= kCalHoliday;
    if (t && cal::IsMarketClosed(*t, now.month, d)) f |= kCalClosed;
    p.flags[d] = f;
  }
  for (size_t i = 0; i < terms::Count(); i++) {
    const terms::Term& tm = terms::At(i);
    if (tm.year == now.year && tm.month == now.month && tm.day <= p.days)
      p.flags[tm.day] |= kCalTerm;
  }

  // Next public holiday (today counts), looking up to ~13 months ahead.
  const int64_t today_no = KstDayNumber(now.year, now.month, now.day);
  const int64_t noon_today = KstToUtc(now.year, now.month, now.day, 12, 0);
  if (p.tables_ok) {
    for (int off = 0; off <= 400; off++) {
      const KstBroken k = BreakKst(noon_today + (int64_t)off * 86400);  // KST noon, DST-free
      const cal::YearTable* kt = TableFor(st, k.year);
      if (!kt) break;  // unknown year: do not guess
      if (!cal::IsHoliday(*kt, k.month, k.day)) continue;
      const char* nm = holidays::FixedName(k.month, k.day);
      if (off == 0)
        snprintf(p.next_holiday, sizeof(p.next_holiday), "%02d-%02d %s · 오늘", k.month, k.day, nm ? nm : "공휴일");
      else
        snprintf(p.next_holiday, sizeof(p.next_holiday), "%02d-%02d %s · D-%d", k.month, k.day, nm ? nm : "공휴일", off);
      break;
    }
  }
  // Next solar term from the constant table.
  for (size_t i = 0; i < terms::Count(); i++) {
    const terms::Term& tm = terms::At(i);
    const int64_t no = KstDayNumber(tm.year, tm.month, tm.day);
    if (no < today_no) continue;
    if (no == today_no)
      snprintf(p.next_term, sizeof(p.next_term), "%02d-%02d %s · 오늘", tm.month, tm.day, terms::Name(tm.index));
    else
      snprintf(p.next_term, sizeof(p.next_term), "%02d-%02d %s · D-%d", tm.month, tm.day, terms::Name(tm.index), (int)(no - today_no));
    break;
  }
  return p;
}

DevicePage BuildDevice(const app::AppState& st, int64_t now_utc,
                       bool clock_verified, bool wifi_ok, int battery_mv,
                       const char* fw_version, bool usb, bool safe_mode) {
  DevicePage p;
  FillHeader(p.header, st, now_utc, clock_verified, wifi_ok);
  snprintf(p.wifi, sizeof(p.wifi), "%s",
           wifi_ok ? "Wi-Fi 연결됨" : "Wi-Fi 미연결");
  snprintf(p.clock, sizeof(p.clock), "%s",
           clock_verified ? "시계 NTP/RTC 정상" : "시계 시간 확인 필요");
  const bool low = !usb && battery_mv >= 0 && battery::IsLowPower(battery_mv);
  if (battery_mv < 0)
    snprintf(p.battery, sizeof(p.battery), "배터리 알 수 없음%s",
             usb ? " (USB)" : "");
  else
    snprintf(p.battery, sizeof(p.battery), "배터리 %d mV%s", battery_mv,
             usb ? " (USB)" : low ? " · 부족" : "");
  // The cycle planner forces this page in these modes; say why on the panel.
  if (safe_mode)
    snprintf(p.notice, sizeof(p.notice), "반복 오류로 안전 모드 · 갱신 중지");
  else if (low)
    snprintf(p.notice, sizeof(p.notice), "배터리 부족 · 충전하면 갱신 재개");
  snprintf(p.fw, sizeof(p.fw), "FW %s", fw_version ? fw_version : "?");
  snprintf(p.charging, sizeof(p.charging), "%s", charge::Label(st.charge_state));
  p.wifi_icon = wifi_ok ? ui::IconId::Wifi : ui::IconId::WifiOff;
  p.battery_icon = fmt::BatteryIconFor(battery_mv, usb);
  // Same provider names as the page footers ("출처 기상청 · 에어코리아").
  static const char* kNames[] = {"실내 센서", "기상청 실황", "기상청 예보",
                                 "에어코리아", "수출입은행", "금융위원회",
                                 "한국투자증권"};
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == (int)sched::Provider::Count,
                "provider names");
  for (int i = 0; i < (int)sched::Provider::Count; i++) {
    sched::Provider pr = (sched::Provider)i;
    const auto& ps = st.providers[(int)pr];
    char* out = p.providers[i];
    const size_t cap = sizeof(p.providers[i]);
    if (pr == sched::Provider::Sht40 && !st.sensor_present &&
        ps.last_success_utc <= 0) {
      snprintf(out, cap, "%s · 센서 없음", kNames[i]);
    } else if (!app::ProviderConfigured(st.settings, pr)) {
      snprintf(out, cap, "%s · %s", kNames[i], BadgeText(Badge::NeedSetup));
    } else if (ps.auth_blocked) {
      snprintf(out, cap, "%s · 키 확인 필요", kNames[i]);
    } else if (ps.last_success_utc > 0) {
      char t[16];
      fmt::KstTime(ps.last_success_utc, now_utc, t, sizeof(t));
      const bool failed = ps.last_attempt_utc > ps.last_success_utc &&
                          ps.last_outcome != Outcome::Ok;
      // 관심종목 일부만 실패(SymbolBatch): 몇 번째 종목인지 1부터 센 번호로 보여 준다.
      char partial[32] = {0};
      const uint8_t mask = st.symbol_fail_mask[i];
      if (!failed && mask) {
        size_t used = (size_t)snprintf(partial, sizeof(partial), " · 종목 ");
        bool first = true;
        for (int b = 0; b < (int)kMaxWatchlist && used < sizeof(partial); b++) {
          if (!(mask & (1u << b))) continue;
          used += (size_t)snprintf(partial + used, sizeof(partial) - used, "%s%d", first ? "" : ",", b + 1);
          first = false;
        }
        if (used < sizeof(partial)) snprintf(partial + used, sizeof(partial) - used, " 실패");
      }
      snprintf(out, cap, "%s · %s%s%s", kNames[i], t, failed ? " · 최근 실패" : "", partial);
    } else {
      snprintf(out, cap, "%s · 수신 기록 없음", kNames[i]);
    }
  }
  if (st.portal.open && st.portal.ip[0]) {
    snprintf(p.portal_url, sizeof(p.portal_url), "설정 http://%s", st.portal.ip);
    char name[48] = {0};  // mDNS 등록에 실패하면 주소만 안내한다
    if (st.portal.host[0]) snprintf(name, sizeof(name), "또는 %s.local · ", st.portal.host);
    if (st.portal.remaining_min < 0)
      snprintf(p.portal_host, sizeof(p.portal_host), "%s상시 대기", name);
    else
      snprintf(p.portal_host, sizeof(p.portal_host), "%s%d분 남음", name, st.portal.remaining_min);
    if (st.portal.locked)
      snprintf(p.portal_code, sizeof(p.portal_code), "잠김 · 5분 뒤 새 코드");
    else if (st.portal.code[0])
      snprintf(p.portal_code, sizeof(p.portal_code), "페어링 코드 %s", st.portal.code);
  }
  snprintf(p.hint, sizeof(p.hint), "왼쪽 ◀ 이전 화면 · 가운데 ▶ 다음 화면");
  snprintf(p.hint2, sizeof(p.hint2), "녹색 새로고침 · 녹색 2초 누르면 설정");
  return p;
}

}  // namespace vm
}  // namespace kdash
