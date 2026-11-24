#include "RefreshPolicy.h"

namespace kdash {
namespace ui {

RefreshDecision DecideRefresh(bool content_changed, bool region_aligned,
                              int partials_since_full, RefreshCaps caps) {
  if (!content_changed) return RefreshDecision::Skip;  // 값 변경 없으면 미갱신
  if (!caps.partial_supported || !region_aligned)
    return RefreshDecision::Full;
  if (caps.gray_mode) return RefreshDecision::Full;  // Gray4 partial 미검증
  if (partials_since_full >= kMaxPartialsBeforeFull)
    return RefreshDecision::Full;  // 누적 잔상 방지
  return RefreshDecision::Partial;
}

RefreshCaps FastRefreshCaps(bool enabled, bool otp_waveform, bool has_previous_frame) {
  RefreshCaps c;
  c.partial_supported = enabled && otp_waveform && has_previous_frame;
  c.gray_mode = false;
  return c;
}

bool ShouldPresent(bool force, bool has_render, bool content_changed,
                   bool anything_changed, uint32_t ms_since_refresh) {
  if (force || !has_render || content_changed) return true;
  return anything_changed && ms_since_refresh >= kClockOnlyRefreshMs;
}

int NoteRefreshDone(RefreshDecision d, int partials_since_full) {
  if (d == RefreshDecision::Full) return 0;
  if (d == RefreshDecision::Partial) return partials_since_full + 1;
  return partials_since_full;  // Skip: 카운터 유지
}

}  // namespace ui
}  // namespace kdash
