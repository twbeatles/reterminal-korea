#include "SymbolBatch.h"

namespace kdash {
namespace app {

bool StopsBatch(Outcome o, bool transport_error) {
  if (transport_error) return true;
  return o == Outcome::AuthError || o == Outcome::RateLimited ||
         o == Outcome::QuotaExceeded;
}

bool SymbolBatch::Add(size_t index, Outcome o, bool transport_error) {
  if (stopped) return true;
  if (StopsBatch(o, transport_error)) {
    // A transport failure carries no provider verdict; report it as ProviderError.
    Stop(o == Outcome::Ok ? Outcome::ProviderError : o);
    return true;
  }
  if (o == Outcome::Ok) {
    ok++;
    return false;
  }
  failed++;
  if (index < 8) fail_mask = (uint8_t)(fail_mask | (1u << index));
  if (first_failure == Outcome::Ok) first_failure = o;
  return false;
}

void SymbolBatch::Stop(Outcome o) {
  if (stopped) return;
  stopped = true;
  stop_outcome = o == Outcome::Ok ? Outcome::ProviderError : o;
}

Outcome SymbolBatch::Result() const {
  if (stopped) return stop_outcome;
  if (ok > 0) return Outcome::Ok;
  return failed > 0 ? first_failure : Outcome::Ok;
}

}  // namespace app
}  // namespace kdash
