#pragma once
// Refresh policy (docs/05 갱신·잔상 정책).
// - 값 변경 없으면 화소 갱신 없음 (Skip).
// - 변경 있으면 partial(좁은 숫자 영역) — 단, 패널·라이브러리가 영역 정렬과
//   Gray4 partial을 지원할 때만 (실기 검사 전 false).
// - partial N회 누적 또는 Gray 모드 전환 시 Full (ghosting 방지).
// 수집완료 vs 전자잉크 갱신완료 분리: last_rendered_at는 Full/Partial 성공 시만.
#include <stdint.h>

#include "../../include/IDisplay.h"

namespace kdash {
namespace ui {

// Good Display GDEY075T7: fast/partial 5회 연속 뒤 전체 갱신 권장 (ADR 0032).
inline constexpr int kMaxPartialsBeforeFull = 5;

// Seeed E1001 동작온도 0~40°C (패널 단품 0~50°C). 범위 밖 갱신은 화질 미보장이라
// 진단 로그만 남긴다(갱신을 막지 않음). 유효값이 아니면(NaN) 경고하지 않는다.
inline constexpr float kPanelMinTempC = 0.0f;
inline constexpr float kPanelMaxTempC = 40.0f;
inline bool PanelTempInRange(float temp_c) {
  return !(temp_c < kPanelMinTempC || temp_c > kPanelMaxTempC);
}

enum class RefreshDecision : uint8_t { Skip = 0, Partial, Full };

struct RefreshCaps {
  bool partial_supported = false;  // 실기 검사 후 true (Gray4 partial 가정 금지)
  bool gray_mode = false;
};

// 빠른 차분 갱신(ADR 0034, 흑백 전체 창)을 쓸 수 있는지.
// 세 조건이 모두 맞을 때만: 사용자가 켬, 패널이 OTP 파형(빠른 부분 파형 내장),
// 직전 프레임이 RAM에 남아 있음(USB로 깨어 있을 때. 배터리 deep sleep은 PSRAM 소실).
// 빠른 프레임은 흑백이므로 gray_mode=false. N회(kMaxPartialsBeforeFull) 뒤 4계조 Full.
RefreshCaps FastRefreshCaps(bool enabled, bool otp_waveform, bool has_previous_frame);

RefreshDecision DecideRefresh(bool content_changed, bool region_aligned,
                              int partials_since_full, RefreshCaps caps);

// 성공 콜백: 렌더 성공 시 partial 카운터 갱신.
// 시계(헤더 분 표시)만 바뀐 화면은 이 시간이 지나야 다시 그린다(전체 갱신 깜빡임 감소).
inline constexpr uint32_t kClockOnlyRefreshMs = 30u * 60u * 1000u;

// 패널에 올릴지 결정: 강제/첫 화면/내용 변경은 즉시, 시계만 변경은 kClockOnlyRefreshMs 후.
bool ShouldPresent(bool force, bool has_render, bool content_changed,
                   bool anything_changed, uint32_t ms_since_refresh);

int NoteRefreshDone(RefreshDecision d, int partials_since_full);

}  // namespace ui
}  // namespace kdash
