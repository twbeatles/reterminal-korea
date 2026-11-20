#include "Watchdog.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <esp_task_wdt.h>

namespace kdash {
namespace wdt {
namespace { bool registered = false; }

bool Begin() {
  if (registered) return true;
  uint32_t idle_mask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  idle_mask |= 1;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  idle_mask |= 2;
#endif
  esp_task_wdt_config_t cfg = {
      .timeout_ms = kTimeoutSec * 1000,
      .idle_core_mask = idle_mask,
      .trigger_panic = true,
  };
  esp_err_t err = esp_task_wdt_init(&cfg);
  if (err == ESP_ERR_INVALID_STATE) err = esp_task_wdt_reconfigure(&cfg);
  if (err != ESP_OK) return false;
  // A configured timer does not supervise loopTask until it is subscribed.
  err = esp_task_wdt_status(nullptr);
  registered = err == ESP_OK || esp_task_wdt_add(nullptr) == ESP_OK;
  return registered;
}
void Feed() { if (registered) esp_task_wdt_reset(); }
void End() {
  if (registered) esp_task_wdt_delete(nullptr);
  registered = false;  // keep SDK/idle task watchdog intact
}

}  // namespace wdt
}  // namespace kdash
#else
namespace kdash {
namespace wdt {
bool Begin() { return true; }
void Feed() {}
void End() {}
}  // namespace wdt
}  // namespace kdash
#endif
