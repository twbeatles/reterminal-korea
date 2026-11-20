#pragma once
// Canvas: 800x480 2bpp framebuffer, 0=white .. 3=black, MSB first.
// Portable; device allocates in PSRAM, host on the heap. T04c.
#include <stddef.h>
#include <stdint.h>

#include "../../include/IDisplay.h"
#include "BitmapFont.h"

namespace kdash {
namespace ui {

class Canvas {
 public:
  static constexpr int W = 800, H = 480;
  static constexpr size_t kBytes = (size_t)W * H / 4;  // 96,000, PSRAM on E1001
  Canvas();
  void clear(bool white = true);
  void pixel(int x, int y, bool black);
  bool pixel(int x, int y) const;
  uint8_t coverage(int x, int y) const;
  void coverage(int x, int y, uint8_t ink);
  void hline(int x, int y, int w);
  void vline(int x, int y, int h);
  void rect(int x, int y, int w, int h);
  void fill_rect(int x, int y, int w, int h, bool black = true);
  void invert_rect(int x, int y, int w, int h);
  // Draws NUL-terminated UTF-8; returns advance px. Missing glyphs draw
  // as an outlined box (fallback), never blank.
  int draw_text(const font::BitmapFont& f, int x, int y, const char* utf8);
  int measure(const font::BitmapFont& f, const char* utf8) const;
  // Fallback boxes drawn since the last clear() (glyph-coverage oracle).
  int missing_glyphs() const { return missing_; }
  uint32_t crc32() const;
  // FNV-1a over the framebuffer, skipping every byte that overlaps `ignore`
  // (used to ignore the header clock when deciding whether a panel refresh is needed).
  uint32_t content_hash(const Rect& ignore) const;
  const uint8_t* data() const { return fb_; }
  // P4 binary PBM for host previews.
  bool write_pbm(const char* path) const;
  bool write_pgm(const char* path) const;
  // Black-pixel ratio inside a rect (for badge/geometry oracle tests).
  double black_ratio(int x, int y, int w, int h) const;

 private:
  uint8_t fb_[kBytes];
  int missing_ = 0;
};

// TextLayout: fit utf8 into box_w px, appending "…" while keeping the
// protected suffix (unit/기준일) intact (docs/05).
void FitText(const font::BitmapFont& f, const char* text, const char* suffix,
             int box_w_px, char* out, size_t out_cap);

}  // namespace ui
}  // namespace kdash
