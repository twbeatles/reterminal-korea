#include "Layout.h"

namespace kdash {
namespace ui {

bool NeedsEllipsis(int text_advance_px, int box_w_px) {
  return text_advance_px > box_w_px;
}

int FitChars(int text_len, int avg_advance_px, int box_w_px) {
  if (text_len <= 0 || avg_advance_px <= 0 || box_w_px <= 0) return 0;
  int fit = box_w_px / avg_advance_px;
  return fit < text_len ? fit : text_len;
}

}  // namespace ui
}  // namespace kdash
