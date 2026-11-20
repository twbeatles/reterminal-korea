#pragma once
// ButtonDecoder: pure debounce + short/long state machine (T06).
// KEY0=bit0, KEY1=bit1, KEY2=bit2. Pressed = LOW on hardware; samples
// carry already-inverted "pressed" bits. Portable.
#include <stddef.h>
#include <stdint.h>

namespace kdash {

enum class ButtonEvent : uint8_t {
  None = 0,
  ShortKey0,
  ShortKey1,
  ShortKey2,
  LongKey0,
};

struct ButtonSample {
  uint32_t t_ms = 0;
  uint8_t pressed_mask = 0;  // bit0=KEY0 ...
};

class ButtonDecoder {
 public:
  explicit ButtonDecoder(uint16_t debounce_ms = 50, uint16_t long_ms = 2000);
  ButtonEvent Feed(const ButtonSample& s);
  void Reset();

 private:
  uint16_t debounce_ms_, long_ms_;
  uint8_t stable_ = 0;      // debounced pressed mask
  uint8_t candidate_ = 0;   // current candidate mask
  uint32_t cand_since_ = 0;
  bool have_cand_ = false;
  uint8_t active_btn_ = 0;  // single button being tracked (bit index)
  bool active_valid_ = false;
  uint32_t press_since_ = 0;
  bool long_fired_ = false;
  bool first_ = true;
};

// Wake helper: EXT1 mask (bit3=KEY0...) + current pin state. Returns the
// event for a wake that already happened (wake itself is press evidence).
ButtonEvent WakeButtons(uint8_t ext1_mask, uint8_t pressed_now_mask,
                        bool held_long);

}  // namespace kdash
