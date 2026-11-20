#pragma once
// Watchdog (F-05): esp_task_wdt wrapper. Device-only; T05 cycle cap and
// crash counter own the policy, this only feeds/resets.
#include <stdint.h>

namespace kdash {
namespace wdt {

inline constexpr int kTimeoutSec = 90;

bool Begin();
void Feed();
void End();

}  // namespace wdt
}  // namespace kdash
