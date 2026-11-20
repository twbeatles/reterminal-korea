#pragma once
// FramePresenter (SRP: ViewModel -> Canvas -> Panel 프레임 출력만 담당).
// SOLID:
//  - DIP: IDisplay(EpdPanel 추상)/Canvas에만 의존, Wi-Fi/시계는 값으로 주입.
//  - OCP: 새 페이지 추가는 ViewModel+Renderer 확장, 출력 경로는 닫힘.
//  - SRP: 변경 감지(CRC) + 풀 리프레시 정책은 이 모듈이 단일 소유.
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../app/Fetcher.h"
#include "../power/RtcState.h"

namespace kdash {
namespace ui {

// AppState/Snapshot -> 페이지 렌더. force=false면 동일 CRC 스킵.
// state.last_render_utc는 성공 시 갱신. 할당 실패/패널 실패 시 false.
bool RenderSnapshot(app::AppState& state, const app::Snapshot& snap,
                    int battery_mv, const char* fw_version, int crashes,
                    bool force = false);
// 설정 포털 안내 화면(로컬 AP 비번은 패널에만 표시). 성공 시 true.
bool ShowPortal(const char* ap, const char* pass, const char* url);
// 같은 Wi-Fi 설정 포털 안내 화면(주소 + 1회용 페어링 코드). 성공 시 true.
bool ShowLanPortal(const char* url, const char* host, const char* code);
// 변경 감지 상태(RTC 저장·복원용).
bool HasRender();
uint32_t LastRenderCrc();
void SetRenderState(uint32_t crc, bool has);
rtc::RenderState RetainedRenderState();
void RestoreRenderState(uint32_t crc, const rtc::RenderState& render);
// 슬립 전 패널 hibernate 위임.
void HibernatePanel();

}  // namespace ui
}  // namespace kdash
