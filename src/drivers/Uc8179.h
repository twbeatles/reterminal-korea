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
bool SleepPanel(PanelPort& port, uint32_t timeout_ms = 30000);
} }
