#pragma once
// Market sessions (F-03). KRX regular 09:00-15:30 is the only confirmed
// session; NXT/extended hours stay UNCONFIRMED (U-08), so every market
// conservatively uses the KRX window until the source table lands.
#include <stdint.h>

#include "../../include/Config.h"

namespace kdash {
namespace fin {

bool IsMarketOpen(Market m, int64_t epoch_utc);

}  // namespace fin
}  // namespace kdash
