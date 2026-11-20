#pragma once
// WifiSupervisor: portable Wi-Fi keep-alive policy (no radio calls here).
// The firmware executes the returned actions; host tests drive it with fake time.
//
// Policy (USB-powered, always-on):
//  - never connected / link lost -> reconnect without waiting for a data refresh;
//  - lost link gets a short grace period for the driver's own auto-reconnect;
//  - an attempt times out after kAttemptTimeoutMs; retries back off 5 s .. 5 min;
//  - every kResetEvery-th attempt fully resets the radio (stuck driver state);
//  - the header icon is redrawn only after the new state has been stable for a while
//    and not more often than once per kRedrawMinGapMs (no e-paper flicker on flapping).
// Battery mode never uses it (the radio is only on during a refresh).
#include <stdint.h>

namespace kdash {
namespace net {

enum class WifiAction : uint8_t { None = 0, Begin, ResetAndBegin };

inline constexpr uint32_t kWifiAttemptTimeoutMs = 20u * 1000u;
inline constexpr uint32_t kWifiLostGraceMs = 10u * 1000u;
inline constexpr uint32_t kWifiBackoffBaseMs = 5u * 1000u;
inline constexpr uint32_t kWifiBackoffMaxMs = 300u * 1000u;
inline constexpr int kWifiResetEvery = 4;
inline constexpr uint32_t kWifiStableMs = 30u * 1000u;
inline constexpr uint32_t kWifiRedrawMinGapMs = 300u * 1000u;

class WifiSupervisor {
 public:
  // Called often (every loop). keep_alive=false (battery) or has_ssid=false resets state.
  WifiAction Tick(uint32_t now_ms, bool connected, bool keep_alive, bool has_ssid);

  // A blocking connect made elsewhere (RunCycle) finished without a link.
  void NoteBlockingFailure(uint32_t now_ms);

  // A keep-alive attempt started by Tick() is still within its timeout. A blocking
  // connect should wait for it rather than restart association (audit G-4).
  bool AttemptInFlight(uint32_t now_ms) const {
    return in_progress_ && (uint32_t)(now_ms - attempt_start_) < kWifiAttemptTimeoutMs;
  }

  // True once when the header Wi-Fi icon should be redrawn (state differs from what
  // was last shown, stable for kWifiStableMs, rate limited).
  bool TakeRedrawRequest(uint32_t now_ms);

  // Diagnostics for logs/tests.
  int failures() const { return failures_; }
  int attempts() const { return attempts_; }
  uint32_t last_backoff_ms() const { return last_backoff_ms_; }
  static uint32_t BackoffMs(int failures);

 private:
  bool initialized_ = false;
  bool connected_ = false;
  bool in_progress_ = false;
  uint32_t attempt_start_ = 0;
  uint32_t next_attempt_ = 0;
  uint32_t state_since_ = 0;
  int failures_ = 0;
  int attempts_ = 0;
  uint32_t last_backoff_ms_ = 0;
  // header icon bookkeeping
  bool shown_known_ = false;
  bool shown_ = false;
  bool has_redrawn_ = false;
  uint32_t last_redraw_ = 0;
};

}  // namespace net
}  // namespace kdash
