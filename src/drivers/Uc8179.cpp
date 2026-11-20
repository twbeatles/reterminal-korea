// E1001 UC8179 command/LUT values adapted from Seeed-Projects' MIT example
// @8d97d891: examples/base/GxEPD2_reTerminal_E1001_Gray4.
// Copyright (c) 2026 Seeed-Projects; permission and warranty notice retained
// in hardware/reference/LICENSE. No GxEPD2 library code is linked.
#include "Uc8179.h"
#include <initializer_list>

namespace kdash { namespace epd { namespace {
constexpr uint8_t lut[5][42] = {
 {0x00,0x00,0x06,0x08,0x07,0x01,0x00,0x06,0x0A,0x0B,0x0A,0x01,0x00,0x03,0x03,0x00,0x00,0x03,0x00,0x05,0x09,0x06,0x06,0x01,0x00,0x02,0x02,0x0A,0x0A,0x01,0x00,0x0A,0x11,0x06,0x07,0x01,0x00,0x02,0x01,0x02,0x01,0x01},
 {0x15,0x00,0x06,0x08,0x07,0x01,0x54,0x06,0x0A,0x0B,0x0A,0x01,0x90,0x03,0x03,0x00,0x00,0x03,0x2A,0x05,0x09,0x06,0x06,0x01,0xAA,0x02,0x02,0x0A,0x0A,0x01,0x00,0x0A,0x11,0x06,0x07,0x01,0x28,0x02,0x01,0x02,0x01,0x01},
 {0x2A,0x00,0x06,0x08,0x07,0x01,0x59,0x06,0x0A,0x0B,0x0A,0x01,0x90,0x03,0x03,0x00,0x00,0x03,0x5A,0x05,0x09,0x06,0x06,0x01,0xA8,0x02,0x02,0x0A,0x0A,0x01,0x45,0x0A,0x11,0x06,0x07,0x01,0xA8,0x02,0x01,0x02,0x01,0x01},
 {0x16,0x00,0x06,0x08,0x07,0x01,0xA0,0x06,0x0A,0x0B,0x0A,0x01,0x90,0x03,0x03,0x00,0x00,0x03,0x99,0x05,0x09,0x06,0x06,0x01,0xA0,0x02,0x02,0x0A,0x0A,0x01,0x40,0x0A,0x11,0x06,0x07,0x01,0x20,0x02,0x01,0x02,0x01,0x01},
 {0x26,0x00,0x06,0x08,0x07,0x01,0x6A,0x06,0x0A,0x0B,0x0A,0x01,0x90,0x03,0x03,0x00,0x00,0x03,0x65,0x05,0x09,0x06,0x06,0x01,0x50,0x02,0x02,0x0A,0x0A,0x01,0x10,0x0A,0x11,0x06,0x07,0x01,0x10,0x02,0x01,0x02,0x01,0x01}
};
bool Wait(PanelPort& p, uint32_t start, uint32_t timeout) {
  p.pause_ms(10);
  while ((uint32_t)(p.now_ms() - start) < timeout) {
    if (p.idle()) return true;
    p.pause_ms(10);
  }
  return false;
}
void Send(PanelPort& p, uint8_t c, const uint8_t* bytes, size_t n) {
  p.command(c);
  p.data(bytes, n);
}
class DataSession {
 public:
  explicit DataSession(PanelPort& p) : port(p) { port.begin_data(); }
  ~DataSession() { port.end_data(); }
 private:
  PanelPort& port;
};
}  // namespace

namespace {
bool PushFrame(PanelPort& p, const uint8_t* fb, uint32_t timeout, bool gray) {
  if (!fb || timeout == 0) return false;
  const uint32_t start = p.now_ms();
  const bool otp = p.use_otp_lut();
  p.reset(false); p.pause_ms(10); p.reset(true); p.pause_ms(10);
  if (!Wait(p, start, timeout)) return false;
  p.trace("reset", start);
  const uint8_t power[] = {0x07,0x17,0x3F,0x3F,0x07};
  const uint8_t otp_power[] = {0x07,0x07,0x3F,0x3F};
  const uint8_t pll[] = {0x06}, vcom[] = {0x12};
  const uint8_t booster[] = {0x27,0x27,0x28,0x17};
  const uint8_t otp_booster[] = {0x27,0x27,0x18,0x17};
  if (otp) {
    Send(p,0x01,otp_power,sizeof(otp_power));
    Send(p,0x06,otp_booster,sizeof(otp_booster));
  } else {
    Send(p,0x01,power,sizeof(power)); Send(p,0x30,pll,sizeof(pll));
    Send(p,0x82,vcom,sizeof(vcom)); Send(p,0x06,booster,sizeof(booster));
  }
  p.command(0x04); p.pause_ms(100);
  if (!Wait(p,start,timeout)) return false;
  p.trace("power_on", start);
  const uint8_t panel[] = {0x3F}, saving[] = {0x88};
  const uint8_t interval[] = {0x10,0x07}, tcon[] = {0x00};
  const uint8_t resolution[] = {0x03,0x20,0x01,0xE0};
  const uint8_t otp_panel[] = {0x1F}, otp_temp_mode[] = {0x02}, otp_temp[] = {0x5F};
  Send(p,0x00,otp ? otp_panel : panel,sizeof(panel));
  if (!otp) Send(p,0xE3,saving,sizeof(saving));
  Send(p,0x50,interval,sizeof(interval));
  if (!otp) Send(p,0x52,tcon,sizeof(tcon));
  Send(p,0x61,resolution,sizeof(resolution));
  if (otp) {
    Send(p,0xE0,otp_temp_mode,sizeof(otp_temp_mode));
    Send(p,0xE5,otp_temp,sizeof(otp_temp));
  }
  for (size_t i=0;!otp && i<5;i++) {
    Send(p,(uint8_t)(0x20+i),lut[i],sizeof(lut[i]));
    if (!Wait(p,start,timeout)) return false;
  }
  for (uint8_t c : {uint8_t(0x10),uint8_t(0x13)}) {
    p.command(c);
    DataSession session(p);  // official uploadGray4Frame holds CS for a plane
    uint8_t line[100];
    for (size_t row=0;row<480;row++) {
      if ((uint32_t)(p.now_ms()-start) >= timeout) return false;
      if (!gray && !otp) p.data(fb+row*100,100);
      else {
        for (size_t byte = 0; byte < 100; ++byte) {
          uint8_t packed = 0;
          for (unsigned bit = 0; bit < 8; ++bit) {
            const size_t x = byte * 8 + bit;
            uint8_t ink = gray ? (fb[row * 200 + x / 4] >> (6 - 2 * (x % 4))) & 3u :
                                ((fb[row * 100 + x / 8] >> (7 - x % 8)) & 1u) * 3;
            // E1001 hardware check (ADR 0012): the OTP waveform keeps the
            // same 00 white / 11 black wire polarity as the register LUT.
            // Seeed_GFX's 11-white convention rendered an inverted panel.
            const unsigned plane = c == 0x10 ? 0 : 1;
            if ((ink >> plane) & 1u) packed |= (uint8_t)(0x80 >> bit);
          }
          line[byte] = packed;
        }
        p.data(line, sizeof(line));
      }
    }
  }
  p.trace("data", start);
  p.command(0x12); p.pause_ms(100);
  p.trace("refresh_issued", start);
  if (!Wait(p,start,timeout)) return false;
  p.trace("refresh_done", start);
  // Match the official Gray4 example's complete init -> upload -> refresh ->
  // power-off/deep-sleep cycle. The next frame exits sleep by hardware reset.
  const uint32_t elapsed = p.now_ms() - start;
  return elapsed < timeout && SleepPanel(p, timeout - elapsed);
}
}  // namespace

bool PushMonoFrame(PanelPort& p, const uint8_t* fb, uint32_t timeout) {
  return PushFrame(p, fb, timeout, false);
}
bool PushGrayFrame(PanelPort& p, const uint8_t* fb, uint32_t timeout) {
  return PushFrame(p, fb, timeout, true);
}

bool SleepPanel(PanelPort& p, uint32_t timeout) {
  const uint32_t start=p.now_ms();
  const uint8_t border_off = 0xF7;
  Send(p,0x50,&border_off,1); // official Seeed_GFX sleep sequence
  p.command(0x02);
  if (!Wait(p,start,timeout)) return false;
  p.trace("power_off", start);
  const uint8_t key=0xA5;
  Send(p,0x07,&key,1);
  return true;
}
} }
