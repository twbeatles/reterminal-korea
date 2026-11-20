#include "PowerManager.h"

namespace kdash {
namespace power {

Step Advance(State cur, bool data_changed, PowerSource src) {
  Step s{cur, false, false, false};
  switch (cur) {
    case State::ActiveFetch:
      s.next = State::RenderIfChanged;
      s.should_fetch = true;
      break;
    case State::RenderIfChanged:
      s.should_render = data_changed;  // 값 변경 없으면 화소 갱신 없음 (05)
      s.next = UseUsbSchedule(src) ? State::IdleUsb : State::SetWakeAlarm;
      break;
    case State::IdleUsb:
      s.next = State::ActiveFetch;  // 다음 due에 호출측이 재진입
      break;
    case State::SetWakeAlarm:
      s.next = State::DeepSleep;
      s.should_sleep = true;
      break;
    case State::DeepSleep:
      s.next = State::WakeBoot;  // wake는 리부팅으로 관찰됨
      break;
    case State::WakeBoot:
      s.next = State::ActiveFetch;
      break;
  }
  return s;
}

const char* ToString(State s) {
  switch (s) {
    case State::ActiveFetch:
      return "ACTIVE_FETCH";
    case State::RenderIfChanged:
      return "RENDER_IF_CHANGED";
    case State::IdleUsb:
      return "IDLE_USB";
    case State::SetWakeAlarm:
      return "SET_WAKE_ALARM";
    case State::DeepSleep:
      return "DEEP_SLEEP";
    case State::WakeBoot:
      return "WAKE_BOOT";
  }
  return "UNKNOWN";
}

const char* ToString(WakeReason r) {
  switch (r) {
    case WakeReason::Timer:
      return "TIMER";
    case WakeReason::Button:
      return "BUTTON";
    case WakeReason::FirstBoot:
      return "FIRST_BOOT";
    default:
      return "UNKNOWN";
  }
}

}  // namespace power
}  // namespace kdash
