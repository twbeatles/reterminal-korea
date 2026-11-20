#include "PortalAuth.h"

#include <string.h>

#include "SetupPortal.h"

namespace kdash {
namespace portal {

void PortalAuth::NewCode(int64_t now) {
  for (size_t i = 0; i < kPairCodeLen; i++)
    code_[i] = (char)('0' + UniformBelow(rng_, 10));
  code_[kPairCodeLen] = '\0';
  has_code_ = true;
  code_at_ = now;
  fails_ = 0;
  changed_ = true;
}

void PortalAuth::Tick(int64_t now) {
  if (locked_) {
    if (now >= lock_until_) {
      locked_ = false;
      NewCode(now);
    }
  } else if (!has_code_ || now - code_at_ >= kPairCodeTtlSec) {
    NewCode(now);
  }
  if (has_session_ && (now - session_last_ >= kSessionIdleSec ||
                       now - session_start_ >= kSessionMaxSec)) {
    EndSession();
  }
}

bool PortalAuth::TakeDisplayChanged() {
  const bool c = changed_;
  changed_ = false;
  return c;
}

LoginResult PortalAuth::TryLogin(const char* presented, int64_t now, char* token_out,
                                 size_t token_cap) {
  Tick(now);
  if (locked_) return LoginResult::Locked;
  if (!token_out || token_cap < kSessionTokenHex + 1) return LoginResult::Bad;
  // 길이 고정 비교 루프(CSRF 비교와 같은 함수)를 재사용한다.
  if (!provision::CheckCsrfToken(presented, code_)) {
    if (++fails_ >= kPairMaxFails) {
      locked_ = true;
      lock_until_ = now + kPairLockSec;
      code_[0] = '\0';
      has_code_ = false;
      changed_ = true;
      return LoginResult::Locked;
    }
    return LoginResult::Bad;
  }
  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < kSessionTokenHex; i++)
    token_[i] = kHex[rng_.next_u32() & 0xF];
  token_[kSessionTokenHex] = '\0';
  has_session_ = true;
  session_start_ = session_last_ = now;
  memcpy(token_out, token_, kSessionTokenHex + 1);
  NewCode(now);  // 코드는 1회용
  return LoginResult::Ok;
}

bool PortalAuth::SessionValid(const char* token, int64_t now) {
  Tick(now);
  if (!has_session_ || !token) return false;
  if (!provision::CheckCsrfToken(token, token_)) return false;
  session_last_ = now;
  return true;
}

void PortalAuth::EndSession() {
  has_session_ = false;
  token_[0] = '\0';
}

}  // namespace portal
}  // namespace kdash
