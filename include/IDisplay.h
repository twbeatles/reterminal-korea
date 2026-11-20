#pragma once
// Display abstraction: 800x480, 1bpp text-first + 4-level gray aux.
// Providers never see pixels; UI never performs HTTPS (02_ARCHITECTURE_CONTRACTS).
#include <stdint.h>

namespace kdash {

inline constexpr int kDisplayW = 800;
inline constexpr int kDisplayH = 480;

enum class RefreshMode : uint8_t { Full, Partial };

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
};

class IDisplay {
 public:
  virtual ~IDisplay() = default;
  virtual void begin() = 0;
  // Returns false when the panel reports failure; last_rendered_at is then
  // NOT updated (05: 수집완료 vs 전자잉크 갱신완료 분리).
  virtual bool refresh(const Rect& region, RefreshMode mode) = 0;
  virtual void full_refresh() = 0;
};

}  // namespace kdash
