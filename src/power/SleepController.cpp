#include "SleepController.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "../boot/SerialLog.h"
#include "../drivers/Pmic.h"
#include "../ui/FramePresenter.h"
#include "CyclePlanner.h"
#include "RtcPersistence.h"

namespace kdash {
namespace power {

uint8_t PressedButtons() {
  uint8_t mask = 0;
  // Official UserButtons example: hardware pull-ups, active LOW, INPUT.
  for (int i = 0; i < 3; ++i) if (digitalRead(3 + i) == LOW) mask |= 1u << i;
  return mask;
}

void SleepFor(int64_t seconds, const app::Snapshot& snap,
              const app::AppState& state, const app::KisSession& kis) {
  if (seconds < power::kMinSleepSec) seconds = power::kMinSleepSec;
  if (seconds > power::kMaxSleepLowSec) seconds = power::kMaxSleepLowSec;
  SaveRtc(snap, state, kis);
  ui::HibernatePanel();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  digitalWrite(21, LOW);  // official Battery_Monitor enable
  if (!pmic::PrepareForSleep()) {
    KDASH_LOG.println("charge: sleep restore FAILED; retry wake capped at 30s");
    seconds = kMinSleepSec;
  }
  Wire.end(); Wire1.end();
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  uint64_t mask = 0;
  const uint8_t held = PressedButtons();
  for (int i = 0; i < 3; ++i) {
    // Exclude a held/stuck key for this sleep to avoid an immediate wake loop.
    if (!(held & (1u << i))) {
      const gpio_num_t pin = (gpio_num_t)(3 + i);
      mask |= 1ULL << (3 + i);
      rtc_gpio_pullup_en(pin); rtc_gpio_pulldown_dis(pin);
    }
  }
  // Timer is always active; GPIO3 is official, GPIO4/5 still need hardware QA.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
  if (mask) esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  KDASH_LOG.printf("sleep: seconds=%lld\n", (long long)seconds);
  KDASH_LOG.flush();
  esp_deep_sleep_start();
}

}  // namespace power
}  // namespace kdash
#else
// Host/CI guard: deep sleep needs the ESP32 toolchain.
#endif
