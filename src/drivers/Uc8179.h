#pragma once
#include <stddef.h>
#include <stdint.h>

namespace kdash { namespace epd {
// Transport-independent sequence from the official E1001 Gray4 example.
// Canvas black bits map to both inverted gray planes (black=11, white=00).
class PanelPort {
 public:
  virtual ~PanelPort() = default;
  virtual uint32_t now_ms() = 0;
  virtual void pause_ms(uint32_t ms) = 0;
  virtual void reset(bool high) = 0;
  virtual bool idle() = 0;
  virtual void command(uint8_t value) = 0;
  // A complete RAM plane must retain one SPI transaction/CS assertion.
  virtual void begin_data() {}
  virtual void end_data() {}
  virtual bool use_otp_lut() const { return false; }
  // Optional diagnostics hook: phase name and elapsed time since frame start.
  virtual void trace(const char*, uint32_t) {}
  virtual void data(const uint8_t* bytes, size_t len) = 0;
};
bool PushMonoFrame(PanelPort& port, const uint8_t* framebuffer,
                   uint32_t timeout_ms = 30000);
// Packed 2bpp ink coverage: 00 white, 01 light, 10 dark, 11 black.
bool PushGrayFrame(PanelPort& port, const uint8_t* framebuffer,
                   uint32_t timeout_ms = 30000);
// Fast differential refresh (ADR 0034): GDEY075T7 OTP fast-partial waveform
// over the whole window, ~0.5 s and no full-screen inversion. Register values
// follow GxEPD2 GDEY075T7 (_InitDisplay + _Init_Part, useFastPartialUpdateFromOTP);
// no GxEPD2 code is copied or linked. Both inputs are 2bpp Canvas buffers,
// thresholded to black/white (coverage >= kFastInkThreshold is black).
// 0x10 receives the image currently on the panel, 0x13 the new image.
// old_any_ink: the panel currently shows a 4-gray frame, so any coverage counts
// as black in the old plane and faint antialias pixels are driven to white.
inline constexpr uint8_t kFastInkThreshold = 2;
bool PushFastFrame(PanelPort& port, const uint8_t* old_fb, bool old_any_ink,
                   const uint8_t* new_fb, uint32_t timeout_ms = 30000);
bool SleepPanel(PanelPort& port, uint32_t timeout_ms = 30000);
} }
