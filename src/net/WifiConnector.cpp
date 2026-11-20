#include "WifiConnector.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>

#include "../boot/SerialLog.h"
#include "../drivers/Pmic.h"
#include "WifiSupervisor.h"

namespace kdash {
namespace net {

namespace {
WifiSupervisor g_sup;
std::atomic<int> g_last_reason{0};
std::atomic<int> g_pending_reason{0};  // latest disconnect reason, 0=no event
bool g_events_registered = false;
bool g_ever_connected = false;
bool g_logged_lost = false;
uint32_t g_lost_since_ms = 0;

void OnWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    const int reason = (int)info.wifi_sta_disconnected.reason;
    g_last_reason.store(reason);
    g_pending_reason.store(reason);
  }
}

// Radio settings that favour stability on USB power; the SSID/password are never logged.
void ConfigureRadio(bool usb) {
  if (!g_events_registered) {
    WiFi.onEvent(OnWifiEvent);
    g_events_registered = true;
  }
  WiFi.persistent(false);        // do not wear flash with credentials on every begin
  WiFi.setAutoReconnect(true);   // driver retries on its own between our attempts
  WiFi.setSleep(!usb);           // no modem power-save on USB only (fewer missed beacons); battery keeps it
}

void StartConnect(const Settings& s, bool reset_radio, bool usb) {
  if (reset_radio) {
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    delay(150);
  }
  WiFi.mode(WIFI_STA);
  ConfigureRadio(usb);
  WiFi.begin(s.wifi_ssid, s.wifi_secret);
}
}  // namespace

bool ConnectWifi(const Settings& s) {
  if (!s.wifi_ssid[0]) return false;
  if (WiFi.status() == WL_CONNECTED) return true;
  // Modem power-save follows the real power source (battery keeps it on, ISSUE-010).
  // A keep-alive attempt already in flight is awaited, not restarted (G-4).
  const uint32_t start = millis();
  if (!g_sup.AttemptInFlight(start)) StartConnect(s, false, pmic::UsbAttached());
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiAttemptTimeoutMs) delay(200);
  const wl_status_t st = WiFi.status();
  // Diagnostics only: status code, RSSI and elapsed time. SSID/password are never logged.
  if (st == WL_CONNECTED) {
    KDASH_LOG.printf("wifi: connected rssi=%d ms=%lu\n", (int)WiFi.RSSI(),
                     (unsigned long)(millis() - start));
  } else {
    KDASH_LOG.printf("wifi: FAILED status=%d reason=%d ms=%lu\n", (int)st, g_last_reason.load(),
                     (unsigned long)(millis() - start));
    g_sup.NoteBlockingFailure(millis());
  }
  return st == WL_CONNECTED;
}

void MaintainWifi(const Settings& s, bool usb) {
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  const int reason = g_pending_reason.exchange(0);  // log outside event task
  if (reason) {
    KDASH_LOG.printf("wifi: disconnected reason=%d\n", reason);
  }
  if (connected) {
    if (g_logged_lost)
      KDASH_LOG.printf("wifi: link restored after_ms=%lu rssi=%d\n",
                       (unsigned long)(now - g_lost_since_ms), (int)WiFi.RSSI());
    g_ever_connected = true;
    g_logged_lost = false;
  } else if (g_ever_connected && !g_logged_lost) {
    g_logged_lost = true;
    g_lost_since_ms = now;
    KDASH_LOG.printf("wifi: link lost status=%d\n", (int)WiFi.status());
  }
  switch (g_sup.Tick(now, connected, usb, s.wifi_ssid[0] != '\0')) {
    case WifiAction::Begin:
      KDASH_LOG.printf("wifi: reconnect attempt=%d failures=%d\n", g_sup.attempts(), g_sup.failures());
      StartConnect(s, false, usb);
      break;
    case WifiAction::ResetAndBegin:
      KDASH_LOG.printf("wifi: radio reset + reconnect attempt=%d failures=%d\n", g_sup.attempts(),
                       g_sup.failures());
      StartConnect(s, true, usb);
      break;
    case WifiAction::None:
      break;
  }
}

bool TakeWifiRedrawRequest() { return g_sup.TakeRedrawRequest(millis()); }

}  // namespace net
}  // namespace kdash
#else
// Host/CI guard: Wi-Fi radio needs the ESP32 toolchain.
#endif
