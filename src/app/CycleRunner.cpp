#include "CyclePlatform.h"
#include "CalendarRefresh.h"
#include "../domain/Calendar.h"
#include "../net/CaBundle.h"

namespace kdash { namespace app {
const char* MarketStateText(int64_t now, const AppState& state) {
  return cal::MarketStateText(now, state.time_verified, state.calendars, 2);
}
namespace {
constexpr uint32_t kCycleWorkMs = 60000;
class CycleBudget final : public IFetchControl {
 public:
  CycleBudget(ICyclePlatform& p, const uint32_t& start) : p_(p), start_(start) {}
  bool CanRequest() const override { return (uint32_t)(p_.Millis() - start_) < kCycleWorkMs; }
 private:
  ICyclePlatform& p_;
  const uint32_t& start_;
};
}
void RunCycle(power::CycleWake reason, ButtonEvent event, AppState& state,
              Snapshot& snap, KisSession& kis, IHttpClient* http,
              IStorage& storage, int jitter_slot, const char* fw_version, ICyclePlatform& platform) {
  uint32_t cycle_start = platform.Millis();
  CycleBudget budget(platform, cycle_start);
  const bool usb = platform.Usb();
  const int battery_mv = platform.BatteryMv();
  platform.SyncClock(false, state);  // trusted RTC is applied before planning/network
  int64_t initial_now = platform.Utc();
  power::CycleInput input;
  input.reason = reason;
  input.st = &state;
  input.now_utc = initial_now;
  input.clock_verified = state.time_verified;
  input.usb = usb;
  input.battery_mv = usb ? -1 : battery_mv;
  input.crashes = (int)platform.Crashes();
  input.next_due_utc = app::NextNetworkWakeUtc(state, initial_now, usb);
  input.button_mask = event == ButtonEvent::ShortKey1 ? 2 :
                      event == ButtonEvent::ShortKey2 ? 4 :
                      event == ButtonEvent::None ? 0 : 1;
  input.long_press = event == ButtonEvent::LongKey0;
  power::CyclePlan plan = power::PlanCycle(input);
  state.page = plan.page;
  if (plan.safe_mode || plan.low_battery_mode) state.page = ui::Page::Device;
  if (plan.enter_portal && !plan.low_battery_mode && !plan.safe_mode) {
    platform.OpenPortal(state, storage);
    cycle_start = platform.Millis();  // interactive setup has its own 10-minute limit
    initial_now = platform.Utc();
  }

  // Probe before RF/display heating, including absent-at-boot recovery.
  float temp = 0, rh = 0;
  bool sampled = false, sample_ok = false;
  const auto& sensor = state.providers[(int)sched::Provider::Sht40];
  const int64_t sensor_interval = usb ? 300 : 900;
  if (!plan.safe_mode && !plan.low_battery_mode &&
      (sensor.last_attempt_utc == 0 || initial_now - sensor.last_attempt_utc >= sensor_interval)) {
    sampled = true;
    sample_ok = platform.ReadIndoor(temp, rh);
  }
  const bool need_network = !plan.safe_mode && !plan.low_battery_mode &&
      (plan.connect_wifi || plan.sync_clock);
  bool wifi = platform.Wifi();
  if (need_network && budget.CanRequest()) {
    // USB reconnects in MaintainWifi without blocking the button/portal loop.
    // Keep explicit refresh and battery wake connection behavior.
    if (!(usb && plan.sync_clock && !plan.connect_wifi &&
          reason != power::CycleWake::Button))
      wifi = platform.ConnectWifi(state.settings);
    platform.SyncClock(wifi, state);  // NTP is allowed only after Wi-Fi, separately from HTTPS
    if (!wifi && state.time_verified)
      app::MarkNetworkUnavailable(state, platform.Utc(), usb);
  }
  platform.Heartbeat();
  FetchContext ctx;
  ctx.fetched_utc = ctx.now_utc = platform.Utc();
  ctx.clock_verified = state.time_verified;
  if (sampled) app::RecordIndoor(state, snap, sample_ok, temp, rh, ctx, usb);
  if (need_network && wifi && ctx.clock_verified && http && http->ready() && budget.CanRequest()) {
    http->SetDeadline(cycle_start + kCycleWorkMs);
    app::PassLog log;
    app::RunFetchPass(state, snap, kis, *http, net::kCaBundlePem,
                      ctx, usb, jitter_slot, &log, &budget, plan.force_refresh);
    platform.LogPass(log, snap);
    RefreshCalendar(state, *http, storage, ctx, budget);
  }
  platform.Heartbeat();
  ctx.now_utc = ctx.fetched_utc = platform.Utc();
  app::RefreshFreshness(snap, ctx);
  // Rendering has its own bounded panel deadline. A failed refresh never advances CRC.
  if (plan.render || sampled || !platform.HasRender() || reason == power::CycleWake::FirstBoot ||
      state.display_failed || state.redraw_requested)
    state.display_failed = !platform.Render(state, snap, battery_mv, fw_version,
                       (int)platform.Crashes(),
                       plan.force_refresh || reason == power::CycleWake::Button);
  state.redraw_requested = false;
  if (state.time_verified)
    platform.Checkpoint(storage, state, snap, ctx.now_utc);
  if (!plan.safe_mode) platform.ClearCrashes();
  platform.SetPending(false);
  platform.Retain(snap, state, kis);
  platform.Heartbeat();
  if (!usb) {
    input.reason = power::CycleWake::Timer; input.button_mask = 0; input.long_press = false;
    input.now_utc = ctx.now_utc; input.clock_verified = state.time_verified;
    input.next_due_utc = app::NextWakeUtc(state, ctx.now_utc, false);
    // Missing local sensor still gets a periodic probe rather than sleeping forever.
    const int64_t sensor_next = state.providers[(int)sched::Provider::Sht40].last_attempt_utc + 900;
    if (sensor_next < input.next_due_utc) input.next_due_utc = sensor_next;
    const auto next = power::PlanCycle(input);
    platform.Sleep(next.sleep_seconds, snap, state, kis);
  }
}

} }
