#include "PageRotation.h"

namespace kdash {
namespace app {

bool InMarketHours(const DeviceConfig& d, int kst_hour) {
  if (kst_hour < 0 || kst_hour > 23) return false;
  return kst_hour >= d.market_from_hour && kst_hour < d.market_to_hour;
}

RotationDecision DecideRotation(const RotationInput& in) {
  RotationDecision out;
  out.next = in.page;
  if (!in.device || !in.device->auto_rotate || !in.usb || in.portal_open) return out;
  if (in.page != ui::Page::Life && in.page != ui::Page::Market) return out;
  const uint32_t interval_ms = (uint32_t)in.device->rotate_minutes * 60u * 1000u;
  if ((uint32_t)(in.now_ms - in.last_change_ms) < interval_ms) return out;  // wrap-safe
  out.due = true;
  const bool market_ok =
      !in.device->market_hours_only ||
      (in.trading_day && InMarketHours(*in.device, in.kst_hour));
  if (!market_ok) {
    if (in.page == ui::Page::Market) {  // 장 시간이 끝났다: 생활로 복귀
      out.change = true;
      out.next = ui::Page::Life;
    }
    return out;  // 이미 생활이면 그대로(타이머만 다시 시작)
  }
  out.change = true;
  out.next = in.page == ui::Page::Life ? ui::Page::Market : ui::Page::Life;
  return out;
}

}  // namespace app
}  // namespace kdash
