#include "WifiSupervisor.h"

namespace kdash {
namespace net {

uint32_t WifiSupervisor::BackoffMs(int failures) {
  if (failures < 1) failures = 1;
  if (failures > 12) failures = 12;  // 5 s << 11 already exceeds the cap
  uint32_t ms = kWifiBackoffBaseMs << (failures - 1);
  return ms > kWifiBackoffMaxMs ? kWifiBackoffMaxMs : ms;
}

WifiAction WifiSupervisor::Tick(uint32_t now_ms, bool connected, bool keep_alive,
                                bool has_ssid) {
  if (!keep_alive || !has_ssid) {
    *this = WifiSupervisor();  // battery mode / unconfigured: stay out of the way
    return WifiAction::None;
  }
  if (!initialized_) {
    initialized_ = true;
    connected_ = connected;
    state_since_ = now_ms;
    next_attempt_ = now_ms;  // not connected at boot => try right away
    shown_known_ = true;
    shown_ = connected;
  }
  if (connected != connected_) {
    connected_ = connected;
    state_since_ = now_ms;
    in_progress_ = false;
    if (connected) {
      failures_ = 0;
      attempts_ = 0;
    } else {
      next_attempt_ = now_ms + kWifiLostGraceMs;  // let the driver auto-reconnect first
    }
  }
  if (connected) return WifiAction::None;

  if (in_progress_) {
    if ((uint32_t)(now_ms - attempt_start_) >= kWifiAttemptTimeoutMs) {
      in_progress_ = false;
      failures_++;
      last_backoff_ms_ = BackoffMs(failures_);
      next_attempt_ = now_ms + last_backoff_ms_;
    }
    return WifiAction::None;
  }
  if ((int32_t)(now_ms - next_attempt_) >= 0) {
    attempts_++;
    in_progress_ = true;
    attempt_start_ = now_ms;
    return attempts_ % kWifiResetEvery == 0 ? WifiAction::ResetAndBegin
                                            : WifiAction::Begin;
  }
  return WifiAction::None;
}

void WifiSupervisor::NoteBlockingFailure(uint32_t now_ms) {
  in_progress_ = false;
  failures_++;
  last_backoff_ms_ = BackoffMs(failures_);
  next_attempt_ = now_ms + last_backoff_ms_;
}

bool WifiSupervisor::TakeRedrawRequest(uint32_t now_ms) {
  if (!initialized_ || !shown_known_) return false;
  if (connected_ == shown_) return false;
  if ((uint32_t)(now_ms - state_since_) < kWifiStableMs) return false;
  if (has_redrawn_ && (uint32_t)(now_ms - last_redraw_) < kWifiRedrawMinGapMs) return false;
  shown_ = connected_;
  has_redrawn_ = true;
  last_redraw_ = now_ms;
  return true;
}

}  // namespace net
}  // namespace kdash
