#include "CalendarRefresh.h"

#include "../domain/Calendar.h"
#include "../domain/Freshness.h"
#include "../net/CaBundle.h"
#include "../providers/HolidayService.h"
#include "../storage/CalendarStore.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include "../boot/SerialLog.h"
#endif
#if !defined(ARDUINO) && !defined(RTC_NOINIT_ATTR)
#define RTC_NOINIT_ATTR
#endif

namespace kdash {
namespace app {
namespace {
// Deep-sleep을 넘어 유지되는 재시도 상태(설정 변경 시 초기화).
constexpr uint32_t kRetryMagic=0x43414C32u;
RTC_NOINIT_ATTR uint32_t g_retry_magic;
RTC_NOINIT_ATTR int64_t g_calendar_retry[2];
RTC_NOINIT_ATTR uint8_t g_calendar_blocked[2];
}  // namespace

void ResetCalendarRetry() {
  g_retry_magic=kRetryMagic;
  for (int i = 0; i < 2; ++i) {
    g_calendar_retry[i] = 0;
    g_calendar_blocked[i] = false;
  }
}

void ClearCalendarAuthBlocks() {
  if (g_retry_magic != kRetryMagic) { ResetCalendarRetry(); return; }
  for (int i = 0; i < 2; ++i) {
    if (!g_calendar_blocked[i]) continue;
    g_calendar_blocked[i] = false;
    g_calendar_retry[i] = 0;
  }
}

void RefreshCalendar(AppState& state, IHttpClient& http, IStorage& storage,
                     const FetchContext& ctx, IFetchControl& budget) {
  if (g_retry_magic != kRetryMagic) ResetCalendarRetry();
  if (!state.settings.providers.kma_key[0]) return;
  const int year = BreakKst(ctx.now_utc).year;
  if (state.calendars[0].year != year) {
    cal::AlignYears(state.calendars, year);
    for (int i = 0; i < 2; i++) { g_calendar_retry[i] = 0; g_calendar_blocked[i] = false; }
  }
  for (int i = 0; i < 2 && budget.CanRequest(); ++i) {
    cal::YearTable& table = state.calendars[i];
    if (table.year != year + i) {
      cal::Clear(table, year + i);
      g_calendar_retry[i] = 0;
      g_calendar_blocked[i] = false;
    }
    // A corrupt retained deadline must not suppress requests indefinitely.
    if (g_calendar_retry[i] > ctx.now_utc + 28LL * 86400) {
      g_calendar_retry[i]=0; g_calendar_blocked[i]=false;
    }
    if (g_calendar_retry[i] > ctx.now_utc) continue;
    if (table.loaded && ctx.now_utc - table.fetched_utc < 28LL * 86400) continue;
    const Outcome result = holidays::FetchHolidayYear(http,
        state.settings.providers.kma_key, net::kCaBundlePem, year + i, ctx, table);
    if (result == Outcome::Ok) {
      if (!cal::SaveTables(storage, state.calendars, ctx.now_utc))
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
        KDASH_LOG.println("calendar: persistence FAILED");
#else
        ;
#endif
      g_calendar_blocked[i] = false;
      g_calendar_retry[i] = ctx.now_utc + 28LL * 86400;
    } else {
      // 인증 오류도 영구 중단 대신 하루 1회 재시도하고, 설정 재저장 시 ClearCalendarAuthBlocks로 즉시 푼다(ISSUE-012).
      g_calendar_blocked[i] = result == Outcome::AuthError;
      const bool daily = result == Outcome::AuthError || result == Outcome::QuotaExceeded ||
                         (i == 1 && result == Outcome::NoData);
      g_calendar_retry[i] = ctx.now_utc + (daily ? 86400 : 3600);
    }
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
    KDASH_LOG.printf("calendar: year=%d outcome=%s\n", year + i, ToString(result));
#else
    (void)result;
    (void)year;
    (void)i;
#endif
  }
}

}  // namespace app
}  // namespace kdash
