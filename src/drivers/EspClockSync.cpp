#include "EspClockSync.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include "../boot/SerialLog.h"
#include "ClockBootstrap.h"
#include "Pcf8563_esp32.h"

namespace kdash {
namespace clock {
namespace {
Decision g_time;
std::atomic<uint32_t> g_ntp_notifications{0};
uint32_t g_last_ntp_attempt_ms = 0;
bool g_attempted_ntp = false;
bool g_rtc_seeded = false;
uint32_t g_clock_applies = 0, g_rtc_writes = 0;
}  // namespace

void NtpSynchronized(struct timeval* tv) {
  if (tv && tv->tv_sec >= KDASH_BUILD_EPOCH)
    g_ntp_notifications.fetch_add(1, std::memory_order_relaxed);
}

class EspClockPlatform : public clock::IClockPlatform {
 public:
  bool ReadRtc(int64_t& epoch, bool& vl) override {
    return pcf::ReadRtcUtc(epoch, vl);
  }
  bool TrustedNtpUtc(int64_t& epoch) override {
    epoch = (int64_t)time(nullptr);
    return g_ntp_notifications.load(std::memory_order_relaxed) != 0 &&
           epoch >= KDASH_BUILD_EPOCH;
  }
  bool SyncNtp(int64_t& epoch) override {
    if (WiFi.status() != WL_CONNECTED) return false;
    const uint32_t before = g_ntp_notifications.load(std::memory_order_relaxed);
    // API verified in the installed ESP-IDF esp_sntp.h; this notification
    // follows an actual SNTP update, unlike checking an RTC-seeded epoch.
    sntp_set_time_sync_notification_cb(NtpSynchronized);
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    const uint32_t start = millis();
    while (g_ntp_notifications.load(std::memory_order_relaxed) == before &&
           (uint32_t)(millis() - start) < 15000) delay(200);
    epoch = (int64_t)time(nullptr);
    return g_ntp_notifications.load(std::memory_order_relaxed) != before &&
           epoch >= KDASH_BUILD_EPOCH;
  }
  bool ApplyUtc(int64_t epoch) override {
    if (g_rtc_seeded) return true;
    ++g_clock_applies;
    struct timeval tv = {};
    tv.tv_sec = (time_t)epoch;
    g_rtc_seeded = settimeofday(&tv, nullptr) == 0;
    return g_rtc_seeded;
  }
  bool WriteRtc(int64_t epoch) override {
    ++g_rtc_writes;
    return pcf::WriteRtcUtc(epoch);
  }
};

bool SyncClock(bool wifi_ready, app::AppState& state) {
  EspClockPlatform platform;
  const uint32_t cadence =
      g_ntp_notifications.load(std::memory_order_relaxed) ? 6u * 3600 * 1000
                                                        : 30u * 60 * 1000;
  const bool attempt = wifi_ready && (!g_attempted_ntp ||
      (uint32_t)(millis() - g_last_ntp_attempt_ms) >= cadence);
  if (attempt) {
    g_attempted_ntp = true;
    g_last_ntp_attempt_ms = millis();
  }
  g_time = clock::BootstrapClock(platform, KDASH_BUILD_EPOCH, attempt);
  state.time_verified = g_time.verified;
  const char* source = g_time.source == clock::Source::Ntp ? "NTP" :
                       g_time.source == clock::Source::Rtc ? "RTC" : "NONE";
  struct timeval current = {};
  gettimeofday(&current, nullptr);
  KDASH_LOG.printf("time: source=%s verified=%d utc=%lld usec=%ld applies=%lu rtc_writes=%lu ntp=%lu\n",
      source, (int)g_time.verified, (long long)current.tv_sec, (long)current.tv_usec,
      (unsigned long)g_clock_applies, (unsigned long)g_rtc_writes,
      (unsigned long)g_ntp_notifications.load(std::memory_order_relaxed));
  return g_time.verified;
}

Decision LastDecision() { return g_time; }

}  // namespace clock
}  // namespace kdash
#else
// Host/CI guard: RTC/NTP hardware needs the ESP32 toolchain.
#endif
