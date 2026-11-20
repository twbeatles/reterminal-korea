#pragma once
// Application pipeline (docs/02):
// BOOT -> LOAD_CONFIG -> (NO_WIFI? SETUP_PORTAL : WIFI_CONNECT)
// -> SYNC_UTC -> DETERMINE_DUE -> FETCH_WITH_BUDGET -> VALIDATE -> NORMALIZE
// -> CACHE_ATOMIC -> COMPARE_SNAPSHOT -> RENDER_CHANGED -> PLAN_NEXT_WAKE
// -> (USB? IDLE : DEEP_SLEEP)
// One provider failure never fails the whole screen; every network request
// has one timeout, a size cap, and a retry budget; no infinite loops.
#include <stdint.h>

#include "../../include/Config.h"
#include "../domain/Types.h"
#include "../domain/Calendar.h"
#include "../power/PowerManager.h"
#include "../power/ChargeProtection.h"
#include "../power/Schedule.h"
#include "../ui/Layout.h"
#include "../ui/Pages.h"

namespace kdash {
namespace app {

// 수동 새로고침(KEY2)이 같은 공급자를 다시 호출하기 전 최소 간격(초).
inline constexpr int64_t kForceMinGapSec = 120;
// 인증 오류 뒤 자동 재시도 간격(초). 영구 차단 대신 하루 1회(ADR 0026, 감사 G-1).
inline constexpr int64_t kAuthRetrySec = 24 * 3600;

struct ProviderStatus {
  int64_t last_success_utc = 0;
  int64_t last_attempt_utc = 0;
  int attempts = 0;               // consecutive failed attempts
  Outcome last_outcome = Outcome::Ok;
  int64_t retry_after_utc = 0;    // no attempt before this (backoff/429/quota)
  bool auth_blocked = false;      // AuthError: "키 확인 필요", retried once per kAuthRetrySec
  int64_t last_base_utc = 0;      // KMA 회차 last fetched successfully
};

// 설정 포털(LAN) 표시 상태. 저장하지 않는 런타임 값. 기기 탭이 읽는다.
struct PortalStatus {
  bool open = false;
  char ip[16] = {0};        // "192.168.0.23"
  char host[24] = {0};      // "kdash-3f2a" (".local" 제외)
  char code[8] = {0};       // 페어링 코드(화면 전용). 잠금 중이면 ""
  bool locked = false;
  int remaining_min = -1;   // 남은 분. -1 = 상시(만료 없음)
  ui::Page return_page = ui::Page::Life;  // 포털이 닫히면 돌아갈 화면
};

struct AppState {
  Settings settings;
  ProviderStatus providers[(int)sched::Provider::Count];
  int64_t last_render_utc = 0;
  ui::Page page = ui::Page::Life;
  bool time_verified = false;
  bool sensor_present = false;  // SHT40 driver attached (board adapter)
  bool display_failed = false;  // transient: force retry without advancing render CRC
  bool redraw_requested = false;  // transient: redraw once (e.g. Wi-Fi icon changed); CRC check still applies
  charge::State charge_state = charge::State::Unknown;  // transient; never restored as confirmed
  int64_t last_fetch_utc = 0;
  PortalStatus portal;              // transient
  uint32_t last_page_change_ms = 0;  // transient: 자동 순환 타이머 기준(버튼/순환/부팅)
  // transient: 마지막 조회에서 실패한 관심종목(bit i = watchlist[i]). KIS/FSC만 쓴다(SymbolBatch).
  uint8_t symbol_fail_mask[(int)sched::Provider::Count] = {};
  cal::YearTable calendars[2];  // current/next-year tables; absent remains Unknown
};

// Is the provider configured at all (key present / feature flag on)?
bool ProviderConfigured(const Settings& s, sched::Provider p);

// Earliest UTC at which `p` should next be attempted (INT64_MAX: never, e.g.
// not configured; auth-blocked waits kAuthRetrySec). Combines: retry/backoff gate, release
// calendar (KMA 회차), market gates (KIS 장중, FSC 평일 13시), base interval.
int64_t NextAttemptUtc(const AppState& st, sched::Provider p, int64_t now_utc,
                       bool usb);

// Providers due at now_utc, in enum order (ISSUE-006).
int DueProviders(const AppState& st, int64_t now_utc, bool usb,
                 sched::Provider* out, int cap, bool force = false);

// Records one attempt. retry_after_sec: parsed Retry-After (-1 unknown).
// jitter_slot_ms: stable per-device slot in [0,999]. base_utc: KMA 회차 for
// Ok results (0 otherwise).
void MarkResult(AppState& st, sched::Provider p, Outcome outcome,
                int64_t now_utc, bool usb, int retry_after_sec,
                int jitter_slot_ms, int64_t base_utc = 0);

// Settings changed (portal save) => clear auth blocks so keys are retried.
void ClearAuthBlocks(AppState& st);

// Deep-sleep planning: earliest NextAttemptUtc over all providers.
int64_t NextWakeUtc(const AppState& st, int64_t now_utc, bool usb);

// A local sensor becoming due must not turn on the Wi-Fi radio.
int64_t NextNetworkWakeUtc(const AppState& st, int64_t now_utc, bool usb);
// Connection failure is one failed attempt for each due network provider.
void MarkNetworkUnavailable(AppState& st, int64_t now_utc, bool usb);

// Button mapping: short prev/next/refresh; long => portal (never duplicated).
ui::Page NextPage(ui::Page p);
ui::Page PrevPage(ui::Page p);

}  // namespace app
}  // namespace kdash
