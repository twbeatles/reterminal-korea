#pragma once
// RtcPersistence (SRP: RTC/NVS 런타임 저장·복원만 담당).
// SOLID:
//  - DIP: IStorage 추상에만 의존, 구체 NvsStorage와 분리.
//  - SRP: RTC 인코딩(rtc::)/NVS 체크포인트(RuntimeStore)/달력 테이블
//    로드를 한 흐름으로 묶되, 렌더 상태는 FramePresenter에 위임.
//  - OCP: 보존 스키마 변경은 rtc::Codec 확장으로 수용.
#include <stdint.h>

#include "../../include/IStorage.h"
#include "../app/Coordinator.h"
#include "../app/Fetcher.h"
#include "RtcState.h"

namespace kdash {
namespace power {

// 설정 지문(portable, host 테스트 가능). NVS/RTC 무효화 판단용.
uint32_t SettingsFingerprint(const Settings& s);
// Called after cache restore. Config saves use the configured start page.
void ApplyBootPage(app::AppState& state, bool config_saved, uint8_t retained_page);
// Shared production restore path; atomic decode, page policy and render metadata.
bool RestoreRetainedRuntime(const uint8_t* blob, size_t len, bool config_saved,
                            app::AppState& state, app::Snapshot& snap,
                            app::KisSession& kis, uint32_t& crc, rtc::RenderState& render);
// RTC blob에 스냅샷+스케줄러+세션+렌더 상태 저장.
void SaveRtc(const app::Snapshot& snap, const app::AppState& state,
             const app::KisSession& kis);
// NVS -> RTC 순서로 복원. 시계 신뢰는 절대 복원하지 않음(false 고정).
void RestoreRuntime(IStorage& storage, app::AppState& state,
                    app::Snapshot& snap, app::KisSession& kis);
// 포털 저장/초기화 후 호출: RTC 무효화 + RuntimeStore 무효화.
void InvalidateForPortal(IStorage& storage);
// 데이터 지문이 같은 저장(기기 설정만 변경, 같은 키 재저장) 뒤: 캐시는 유지하고
// 다음 부팅에서 인증 차단을 풀고 설정한 시작 화면을 적용한다(RTC 플래그). ADR 0031.
void RequestAuthClearOnBoot();
// 검증된 시각에만 NVS 체크포인트 기록(내부 RuntimeStore 위임).
bool CheckpointRuntime(IStorage& storage, const app::AppState& state,
                       const app::Snapshot& snap, int64_t now_utc);
// 부트 가드: 리셋 원인별 boots/crashes/cycle_pending 추적.
void BootTrackReset();
uint32_t Boots();
uint32_t Crashes();
void ClearCrashes();
bool CyclePending();
void SetCyclePending(bool pending);

}  // namespace power
}  // namespace kdash
