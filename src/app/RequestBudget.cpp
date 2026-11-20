#include "RequestBudget.h"

#include "Fetcher.h"

namespace kdash {
namespace budget {

int CapFor(sched::Provider p) {
  switch (p) {
    case sched::Provider::Exim: return kEximCap;
    case sched::Provider::FscClose: return kFscCap;
    case sched::Provider::KisPrice: return kKisCap;
    default: return kOtherCap;
  }
}

bool ShouldDefer(sched::Provider p, int spent_for_p, int spent_total,
                 int pass_max) {
  if (spent_for_p >= CapFor(p)) return true;
  return spent_total >= pass_max;
}

}  // namespace budget
}  // namespace kdash
