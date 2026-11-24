#include "EpdPanel.h"
#include "Uc8179.h"
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <SPI.h>
#include "../boot/SerialLog.h"

namespace kdash { namespace epd { namespace {
SPIClass spi(HSPI);
bool begun = false;
bool awake = false;
bool use_otp = false;

// Read-only half-duplex probe follows Seeed_GFX UC8179's public OTP marker
// protocol: command A2, offsets BE3/17E3. Never programs OTP/eFuses.
bool GpioReady(uint32_t start) {
  delay(10); // allow BUSY to assert after the command, as in Seeed_GFX
  while (digitalRead(kPinBusy) == LOW) {
    if ((uint32_t)(millis() - start) >= 3000) return false;
    delay(10);
  }
  return true;
}
void GpioCommand(uint8_t command) {
  digitalWrite(kPinCs, LOW); digitalWrite(kPinDc, LOW);
  digitalWrite(kPinSck, LOW);
  for (unsigned i = 0; i < 8; ++i) {
    digitalWrite(kPinMosi, (command & 0x80) ? HIGH : LOW);
    digitalWrite(kPinSck, HIGH); digitalWrite(kPinSck, LOW);
    command <<= 1;
  }
  digitalWrite(kPinCs, HIGH);
}
uint8_t GpioReadByte() {
  uint8_t value = 0;
  digitalWrite(kPinCs, LOW); digitalWrite(kPinDc, HIGH);
  pinMode(kPinMosi, INPUT);
  for (unsigned i = 0; i < 8; ++i) {
    digitalWrite(kPinSck, HIGH);
    value = (uint8_t)((value << 1) | (digitalRead(kPinMosi) == HIGH));
    digitalWrite(kPinSck, LOW);
  }
  pinMode(kPinMosi, OUTPUT); digitalWrite(kPinMosi, HIGH);
  digitalWrite(kPinCs, HIGH);
  return value;
}
bool ProbeOtp() {
  pinMode(kPinSck, OUTPUT); pinMode(kPinMosi, OUTPUT);
  digitalWrite(kPinCs, HIGH); digitalWrite(kPinSck, LOW);
  uint8_t markers[2] = {};
  for (unsigned pass = 0; pass < 3; ++pass) {
    digitalWrite(kPinRst, LOW); delay(20);
    digitalWrite(kPinRst, HIGH); delay(20);
    if (!GpioReady(millis())) return false;
    if (pass == 0) {
      GpioCommand(0x40);
      if (!GpioReady(millis())) return false;
      GpioReadByte(); GpioReadByte();
    } else {
      const unsigned offset = pass == 1 ? 0x0BE3 : 0x17E3;
      GpioCommand(0xA2);
      for (unsigned i = 0; i < offset + 10; ++i) {
        const uint8_t v = GpioReadByte();
        if (i == offset) markers[pass - 1] = v;
      }
      delay(20);
    }
  }
  return markers[0] == 0x01 || markers[1] == 0x01;
}
class SpiPort : public PanelPort {
 public:
  uint32_t now_ms() override { return millis(); }
  void pause_ms(uint32_t ms) override { delay(ms); }
  void reset(bool high) override { digitalWrite(kPinRst, high ? HIGH : LOW); }
  bool idle() override { return digitalRead(kPinBusy) != LOW; }
  bool use_otp_lut() const override { return use_otp; }
  void trace(const char* phase, uint32_t start) override {
    KDASH_LOG.printf("epd: phase=%s t_ms=%lu busy_idle=%d\n", phase,
                     (unsigned long)(millis() - start), (int)idle());
  }
  void command(uint8_t value) override {
    end_data();
    spi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(kPinDc, LOW); digitalWrite(kPinCs, LOW);
    spi.transfer(value);
    digitalWrite(kPinCs, HIGH); digitalWrite(kPinDc, HIGH);
    spi.endTransaction();
  }
  void begin_data() override {
    if (streaming_) return;
    spi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(kPinDc, HIGH); digitalWrite(kPinCs, LOW);
    streaming_ = true;
  }
  void end_data() override {
    if (!streaming_) return;
    digitalWrite(kPinCs, HIGH);
    spi.endTransaction();
    streaming_ = false;
  }
  void data(const uint8_t* bytes, size_t len) override {
    const bool owned = !streaming_;
    if (owned) begin_data();
    for (size_t i=0;i<len;i++) spi.transfer(bytes[i]);
    if (owned) end_data();
  }
 private:
  bool streaming_ = false;
};
SpiPort port;
}  // namespace

void EpdPanel::begin() {
  if (begun) return;
  // Shared-bus guard: deselect and power down the microSD slot so a card in
  // SD (non-SPI) mode cannot answer on SCK/MOSI during panel traffic.
  pinMode(kPinSdCs, OUTPUT); digitalWrite(kPinSdCs, HIGH);
  pinMode(kPinSdEn, OUTPUT); digitalWrite(kPinSdEn, LOW);
  pinMode(kPinSdDet, INPUT_PULLUP);
  delay(5);
  KDASH_LOG.printf("display: sd_card_present=%d sd_power=off sd_cs=high\n",
                   (int)(digitalRead(kPinSdDet) == LOW));
  pinMode(kPinCs, OUTPUT); digitalWrite(kPinCs, HIGH);
  pinMode(kPinDc, OUTPUT); digitalWrite(kPinDc, HIGH);
  pinMode(kPinRst, OUTPUT); digitalWrite(kPinRst, HIGH);
  pinMode(kPinBusy, INPUT);
  use_otp = ProbeOtp();
  KDASH_LOG.printf("display: waveform=%s\n", use_otp ? "internal-otp" : "external-lut");
  spi.begin(kPinSck,-1,kPinMosi,-1);
  begun = true;
}
bool EpdPanel::push_full(const uint8_t* fb) {
  begin();
  awake = true;
  const bool ok = PushMonoFrame(port, fb, kBusyTimeoutMs);
  if (ok) awake = false;  // completed frames now power off and enter deep sleep
  return ok;
}
bool EpdPanel::push_gray(const uint8_t* fb) {
  begin();
  awake = true;
  const bool ok = PushGrayFrame(port, fb, kBusyTimeoutMs);
  if (ok) awake = false;
  return ok;
}
bool EpdPanel::push_fast(const uint8_t* old_fb, bool old_any_ink, const uint8_t* new_fb) {
  begin();
  if (!use_otp) return false;  // fast-partial waveform lives in OTP only
  awake = true;
  const bool ok = PushFastFrame(port, old_fb, old_any_ink, new_fb, kBusyTimeoutMs);
  if (ok) awake = false;
  return ok;
}
bool EpdPanel::fast_capable() const { return begun && use_otp; }
void EpdPanel::hibernate() {
  if (begun && awake) SleepPanel(port, kBusyTimeoutMs);
  awake = false;
}
bool EpdPanel::refresh(const Rect&, RefreshMode) { return false; }
void EpdPanel::full_refresh() {}
} }
#endif
