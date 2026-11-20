#pragma once
// RequestBudget: per-provider caps within kMaxRequestsPerPass (F-02).
// EXIM walk-back (7) + KIS token/quotes (6) + FSC (4) must never starve
// KMA/AirKorea. Deferred work is NOT recorded as a failure.
#include "../power/Schedule.h"

namespace kdash {
namespace budget {

inline constexpr int kEximCap = 7;
inline constexpr int kFscCap = 4;
inline constexpr int kKisCap = 6;
inline constexpr int kOtherCap = 2;

int CapFor(sched::Provider p);
// True when spending one more request for p would exceed its cap or the
// pass total (caller defers to the next cycle, no MarkResult failure).
bool ShouldDefer(sched::Provider p, int spent_for_p, int spent_total,
                 int pass_max);

}  // namespace budget
}  // namespace kdash
