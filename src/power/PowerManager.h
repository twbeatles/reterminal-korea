#pragma once
// Power state machine + wake reasons (docs/06).
// ACTIVE_FETCH -> RENDER_IF_CHANGED -> (USB ? IDLE : SET_WAKE_ALARM -> DEEP_SLEEP)
// Deep-sleep/wake GPIO mapping is NOT assumed here — board adapter resolves it
// from the official examples (U-02). Host simulation drives this machine with
// a fake clock (100x sleep/wake loop, PWR-01).
#include <stdint.h>

namespace kdash {
namespace power {

enum class State : uint8_t {
  ActiveFetch = 0,
  RenderIfChanged,
  IdleUsb,
  SetWakeAlarm,
  DeepSleep,
  WakeBoot,
};

enum class WakeReason : uint8_t {
  Timer = 0,
  Button,
  FirstBoot,
  Unknown,
};

enum class PowerSource : uint8_t { Usb, Battery, Unknown };

// USB 감지 실패 시 안전한 저빈도(배터리) 스케줄로 fallback (docs/06).
inline bool UseUsbSchedule(PowerSource src) { return src == PowerSource::Usb; }

struct Step {
  State next;
  bool should_fetch;
  bool should_render;
  bool should_sleep;
};

// Single-step transition with an explicit termination condition per loop
// (02: 루프마다 단일 책임과 종료조건 필수). No unbounded loops in here.
Step Advance(State cur, bool data_changed, PowerSource src);

const char* ToString(State s);
const char* ToString(WakeReason r);

}  // namespace power
}  // namespace kdash
