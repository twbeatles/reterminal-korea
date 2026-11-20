#pragma once
// WifiConnector (SRP: Wi-Fi 연결만 담당).
// SOLID:
//  - ISP: 호출에 필요한 최소 입력(Settings SSID/secret)만 받음.
//  - DIP: 연결 결과(bool)만 반환, 상위 정책은 CycleRunner가 결정.
#include "../../include/Config.h"

namespace kdash {
namespace net {

// SSID가 비어 있으면 false. 20초 바운드 대기, 결과만 반환.
bool ConnectWifi(const Settings& s);

// USB 상시 동작 중 연결 유지: loop()에서 자주 호출한다(HTTP 요청은 만들지 않는다).
// 갱신할 것이 없어도 연결하고, 끊기면 유예(10초) 후 재연결, 실패 시 5초~5분 백오프,
// 4번째 시도마다 라디오 전체 리셋(WifiSupervisor 정책). usb=false(배터리)면 아무 것도 하지 않는다.
void MaintainWifi(const Settings& s, bool usb);

// 헤더 Wi-Fi 아이콘을 다시 그려야 하면 true를 한 번 돌려준다(30초 안정 + 5분 간격 제한).
bool TakeWifiRedrawRequest();

}  // namespace net
}  // namespace kdash
