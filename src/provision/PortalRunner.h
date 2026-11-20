#pragma once
// PortalRunner (SRP: 설정 포털 실행·후처리만 담당).
// SOLID:
//  - DIP: IStorage 추상 + FramePresenter 콜백에 의존.
//  - SRP: 저장/리셋 후 RTC 무효화·재부팅 결정은 이 모듈이 단일 소유.
//
// 두 포털 (ADR 0024)
//  - 같은 Wi-Fi(LAN) 포털: 연결돼 있고 device.lan_mode != Off 이면 우선. USB는 논블로킹(loop에서
//    TickLanPortal), 배터리는 10분 블로킹. 기기 탭/안내 화면에 주소와 페어링 코드를 표시한다.
//  - SoftAP 포털: 첫 설정(SSID 없음), Wi-Fi 미연결, LAN 포털 끔, 또는 LAN 세션이 AP 전환을 요청.
#include <stdint.h>

#include "../../include/IStorage.h"
#include "../app/Coordinator.h"

namespace kdash {
namespace portal {

// loop()가 해야 할 일. None이 아니면 RunCycle을 한 번 돌린다.
enum class LanTick : uint8_t {
  None = 0,
  Redraw,   // 페어링 코드/상태가 바뀌어 기기 탭을 다시 그려야 함
  Refresh,  // 세션이 "지금 새로고침"을 요청
};

// 포털 진입(KEY0 길게/첫 부팅). Saved/Reset이면 RTC 무효화 후 재부팅하므로 돌아오지 않는다.
// LAN 포털이 이미 열려 있으면 길게 누름은 닫기(Button 모드) 또는 기기 탭 보기(Always 모드).
bool OpenPortal(app::AppState& state, IStorage& storage);

// USB 상시 동작 중 loop()에서 자주 호출. Always 모드면 연결 시 자동으로 열고, 열려 있으면 요청을
// 처리한다. state.portal(기기 탭 표시용)을 갱신한다. 배터리면 아무 것도 하지 않는다.
LanTick TickLanPortal(app::AppState& state, IStorage& storage, bool usb);

}  // namespace portal
}  // namespace kdash
