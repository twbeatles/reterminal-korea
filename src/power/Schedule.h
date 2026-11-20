#pragma once
// Schedule + backoff + retry budgets (docs/06, docs/04).
// Portable. Mirrored by host/kdash/policy.py.
#include <stdint.h>

#include "../domain/Types.h"

namespace kdash {
namespace sched {

enum class Provider : uint8_t {
  Sht40 = 0,
  KmaNow,
  KmaForecast,
  AirKorea,
  Exim,
  FscClose,
  KisPrice,
  Count
};

// Base intervals in seconds (06 기본 계획값; 실측/요금제 확인 후 조정).
// USB 상시전원 / 배터리 분리. KMA forecast/FSC는 발표시각 구동이 원칙이며
// 아래 값은 최대 보호 주기(발표 놓침 방지 상한)이다.
struct Intervals {
  int64_t usb_sec;
  int64_t battery_sec;
};

Intervals BaseInterval(Provider p);

// Next due = last_success + interval (발표 구동 provider는 발표 캘린더가
// 더 이른 시각을 주면 그쪽 우선 — 호출측에서 min() 적용).
int64_t NextDue(int64_t last_success_utc, int64_t interval_sec);

// 주기 배율 적용: pct를 100~400으로 제한(호출 빈도를 늘리는 값은 허용하지 않음).
int64_t ScaleInterval(int64_t interval_sec, int pct);

// Backoff: 2s, 4s, 8s ... cap 5min + deterministic jitter slot (device has no
// rand guarantee at this layer; caller passes attempt + stable slot in [0,999]).
int64_t BackoffSec(int attempt /*0-based*/, int jitter_slot_ms);
bool RetryAllowed(Provider p, int attempt, bool auth_error);
// auth_error(401/서비스키 오류): 재시도 1회 이하 + UI 키 점검 (docs/04).

// 429: Retry-After(초) 존중, 모르면 BackoffSec; 어떤 경우도 호출 폭주 금지.
int64_t RateLimitDelaySec(int retry_after_sec, int attempt, int jitter_slot_ms);

}  // namespace sched
}  // namespace kdash
