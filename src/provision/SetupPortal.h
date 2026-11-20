#pragma once
// Setup portal contracts (docs/04): local AP only, 10-min window, CSRF/
// session/form limits, secrets never re-served, 2-step factory reset.
// Portable validation core; the WiFi-AP/DNS/HTTP server wiring is a later
// device adapter (not part of this tree yet — see docs/12 G-1).
#include <stddef.h>
#include <stdint.h>

#include "../../include/IRandom.h"

namespace kdash {
namespace provision {

inline constexpr const char* kApPrefix = "KDASH-E1001-";
inline constexpr int kPortalTimeoutSec = 10 * 60;  // AP는 10분만
inline constexpr size_t kFormFieldMax = 256;       // == kMaxKeyLen
inline constexpr size_t kCsrfTokenLen = 16;        // bytes => 32 hex chars
inline constexpr size_t kApPasswordLen = 12;       // 31^12 ≈ 2^59

enum class Field : uint8_t {
  Ssid = 0,
  WifiSecret,
  KmaKey,
  AirKey,
  EximKey,
  KisAppKey,
  KisAppSecret,
  GridX,
  GridY,
  DisplayName,
  AirStation,
  Watchlist,
};

// Single form-field validation — error strings never include secret values.
// Shares the SettingsStore validators (one rule set, ISSUE-003).
bool ValidateField(Field f, const char* value, char* err_out,
                   size_t err_cap);

// HTML escaping for reflected values (displayName/station/ssid echo).
// Never emits a partial entity when out_cap is too small.
size_t EscapeHtml(const char* in, char* out, size_t out_cap);

// AP password from the RNG port (no ambiguous chars); device screen shows it.
// out_cap must be > kApPasswordLen. Returns false otherwise.
bool MakeApPassword(IRandom& rng, char* out, size_t out_cap);

bool IsTwoStepResetConfirmed(const char* step1, const char* step2);

// CSRF 토큰: 포털 세션당 1개, 10분(포털 수명) 유효. 비교는 길이 고정 루프.
bool IssueCsrfToken(IRandom& rng, char* out, size_t out_cap);  // 32 hex
bool CheckCsrfToken(const char* presented, const char* expected);

// 자체 AP 이름: "KDASH-E1001-XXXX" (XXXX=16진 4자리, MAC 하위에서 유도.
// 고정 PII/시리얼 사용 금지). suffix 검증을 통과해야 사용.
bool MakeApName(const char* suffix4, char* out, size_t out_cap);

}  // namespace provision
}  // namespace kdash
