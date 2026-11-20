#pragma once
// LanPolicy: LAN(같은 Wi-Fi) 포털 어댑터가 쓰는 판정만 모은 순수 함수 (ADR 0026, 감사 ISSUE-007).
// 기기 어댑터(PortalServer_esp32.cpp / PortalRunner.cpp)는 호스트에서 돌지 않으므로,
// "언제 닫는가"를 여기로 빼서 PC 테스트로 고정한다.
#include <stdint.h>

namespace kdash {
namespace portal {

enum class LanGate : uint8_t {
  Serve = 0,  // 요청을 처리한다
  Close,      // 포털을 닫는다(창 만료 또는 사용자가 닫음)
  WaitLink,   // Wi-Fi 재연결을 기다린다(창은 계속 흐른다)
};

// 만료·닫힘은 Wi-Fi 상태보다 먼저 본다. 끊긴 동안에도 창은 끝나야 한다.
LanGate DecideLanGate(bool window_expired, bool closed, bool connected);

// 배터리 블로킹 포털의 절대 상한(활동으로 창이 늘어나도 넘지 않는다).
inline constexpr uint32_t kLanBlockingMaxMs = 30u * 60u * 1000u;

// 배터리 블로킹 포털을 끝내야 하는가: 절대 상한을 넘었거나 저전압.
// battery_mv < 0 은 측정 불가(전압 조건은 보지 않는다).
bool LanBlockingShouldExit(uint32_t elapsed_ms, int battery_mv);

}  // namespace portal
}  // namespace kdash
