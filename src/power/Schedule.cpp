#include "Schedule.h"

namespace kdash {
namespace sched {

Intervals BaseInterval(Provider p) {
  switch (p) {
    case Provider::Sht40:
      return {5 * 60, 15 * 60};
    case Provider::KmaNow:
      return {30 * 60, 60 * 60};
    case Provider::KmaForecast:
      return {3 * 60 * 60, 6 * 60 * 60};  // 상한; 실제론 발표 회차 구동
    case Provider::AirKorea:
      return {60 * 60, 2 * 60 * 60};
    case Provider::Exim:
      return {12 * 60 * 60, 24 * 60 * 60};  // 평일 고시 후 1~2회 / 1회
    case Provider::FscClose:
      return {24 * 60 * 60, 24 * 60 * 60};  // 익영업일 13시 이후 1회
    case Provider::KisPrice:
      return {5 * 60, 15 * 60};  // 장중만; 이용제한 확인 시만
    default:
      return {60 * 60, 2 * 60 * 60};
  }
}

int64_t NextDue(int64_t last_success_utc, int64_t interval_sec) {
  if (last_success_utc <= 0) return 0;  // never fetched => due now
  return last_success_utc + interval_sec;
}

int64_t ScaleInterval(int64_t interval_sec, int pct) {
  if (pct < 100) pct = 100;
  if (pct > 400) pct = 400;
  return interval_sec * pct / 100;
}

int64_t BackoffSec(int attempt, int jitter_slot_ms) {
  if (attempt < 0) attempt = 0;
  if (attempt > 8) attempt = 8;
  int64_t base = (int64_t)2 << attempt;  // 2,4,8,...,512
  if (base > 300) base = 300;            // cap 5min
  if (jitter_slot_ms < 0) jitter_slot_ms = -jitter_slot_ms;
  int64_t jitter = (jitter_slot_ms % 1000) * base / 1000 / 2;  // +0~50%
  return base + jitter;
}

bool RetryAllowed(Provider p, int attempt, bool auth_error) {
  (void)p;
  if (auth_error) return attempt < 1;  // 인증오류: 재시도 1회 이하 (docs/04)
  return attempt < 2;  // kMaxRetriesPerProvider: initial + 1 retry
}

int64_t RateLimitDelaySec(int retry_after_sec, int attempt,
                          int jitter_slot_ms) {
  if (retry_after_sec > 0 && retry_after_sec <= 3600) return retry_after_sec;
  return BackoffSec(attempt, jitter_slot_ms);
}

}  // namespace sched
}  // namespace kdash
