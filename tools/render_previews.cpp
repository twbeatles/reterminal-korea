// Renders the T04c 10-scenario set to PBM files (host preview, human review).
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../src/app/Coordinator.h"
#include "../src/app/DemoProvider.h"
#include "../src/domain/Freshness.h"
#include "../src/storage/SettingsStore.h"
#include "../src/ui/Canvas.h"
#include "../src/ui/Renderer.h"
#include "../src/ui/ViewModel.h"

namespace {
using namespace kdash;
const int64_t kNow = 1790000000LL;  // a Wednesday afternoon (KST)

void Base(app::AppState& st, app::Snapshot& snap) {
  st.settings = settings::DefaultSettings();
  st.charge_state = charge::State::Off;
  settings::CopyBounded(st.settings.location.display_name,
                        sizeof(st.settings.location.display_name), "서울 종로구");
  settings::CopyBounded(st.settings.wifi_ssid, sizeof(st.settings.wifi_ssid), "HomeAP");
  settings::CopyBounded(st.settings.providers.kma_key,
                        sizeof(st.settings.providers.kma_key), "k");
  settings::CopyBounded(st.settings.providers.air_key,
                        sizeof(st.settings.providers.air_key), "k");
  settings::CopyBounded(st.settings.providers.exim_key,
                        sizeof(st.settings.providers.exim_key), "k");
  demo::FillDemo(snap, kNow - 600);
}
void Save(ui::Canvas& c, const char* name) {
  // Every exported fixture screen remains visibly synthetic, including the
  // scenarios that deliberately exercise non-demo badge decisions.
  c.draw_text(ui::DefaultSmall(),700,450,"DEMO");
  char path[128];
  snprintf(path, sizeof(path), ".pio/preview/%s.pbm", name);
  printf("%s margin=%d\n", path, (int)ui::CheckMarginsClean(c));
  if (!ui::CheckMarginsClean(c)) std::exit(1);
  c.write_pbm(path);
  snprintf(path, sizeof(path), ".pio/preview/%s.pgm", name);
  c.write_pgm(path);
}
}  // namespace

int main() {
  using namespace kdash;
  // 1 Day (all valid)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap, st, kNow, true, true, false), c, nullptr);
    Save(c, "life-day");
  }
  // 2 Night market (closed state)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderMarket(
        vm::BuildMarket(snap, st, kNow, true, true, "장마감", false), c, nullptr);
    Save(c, "market-night");
  }
  // 3 Weekend (known closure)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderMarket(
        vm::BuildMarket(snap, st, kNow, true, true, "휴장", false), c,
        nullptr);
    Save(c, "market-weekend");
  }
  // Public calendar loaded, exchange calendar unverified: regular hours only.
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    const int64_t now = KstToUtc(2026, 11, 17, 10, 0);
    cal::Clear(st.calendars[0], 2026);
    st.calendars[0].loaded = true;
    ui::Canvas c;
    ui::RenderMarket(vm::BuildMarket(snap, st, now, true, true,
        cal::MarketStateText(now, true, st.calendars, 2), true), c, nullptr);
    Save(c, "market-regular-hours");
    ui::RenderCalendar(vm::BuildCalendar(st, now, true, true, true), c, nullptr);
    Save(c, "calendar-business-day");
  }
  // 4 NoNetwork (wifi off)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderDevice(vm::BuildDevice(st, kNow, true, false, 3800, "0.4.0", true),
                     c, nullptr);
    Save(c, "device-nonet");
  }
  // 5 NoData (empty snapshot)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    snap = app::Snapshot();
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap, st, kNow, true, true, false), c, nullptr);
    Save(c, "life-nodata");
  }
  // 6 LongKorean (21-char district)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    settings::CopyBounded(st.settings.location.display_name,
                          sizeof(st.settings.location.display_name),
                          "세종특별자치시 어진동 한누리대로");
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap, st, kNow, true, true, false), c, nullptr);
    Save(c, "life-longkr");
  }
  // 7 WrongTime (unverified clock)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap, st, kNow, false, true, false), c, nullptr);
    Save(c, "life-badtime");
  }
  // 8 LowBattery
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderDevice(
        vm::BuildDevice(st, kNow, true, true, 3200, "0.4.0", false), c, nullptr);
    Save(c, "device-lowbat");
  }
  // 9 KisDisabled (no watchlist rows)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    st.settings.watch_count = 0;
    ui::Canvas c;
    ui::RenderMarket(vm::BuildMarket(snap, st, kNow, true, true, "장중", false),
                     c, nullptr);
    Save(c, "market-nokis");
  }
  // 10 Demo
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap, st, kNow, true, false, true), c, nullptr);
    Save(c, "life-demo");
  }
  // 11 Watchlist (prior-close rows + one stale row; ADR 0008 card layout)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    const char* syms[] = {"005930", "000660", "035420", "051910"};
    st.settings.watch_count = kMaxWatchlist;
    for (size_t i = 0; i < kMaxWatchlist; i++) {
      snprintf(st.settings.watchlist[i].symbol, 7, "%s", syms[i % 4]);
      snap.has_fsc[i] = true;
      snap.fsc[i].quote.has_value = true;
      snap.fsc[i].quote.validity = i == 3 ? Validity::Stale : Validity::Valid;
      snap.fsc[i].quote.value.price_krw = 78500 + 1250 * (int64_t)i;
      snap.fsc[i].quote.value.price_at.epoch_utc = kNow - 86400;
    }
    ui::Canvas c;
    ui::RenderMarket(vm::BuildMarket(snap, st, kNow, true, true, "장마감", false),
                     c, nullptr);
    Save(c, "market-watchlist");
  }
  // KIS live quotes with day-over-day change (+/-/0.00%)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    const char* syms[] = {"005930", "000660", "035420", "005380"};
    const int bp[] = {123, -245, 0, -51};
    st.settings.watch_count = kMaxWatchlist;
    for (size_t i = 0; i < kMaxWatchlist; i++) {
      snprintf(st.settings.watchlist[i].symbol, 7, "%s", syms[i]);
      snap.has_kis[i] = true;
      snap.kis[i].quote.has_value = true;
      snap.kis[i].quote.validity = Validity::Valid;
      snap.kis[i].quote.value.price_krw = 71000 + 98000 * (int64_t)i;
      snap.kis[i].quote.value.price_at.epoch_utc = kNow;
      snap.kis[i].quote.value.has_change = true;
      snap.kis[i].quote.value.change_bp = bp[i];
    }
    snap.has_kospi = true;
    snap.kospi.quote.has_value = true;
    snap.kospi.quote.validity = Validity::Valid;
    snap.kospi.quote.value.value_x100 = 537168;
    snap.kospi.quote.value.has_change = true;
    snap.kospi.quote.value.change_bp = 253;
    snap.kospi.quote.value.at.epoch_utc = kNow;
    ui::Canvas c;
    ui::RenderMarket(vm::BuildMarket(snap, st, kNow, true, true, "장중", false),
                     c, nullptr);
    Save(c, "market-live-change");
  }
  // Calendar: month grid with holidays, market closure, solar terms and today
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    const int year = BreakKst(kNow).year;
    cal::Clear(st.calendars[0], year);
    cal::Clear(st.calendars[1], year + 1);
    st.calendars[0].loaded = st.calendars[1].loaded = true;
    cal::SetHoliday(st.calendars[0], 9, 24);
    cal::SetHoliday(st.calendars[0], 9, 25);
    cal::SetHoliday(st.calendars[0], 9, 26);
    cal::SetHoliday(st.calendars[0], 10, 3);
    cal::SetHoliday(st.calendars[0], 10, 9);
    cal::SetHoliday(st.calendars[0], 12, 25);
    cal::SetMarketClosed(st.calendars[0], 9, 30);
    ui::Canvas c;
    ui::RenderCalendar(vm::BuildCalendar(st, kNow, true, true, true), c, nullptr);
    Save(c, "calendar-month");
    ui::Canvas c2;
    ui::RenderCalendar(vm::BuildCalendar(st, kNow, false, false, false), c2, nullptr);
    Save(c2, "calendar-no-clock");
  }
  // 12 SafeMode (3+ crashes force the device page; providers with history)
  {
    app::AppState st;
    app::Snapshot snap;
    Base(st, snap);
    st.sensor_present = true;
    for (int i = 0; i < (int)sched::Provider::Count; i++)
      st.providers[i].last_success_utc = kNow - 600;
    st.providers[(int)sched::Provider::AirKorea].last_attempt_utc = kNow - 60;
    st.providers[(int)sched::Provider::AirKorea].last_outcome = Outcome::ProviderError;
    st.providers[(int)sched::Provider::Exim].auth_blocked = true;
    ui::Canvas c;
    ui::RenderDevice(
        vm::BuildDevice(st, kNow, true, true, 3900, "0.4.0", false, true), c,
        nullptr);
    Save(c, "device-safemode");
  }
  {
    app::AppState st; app::Snapshot snap; Base(st,snap);
    snap.air.pm10.observed_at.epoch_utc -= 3600;
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap,st,kNow,true,true,false),c,nullptr);
    Save(c,"life-mixed-air-time");
  }
  {
    app::AppState st; app::Snapshot snap; Base(st,snap);
    const auto p=sched::Provider::KmaNow;
    st.providers[(int)p].last_success_utc=kNow-600;
    st.providers[(int)p].last_attempt_utc=kNow;
    st.providers[(int)p].last_outcome=Outcome::ProviderError;
    ui::Canvas c;
    ui::RenderLife(vm::BuildLife(snap,st,kNow,true,true,false),c,nullptr);
    Save(c,"life-provider-failure");
  }
  {
    app::AppState st; app::Snapshot snap; Base(st,snap);
    st.settings.device.charge_protect=true;
    st.charge_state=charge::State::Limited;
    ui::Canvas c;
    ui::RenderDevice(vm::BuildDevice(st,kNow,true,true,4100,"1.0.0",true),c,nullptr);
    Save(c,"device-charge-limited");
    st.portal.open=true;
    snprintf(st.portal.ip,sizeof(st.portal.ip),"192.168.0.10");
    snprintf(st.portal.code,sizeof(st.portal.code),"123456");  // synthetic DEMO code
    st.portal.remaining_min=10;
    ui::Canvas cp;
    ui::RenderDevice(vm::BuildDevice(st,kNow,true,true,4100,"1.0.0",true),cp,nullptr);
    Save(cp,"device-charge-portal");
  }
  return 0;
}
