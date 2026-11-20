#pragma once
// PageRotation: 생활 <-> 시장 자동 순환 정책 (순수 함수, 시계/버튼은 값으로 주입).
//
// 규칙
//  - USB 전원일 때만 동작한다(배터리는 갱신 때만 깨어나므로 5분마다 패널을 다시 그리지 않는다).
//  - 현재 화면이 생활/시장일 때만 순환한다. 달력/기기 탭은 사용자가 고른 화면이므로 건드리지 않는다.
//  - 사용자가 버튼을 누르면 타이머가 다시 시작한다(호출자가 last_change_ms를 갱신).
//  - market_hours_only: 시장 탭은 거래일(주말·공휴일·휴장일 제외)의 KST 장 시간(from <= 시 < to)에만
//    순환에 넣는다. 그 밖이거나 시각을 믿을 수 없으면 생활 화면에 머문다(시장 화면 중이면 생활로).
//  - "필요할 때" 설정 포털이 열려 있는 동안은 바꾸지 않는다(상시 모드는 늘 열려 있으므로 제외).
#include <stdint.h>

#include "../../include/Config.h"
#include "../ui/Layout.h"

namespace kdash {
namespace app {

struct RotationInput {
  const DeviceConfig* device = nullptr;
  ui::Page page = ui::Page::Life;
  bool usb = false;
  bool portal_open = false;       // "필요할 때" 포털이 열려 있으면 화면을 바꾸지 않는다(호출자가 채움)
  bool trading_day = true;        // 오늘이 KRX 거래일인가(달력 계층, 시각 불명이면 호출자가 true/false 판단)
  int kst_hour = -1;              // 0..23, 시각을 믿을 수 없으면 -1
  uint32_t now_ms = 0;
  uint32_t last_change_ms = 0;    // 마지막 화면 전환(버튼/순환/부팅) 시각
};

struct RotationDecision {
  bool due = false;               // 간격이 지나 점검 시점이다(타이머를 다시 시작해야 함)
  bool change = false;            // 화면을 바꿔야 한다
  ui::Page next = ui::Page::Life;
};

// 장 시간 안인가(시각 불명이면 false). 거래일 여부는 보지 않는다(RotationInput.trading_day).
bool InMarketHours(const DeviceConfig& d, int kst_hour);

RotationDecision DecideRotation(const RotationInput& in);

}  // namespace app
}  // namespace kdash
