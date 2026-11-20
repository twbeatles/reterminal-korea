#include "Icons.h"

namespace kdash {
namespace ui {
namespace {

const uint8_t* Bits(IconId id, IconSize s) {
  const int i = (int)id;
  if (i <= 0 || i >= (int)IconId::Count) return nullptr;
  return s == IconSize::L64 ? icondata::k64[i] : icondata::k24[i];
}

}  // namespace

int IconPx(IconSize s) { return s == IconSize::L64 ? 64 : 24; }

bool HasIcon(IconId id, IconSize s) { return Bits(id, s) != nullptr; }

void DrawIcon(Canvas& c, IconId id, IconSize s, int x, int y) {
  const uint8_t* bits = Bits(id, s);
  if (!bits) return;
  const int n = IconPx(s);
  const int stride = (n + 7) / 8;
  for (int gy = 0; gy < n; gy++)
    for (int gx = 0; gx < n; gx++)
      if (bits[gy * stride + gx / 8] & (0x80 >> (gx % 8)))
        c.pixel(x + gx, y + gy, true);
}

}  // namespace ui
}  // namespace kdash
