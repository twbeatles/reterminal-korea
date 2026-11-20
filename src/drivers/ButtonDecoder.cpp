#include "ButtonDecoder.h"

namespace kdash {

ButtonDecoder::ButtonDecoder(uint16_t debounce_ms, uint16_t long_ms)
    : debounce_ms_(debounce_ms), long_ms_(long_ms) {}

void ButtonDecoder::Reset() {
  stable_ = candidate_ = 0;
  have_cand_ = active_valid_ = long_fired_ = false;
  first_ = true;
}

static int FirstBit(uint8_t m) {
  for (int i = 0; i < 3; i++)
    if (m & (1u << i)) return i;
  return -1;
}

ButtonEvent ButtonDecoder::Feed(const ButtonSample& s) {
  uint8_t m = (uint8_t)(s.pressed_mask & 0x07);
  if (first_) {
    first_ = false;
    stable_ = candidate_ = m;
    cand_since_ = s.t_ms;
    have_cand_ = false;
    if (m) {
      int b = FirstBit(m);
      active_btn_ = (uint8_t)b;
      active_valid_ = true;
      press_since_ = s.t_ms;
      long_fired_ = false;
    }
    return ButtonEvent::None;
  }
  // Track candidate stability.
  if (!have_cand_ || m != candidate_) {
    candidate_ = m;
    cand_since_ = s.t_ms;
    have_cand_ = true;
  }
  if (m != stable_ && (s.t_ms - cand_since_ >= debounce_ms_ ||
                       (!m && stable_ && s.t_ms - press_since_ >= debounce_ms_))) {
    uint8_t prev = stable_;
    stable_ = m;
    // Press edge: latch the first button only.
    if (m && !prev) {
      int b = FirstBit(m);
      active_btn_ = (uint8_t)b;
      active_valid_ = true;
      press_since_ = s.t_ms;
      long_fired_ = false;
    }
    // Release edge: short press if long never fired.
    if (!m && prev) {
      if (active_valid_ && !long_fired_) {
        ButtonEvent e = ButtonEvent::None;
        if (active_btn_ == 0)
          e = ButtonEvent::ShortKey0;
        else if (active_btn_ == 1)
          e = ButtonEvent::ShortKey1;
        else
          e = ButtonEvent::ShortKey2;
        active_valid_ = false;
        return e;
      }
      active_valid_ = false;
    }
  }
  // Long press: fire once at long_ms while held.
  if (active_valid_ && !long_fired_ && stable_ && active_btn_ == 0 &&
      s.t_ms - press_since_ >= long_ms_) {
    long_fired_ = true;
    return ButtonEvent::LongKey0;
  }
  return ButtonEvent::None;
}

ButtonEvent WakeButtons(uint8_t ext1_mask, uint8_t pressed_now_mask,
                        bool held_long) {
  // EXT1 bits are GPIO numbers (3/4/5); map to KEY0/1/2.
  int key = -1;
  if (ext1_mask & (1u << 3))
    key = 0;
  else if (ext1_mask & (1u << 4))
    key = 1;
  else if (ext1_mask & (1u << 5))
    key = 2;
  if (key < 0) return ButtonEvent::None;
  bool still = (pressed_now_mask & (1u << key)) != 0;
  if (key == 0 && still && held_long) return ButtonEvent::LongKey0;
  if (!still) {
    // Released before debounce: wake itself is the press evidence.
    if (key == 0) return ButtonEvent::ShortKey0;
    if (key == 1) return ButtonEvent::ShortKey1;
    return ButtonEvent::ShortKey2;
  }
  // Still held but not long: report short (caller keeps sampling for long).
  if (key == 0) return ButtonEvent::ShortKey0;
  if (key == 1) return ButtonEvent::ShortKey1;
  return ButtonEvent::ShortKey2;
}

}  // namespace kdash
