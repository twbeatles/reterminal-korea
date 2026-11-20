#pragma once
// SleepController (SRP: 딥슬립 진입 + 버튼 하드웨어 읽기만 담당).
// SOLID:
//  - SRP: 슬립 클램프·EXT1 마스크·주변장치 종료를 단일 소유.
//  - DIP: 저장할 스냅샷은 값으로 전달받아 RtcPersistence에 위임.
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../app/Fetcher.h"

namespace kdash {
namespace power {

// 하드웨어 풀업, active LOW, INPUT. bit0=KEY0 ...
uint8_t PressedButtons();
// seconds를 [kMinSleepSec, kMaxSleepLowSec]로 클램프 후 딥슬립.
// 호출 전 SaveRtc·패널 hibernate·Wi-Fi 종료를 수행하므로 복귀하지 않음.
void SleepFor(int64_t seconds, const app::Snapshot& snap,
              const app::AppState& state, const app::KisSession& kis);

}  // namespace power
}  // namespace kdash
