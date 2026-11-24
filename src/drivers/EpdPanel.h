#pragma once
// EpdPanel: UC8179/GDEY075T7 adapter (T04d). Device-only SPI path;
// geometry/refresh policy stays portable (RefreshPolicy + RenderStep).
// Pin map (official GxEPD2_reTerminal_E1001): SCK=7, MOSI=9, CS=10,
// DC=11, RST=12, BUSY=13, HSPI 2MHz MODE0. Fast differential refresh
// (push_fast, ADR 0034) is opt-in and only on OTP-waveform panels.
#include <stdint.h>

#include "../../include/IDisplay.h"

namespace kdash {
namespace epd {

inline constexpr int kPinSck = 7;
inline constexpr int kPinMosi = 9;
inline constexpr int kPinCs = 10;
inline constexpr int kPinDc = 11;
inline constexpr int kPinRst = 12;
inline constexpr int kPinBusy = 13;
// microSD slot shares SCK/MOSI (7/9) with the panel (Seeed wiki
// reterminal_e10xx_with_arduino_peripherals: EN=16 HIGH powers the slot,
// DET=15 LOW = card present, CS=14). An unselected/unpowered state is
// pinned before any panel traffic (ADR 0013).
inline constexpr int kPinSdCs = 14;
inline constexpr int kPinSdDet = 15;
inline constexpr int kPinSdEn = 16;
inline constexpr int kBusyTimeoutMs = 30000;
// BUSY polarity: idle HIGH, busy LOW (official Gray4 checkBusy).
// Init sequence reference: Gray4 initGrayMode (0x01/0x30/0x82/0x06/0x04/
// 0x00/0xE3/0x50/0x52/0x61 + LUT 0x20..0x24). Frame flow: bit-planes
// 0x10/0x13, refresh 0x12, power off 0x02, deep sleep 0x07. The Gray4
// example uses full frames only (no partial) — see hardware/reference/.

class EpdPanel : public IDisplay {
 public:
  void begin() override;
  bool refresh(const Rect& region, RefreshMode mode) override;
  void full_refresh() override;
  // Full/partial framebuffer push (800x480 1bpp, 48,000B, MSB first).
  bool push_full(const uint8_t* fb);
  bool push_gray(const uint8_t* fb);
  // Whole-window fast differential frame (2bpp canvases, mono on the wire).
  bool push_fast(const uint8_t* old_fb, bool old_any_ink, const uint8_t* new_fb);
  // True when begin() found the internal OTP waveform (fast LUT available).
  bool fast_capable() const;
  void hibernate();
};

}  // namespace epd
}  // namespace kdash
