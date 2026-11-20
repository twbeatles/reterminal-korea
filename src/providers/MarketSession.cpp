#include "MarketSession.h"

#include "FinanceParsers.h"

namespace kdash {
namespace fin {

bool IsMarketOpen(Market m, int64_t epoch_utc) {
  (void)m;  // U-08: NXT/통합 hours unconfirmed -> KRX window for all.
  return IsKrxOpen(epoch_utc);
}

}  // namespace fin
}  // namespace kdash
