#pragma once
// Icons: 1bpp Material Symbols bitmaps (ADR 0008). Portable.
// IconId is defined by the generated table (tools/icon_subset/make_icons.py).
// Icons always accompany a text label; a missing icon draws nothing.
#include <stdint.h>

#include "Canvas.h"
#include "icons/IconData.h"

namespace kdash {
namespace ui {

enum class IconSize : uint8_t { S24, L64 };

int IconPx(IconSize s);  // 24 / 64
bool HasIcon(IconId id, IconSize s);
// Top-left at (x, y); pixels outside the canvas are clipped.
void DrawIcon(Canvas& c, IconId id, IconSize s, int x, int y);

}  // namespace ui
}  // namespace kdash
