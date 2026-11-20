#pragma once
// Randomness port (ISSUE-010). Device: hardware RNG (esp_random / esp_fill_random
// with RF on). Tests: deterministic fake. Secrets (AP password, CSRF token)
// must come from this port — never from a seeded LCG.
#include <stdint.h>

namespace kdash {

class IRandom {
 public:
  virtual ~IRandom() = default;
  virtual uint32_t next_u32() = 0;
};

// Uniform integer in [0, bound) without modulo bias (rejection sampling).
inline uint32_t UniformBelow(IRandom& rng, uint32_t bound) {
  if (bound < 2) return 0;
  const uint32_t limit = (uint32_t)(0x100000000ull - (0x100000000ull % bound));
  for (;;) {
    uint32_t r = rng.next_u32();
    if (limit == 0 || r < limit) return r % bound;
  }
}

}  // namespace kdash
