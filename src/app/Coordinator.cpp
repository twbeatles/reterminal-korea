#include "Coordinator.h"

#include <stdint.h>

#include "CalendarRefresh.h"
#include "../domain/Freshness.h"
#include "../providers/FinanceParsers.h"
#include "../providers/KmaParser.h"

namespace kdash {
namespace app {
namespace {

constexpr int64_t kNever = INT64_MAX;

using sched::Provider;

int64_t Max(int64_t a, int64_t b) { return a > b ? a : b; }

// Next KST midnight after now (data.go.kr / EXIM daily quotas reset then).
int64_t NextKstMidnight(int64_t now_utc) {
  KstBroken k = BreakKst(now_utc);
  return KstToUtc(k.year, k.month, k.day, 24, 0);
}

// Close (15:30 KST) of the most recent KRX trading day that has already ended at
// now_utc; 0 if none within ~10 days. Unknown calendar years count weekdays as trading.
int64_t LastKrxCloseUtc(const AppState& st, int64_t now_utc) {
  for (int d = 0; d <= 10; d++) {
    const KstBroken k = BreakKst(now_utc - (int64_t)d * 86400);
    const int64_t close = KstToUtc(k.year, k.month, k.day, 15, 30);
    if (close > now_utc) continue;
    const cal::DayKind kind = cal::ClassifyKstDay(close, st.calendars, 2);
    if (kind == cal::DayKind::Weekend || kind == cal::DayKind::PublicHoliday ||
        kind == cal::DayKind::MarketClosed)
      continue;
    return close;
  }
  return 0;
}

// First minute >= now at which the KRX regular session is open (weekday
// 09:00 KST), respecting the loaded public-holiday/market-closure calendar.
int64_t NextKrxOpen(const AppState& st, int64_t now_utc) {
  if (cal::IsKrxOpen(now_utc, st.calendars, 2)) return now_utc;
  for (int d = 0; d < 366; d++) {
    KstBroken k = BreakKst(now_utc + (int64_t)d * 86400);
    int64_t open = KstToUtc(k.year, k.month, k.day, 9, 0);
    if (open >= now_utc && cal::IsKrxOpen(open, st.calendars, 2)) return open;
  }
  return kNever;
}

// First time >= now at which FscDue() becomes true.
bool FscWindow(const AppState& st, int64_t t, int64_t last_success) {
  const cal::DayKind day = cal::ClassifyKstDay(t, st.calendars, 2);
  return day != cal::DayKind::PublicHoliday && day != cal::DayKind::MarketClosed &&
         fin::FscDue(t, last_success);
}

int64_t NextFscWindow(const AppState& st, int64_t now_utc, int64_t last_success_utc) {
  if (FscWindow(st, now_utc, last_success_utc)) return now_utc;
  for (int d = 0; d < 366; d++) {
    KstBroken k = BreakKst(now_utc + (int64_t)d * 86400);
    int64_t t = KstToUtc(k.year, k.month, k.day, 13, 0);
    if (t >= now_utc && FscWindow(st, t, last_success_utc)) return t;
  }
  return kNever;
}

// Release-driven: time the next not-yet-fetched 회차 becomes requestable.
int64_t NextKmaRelease(Provider p, int64_t now_utc, int64_t last_base_utc) {
  const bool now_kind = p == Provider::KmaNow;
  auto base_at = [&](int64_t t) {
    return now_kind ? kma::UltraNowBaseUtc(t) : kma::VillageBaseUtc(t);
  };
  if (base_at(now_utc) > last_base_utc) return now_utc;
  // Step forward in 10-minute increments (bounded: <= 1 day of steps).
  for (int64_t t = now_utc + 600; t <= now_utc + 86400; t += 600) {
    if (base_at(t) > last_base_utc) {
      // Align to the exact minute the base flips within this 10-min window.
      for (int64_t u = t - 600; u <= t; u += 60) {
        if (base_at(u) > last_base_utc) return u;
      }
      return t;
    }
  }
  return kNever;
}

}  // namespace

bool ProviderConfigured(const Settings& s, Provider p) {
  const ProviderKeys& k = s.providers;
  switch (p) {
    case Provider::Sht40:
      return true;
    case Provider::KmaNow:
    case Provider::KmaForecast:
      return k.kma_key[0] != '\0';
    case Provider::AirKorea:
      return k.air_key[0] != '\0' && s.location.air_station[0] != '\0';
    case Provider::Exim:
      return k.exim_key[0] != '\0';
    case Provider::FscClose:  // data.go.kr key shared with KMA (같은 계정 키)
      return k.fsc_enabled && k.kma_key[0] != '\0' && s.watch_count > 0;
    case Provider::KisPrice:
      return k.kis_enabled && k.kis_app_key[0] != '\0' &&
             k.kis_app_secret[0] != '\0' && s.watch_count > 0;
    default:
      return false;
  }
}

int64_t NextAttemptUtc(const AppState& st, Provider p, int64_t now_utc,
                       bool usb) {
  if (!ProviderConfigured(st.settings, p)) return kNever;
  if (p == Provider::Sht40 && !st.sensor_present) return kNever;
  const ProviderStatus& s = st.providers[(int)p];
  // auth_blocked: MarkResult set retry_after_utc = +24h; the gate below holds it (ADR 0026).
  int64_t t;
  switch (p) {
    case Provider::KmaNow:
    case Provider::KmaForecast:
      t = NextKmaRelease(p, now_utc, s.last_base_utc);
      break;
    case Provider::KisPrice: {
      sched::Intervals iv = sched::BaseInterval(p);
      int64_t by_interval = sched::NextDue(
          s.last_success_utc,
          sched::ScaleInterval(usb ? iv.usb_sec : iv.battery_sec,
                               st.settings.schedule.refresh_scale_pct));
      t = NextKrxOpen(st, Max(now_utc, by_interval));  // 장외 호출 금지
      // 예외(마감 확정값 1회): 마지막 성공이 직전 마감 이전이면 장외라도 한 번 받는다.
      // 성공 이력이 없어도(설정 저장·재플래시로 캐시가 비었을 때) 적용한다(ADR 0026, ISSUE-008).
      // 마감 이후의 시도가 재시도 예산을 다 쓰면(attempts가 0으로 돌아감) 다음 마감까지 멈춘다.
      // Retry-After·백오프는 아래에서 지킨다.
      if (!cal::IsKrxOpen(now_utc, st.calendars, 2)) {
        const int64_t close = LastKrxCloseUtc(st, now_utc);
        if (close > 0 && s.last_success_utc < close &&
            (s.last_attempt_utc < close || s.attempts > 0))
          t = now_utc;
      }
      break;
    }
    case Provider::FscClose:
      t = NextFscWindow(st, now_utc, s.last_success_utc);
      break;
    default: {
      sched::Intervals iv = sched::BaseInterval(p);
      const int64_t base = usb ? iv.usb_sec : iv.battery_sec;
      // 로컬 센서(SHT40)는 네트워크 호출이 아니므로 배율 대상이 아니다.
      t = sched::NextDue(s.last_success_utc,
                         p == Provider::Sht40
                             ? base
                             : sched::ScaleInterval(base, st.settings.schedule.refresh_scale_pct));
      break;
    }
  }
  if (t == kNever) return kNever;
  return Max(t, s.retry_after_utc);  // failures never bypass backoff
}

int DueProviders(const AppState& st, int64_t now_utc, bool usb, Provider* out,
                 int cap, bool force) {
  int n = 0;
  for (int i = 0; i < (int)Provider::Count && n < cap; i++) {
    auto p = (Provider)i;
    if (force && ProviderConfigured(st.settings, p) &&
        !st.providers[i].auth_blocked && st.providers[i].retry_after_utc <= now_utc &&
        now_utc - st.providers[i].last_attempt_utc >= kForceMinGapSec) {
      // A button bypasses normal freshness intervals, never release/market,
      // auth, Retry-After or quota gates. Last-success metadata stays intact.
      bool allowed = p != Provider::Sht40 || st.sensor_present;
      if (p == Provider::KisPrice)
        allowed = cal::IsKrxOpen(now_utc, st.calendars, 2);
      if (p == Provider::FscClose) allowed = FscWindow(st, now_utc, 0);
      if (allowed) { out[n++] = p; continue; }
    }
    if (NextAttemptUtc(st, p, now_utc, usb) <= now_utc) out[n++] = p;
  }
  return n;
}

void MarkResult(AppState& st, Provider p, Outcome outcome, int64_t now_utc,
                bool usb, int retry_after_sec, int jitter_slot_ms,
                int64_t base_utc) {
  ProviderStatus& s = st.providers[(int)p];
  s.last_attempt_utc = now_utc;
  s.last_outcome = outcome;
  if (outcome == Outcome::Ok) {
    s.attempts = 0;
    s.retry_after_utc = 0;
    s.auth_blocked = false;  // a later success proves the key works again
    s.last_success_utc = now_utc;
    if (base_utc > 0) s.last_base_utc = base_utc;
    st.last_fetch_utc = now_utc;
    return;
  }
  s.attempts++;
  switch (outcome) {
    case Outcome::AuthError:
      // 키 확인 필요 표시 + 하루 1회만 재시도(일시적 401/403·키 동기화 지연 회복, ADR 0026).
      // 수동 새로고침은 여전히 차단된 공급자를 부르지 않는다(DueProviders).
      s.auth_blocked = true;
      s.retry_after_utc = now_utc + kAuthRetrySec;
      return;
    case Outcome::QuotaExceeded:
      s.retry_after_utc = NextKstMidnight(now_utc) + 600;
      return;
    case Outcome::RateLimited:
      s.retry_after_utc = now_utc + sched::RateLimitDelaySec(
                                        retry_after_sec, s.attempts - 1,
                                        jitter_slot_ms);
      return;
    default:
      break;
  }
  if (sched::RetryAllowed(p, s.attempts, false)) {
    s.retry_after_utc =
        now_utc + sched::BackoffSec(s.attempts - 1, jitter_slot_ms);
  } else {
    // Retry budget spent: wait one normal interval, then start over.
    sched::Intervals iv = sched::BaseInterval(p);
    s.retry_after_utc = now_utc + (usb ? iv.usb_sec : iv.battery_sec);
    s.attempts = 0;
  }
}

void ClearAuthBlocks(AppState& st) {
  for (ProviderStatus& s : st.providers) {
    if (!s.auth_blocked) continue;  // other backoff (quota/429) stays in force
    s.auth_blocked = false;
    s.retry_after_utc = 0;
    s.attempts = 0;
  }
  ClearCalendarAuthBlocks();
}

int64_t NextWakeUtc(const AppState& st, int64_t now_utc, bool usb) {
  int64_t best = kNever;
  for (int i = 0; i < (int)Provider::Count; i++) {
    int64_t t = NextAttemptUtc(st, (Provider)i, now_utc, usb);
    if (t < best) best = t;
  }
  return best;
}

ui::Page NextPage(ui::Page p) {
  return (ui::Page)(((int)p + 1) % (int)ui::Page::Count);
}

int64_t NextNetworkWakeUtc(const AppState& st, int64_t now_utc, bool usb) {
  int64_t best = kNever;
  for (int i = (int)Provider::KmaNow; i < (int)Provider::Count; ++i) {
    const int64_t t = NextAttemptUtc(st, (Provider)i, now_utc, usb);
    if (t < best) best = t;
  }
  return best;
}

void MarkNetworkUnavailable(AppState& st, int64_t now_utc, bool usb) {
  Provider due[(int)Provider::Count];
  const int n = DueProviders(st, now_utc, usb, due, (int)Provider::Count);
  for (int i=0; i<n; ++i)
    if (due[i] != Provider::Sht40)
      MarkResult(st, due[i], Outcome::ProviderError, now_utc, usb, -1, 0);
}

ui::Page PrevPage(ui::Page p) {
  return (ui::Page)(((int)p + (int)ui::Page::Count - 1) %
                    (int)ui::Page::Count);
}

}  // namespace app
}  // namespace kdash
