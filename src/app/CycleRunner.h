#pragma once
// CycleRunner (SRP: 한 사이클 오케스트레이션만 담당).
// 파이프라인: 시각 동기화 -> 사이클 계획 -> 센서 -> Wi-Fi/NTP -> fetch
// -> 달력 -> freshness 재평가 -> 렌더 -> 체크포인트 -> (배터리 시 슬립).
// SOLID:
//  - DIP: IHttpClient/IStorage/IFetchControl 추상에만 의존. fetch 파이프라인
//    (RunFetchPass)은 순수 portable이며 전송 구현과 분리.
//  - SRP: 각 단계의 실제 작업은 전담 모듈에 위임, 여기는 순서·예산만 결정.
//  - OCP: 새 provider/단계 추가는 Fetcher·모듈 확장, 순서 뼈대는 닫힘.
//  - CyclePlatform.h의 포트로 같은 실행 로직을 Host와 E1001에서 사용한다.
//    ready/SetDeadline도 IHttpClient 계약에 포함되며 concrete cast는 없다.
#include "../../include/IHttpClient.h"
#include "../../include/IStorage.h"
#include "../drivers/ButtonDecoder.h"
#include "../power/CyclePlanner.h"
#include "Coordinator.h"
#include "Fetcher.h"

namespace kdash {
namespace app {

// MarketStateText(now, state): cal::MarketStateText thin wrapper(portable).
const char* MarketStateText(int64_t now, const AppState& state);
// 한 사이클 실행. fw_version은 부트 identity 문자열(예: "1.0.0").
// http==nullptr이면 네트워크 단계를 건너뜀(전송 할당 실패 시에도 크래시 없음).
void RunCycle(power::CycleWake reason, ButtonEvent event, AppState& state,
              Snapshot& snap, KisSession& kis, IHttpClient* http,
              IStorage& storage, int jitter_slot, const char* fw_version);

}  // namespace app
}  // namespace kdash
