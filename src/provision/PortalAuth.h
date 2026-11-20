#pragma once
// PortalAuth: LAN 포털 접근 제어 (순수, 시각/난수 주입). ADR 0024.
//  - 6자리 페어링 코드는 기기 화면에만 표시한다(UART·HTTP 응답 금지). 물리적 근접 = 권한.
//  - 코드는 1회용: 성공하면 폐기하고 새 코드를 발급한다. 10분이 지나면 새 코드.
//  - 5회 틀리면 5분 잠금(코드 폐기, 잠금이 끝나면 새 코드).
//  - 세션은 1개. 새 로그인은 이전 세션을 끊는다. 유휴 10분, 최대 30분.
#include <stddef.h>
#include <stdint.h>

#include "../../include/IRandom.h"

namespace kdash {
namespace portal {

inline constexpr size_t kPairCodeLen = 6;
inline constexpr int kPairCodeTtlSec = 600;
inline constexpr int kPairMaxFails = 5;
inline constexpr int kPairLockSec = 300;
inline constexpr int kSessionIdleSec = 600;
inline constexpr int kSessionMaxSec = 1800;
inline constexpr size_t kSessionTokenHex = 32;

enum class LoginResult : uint8_t { Ok = 0, Bad, Locked };

class PortalAuth {
 public:
  explicit PortalAuth(IRandom& rng) : rng_(rng) {}

  // 코드 만료/잠금 해제/세션 만료 처리. 요청마다, 루프마다 호출한다.
  void Tick(int64_t now);

  // 화면에 표시할 현재 코드. 잠금 중이면 "".
  const char* Code() const { return locked_ ? "" : code_; }
  bool Locked() const { return locked_; }
  // 표시 내용(코드/잠금)이 바뀌었으면 true를 한 번 돌려준다(화면 갱신 트리거).
  bool TakeDisplayChanged();

  LoginResult TryLogin(const char* presented, int64_t now, char* token_out,
                       size_t token_cap);
  // 유효하면 유휴 시간을 갱신한다.
  bool SessionValid(const char* token, int64_t now);
  bool HasSession() const { return has_session_; }
  void EndSession();

 private:
  IRandom& rng_;
  char code_[kPairCodeLen + 1] = {0};
  char token_[kSessionTokenHex + 1] = {0};
  bool has_code_ = false;
  bool locked_ = false;
  bool has_session_ = false;
  bool changed_ = false;
  int fails_ = 0;
  int64_t code_at_ = 0;
  int64_t lock_until_ = 0;
  int64_t session_start_ = 0;
  int64_t session_last_ = 0;

  void NewCode(int64_t now);
};

}  // namespace portal
}  // namespace kdash
