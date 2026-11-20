#pragma once
// EspClockSync (SRP: RTC/NTP 시각 부트스트랩만 담당).
// SOLID:
//  - DIP: IClockPlatform 추상에만 의존(BootstrapClock에 주입).
//  - OCP: 새 클록 소스 추가는 플랫폼 구현 교체로 확장.
//  - SRP: NTP 알림 상태·주기 정책은 이 모듈 내부에 캡슐화.
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../domain/ClockPolicy.h"

#ifndef KDASH_BUILD_EPOCH
#define KDASH_BUILD_EPOCH 1767225600  // 2026-01-01T00:00:00Z (T03: older clocks distrusted)
#endif

namespace kdash {
namespace clock {

// Wi-Fi 연결 후 호출. AppState::time_verified를 갱신하고 검증 여부를 반환.
// verified RTC를 먼저 적용한 뒤 NTP를 시도하므로 TLS가 NTP 차단 시에도 진행 가능.
bool SyncClock(bool wifi_ready, app::AppState& state);
// 마지막 Bootstrap 결정문(부트 로그용).
Decision LastDecision();

}  // namespace clock
}  // namespace kdash
