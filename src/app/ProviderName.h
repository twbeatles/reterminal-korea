#pragma once
// Provider display names for logs and the device page (F-08).
// Shared by UART logs and T04 device rendering ("KMA", "Air", ...).
#include "../power/Schedule.h"

namespace kdash {
namespace app {

const char* ToString(sched::Provider p);

}  // namespace app
}  // namespace kdash
