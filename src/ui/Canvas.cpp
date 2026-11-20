#include "Canvas.h"

#include <stdio.h>
#include <string.h>

#include "../storage/CacheStore.h"

namespace kdash {
namespace ui {

Canvas::Canvas() { clear(true); }

void Canvas::clear(bool white) {
  memset(fb_, white ? 0 : 0xFF, sizeof(fb_));
  missing_ = 0;
}

void Canvas::pixel(int x, int y, bool black) {
  coverage(x, y, black ? 3 : 0);
}

void Canvas::coverage(int x, int y, uint8_t ink) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  const size_t index = (size_t)y * W + (size_t)x;
  const unsigned shift = 6 - (index % 4) * 2;
  fb_[index / 4] = (uint8_t)((fb_[index / 4] & ~(3u << shift)) |
                            ((ink > 3 ? 3u : ink) << shift));
}

bool Canvas::pixel(int x, int y) const {
  return coverage(x, y) >= 2;
}

uint8_t Canvas::coverage(int x, int y) const {
  if (x < 0 || y < 0 || x >= W || y >= H) return 0;
  const size_t index = (size_t)y * W + (size_t)x;
  return (uint8_t)((fb_[index / 4] >> (6 - (index % 4) * 2)) & 3u);
}

void Canvas::hline(int x, int y, int w) {
  for (int i = 0; i < w; i++) pixel(x + i, y, true);
}
void Canvas::vline(int x, int y, int h) {
  for (int i = 0; i < h; i++) pixel(x, y + i, true);
}
void Canvas::rect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  hline(x, y, w);
  hline(x, y + h - 1, w);
  vline(x, y, h);
  vline(x + w - 1, y, h);
}
void Canvas::fill_rect(int x, int y, int w, int h, bool black) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) pixel(x + i, y + j, black);
}
void Canvas::invert_rect(int x, int y, int w, int h) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      int px = x + i, py = y + j;
      if (px < 0 || py < 0 || px >= W || py >= H) continue;
      coverage(px, py, (uint8_t)(3 - coverage(px, py)));
    }
}

int Canvas::draw_text(const font::BitmapFont& f, int x, int y,
                      const char* utf8) {
  if (!utf8) return 0;
  int cx = x;
  int adv_fb = f.line_height ? f.line_height / 2 : 8;
  const char* p = utf8;
  while (*p) {
    uint32_t cp = font::Utf8Next(p);
    if (!cp) break;
    const font::GlyphInfo* g = font::FindGlyph(f, cp);
    if (!g || g->cp != cp) missing_++;  // absent or drawn as the □ fallback
    int adv = g ? g->advance : adv_fb;
    if (g && f.bitmap) {
      const unsigned bpp = f.bits_per_pixel;
      const size_t stride = ((size_t)g->w * bpp + 7) / 8;
      const size_t bytes = stride * g->h;
      if ((bpp == 1 || bpp == 2) && g->offset <= f.bitmap_len && bytes <= f.bitmap_len - g->offset) {
        for (int gy = 0; gy < g->h; gy++)
          for (int gx = 0; gx < g->w; gx++) {
            const unsigned bit = (unsigned)gx * bpp;
            const uint8_t packed = f.bitmap[g->offset + (size_t)gy * stride + bit / 8];
            const uint8_t ink = bpp == 1 ? ((packed >> (7 - bit % 8)) & 1u) * 3 :
                                          (packed >> (6 - bit % 8)) & 3u;
            const int px = cx + g->x_off + gx, py = y + g->y_off + gy;
            if (ink > coverage(px, py)) coverage(px, py, ink);
          }
      }
      cx += adv;
      if (cx >= W) break;
      continue;
    }
    int gw = g && g->w ? g->w : (adv > 2 ? adv - 2 : adv);
    int gh = g && g->h ? g->h : (f.line_height ? f.line_height - 2 : 10);
    if (gw < 1) gw = 1;
    if (gh < 1) gh = 1;
    // Missing/invalid tables remain visibly distinct from real glyphs.
    rect(cx + 1, y + 1, gw, gh);
    if (cp != ' ' && cp != 0x3000) fill_rect(cx + 2, y + 2, gw > 2 ? 1 : gw,
                                             gh > 2 ? gh - 2 : gh, true);
    cx += adv;
    if (cx >= W) break;
  }
  return cx - x;
}

int Canvas::measure(const font::BitmapFont& f, const char* utf8) const {
  return font::Measure(f, utf8);
}

uint32_t Canvas::crc32() const { return Crc32(fb_, sizeof(fb_)); }

uint32_t Canvas::content_hash(const Rect& ignore) const {
  uint32_t h = 2166136261u;
  const int stride = W / 4;
  const int bx0 = ignore.x / 4, bx1 = (ignore.x + ignore.w + 3) / 4;  // [bx0, bx1)
  for (int y = 0; y < H; ++y) {
    const bool in_rows = y >= ignore.y && y < ignore.y + ignore.h;
    const uint8_t* row = fb_ + (size_t)y * stride;
    for (int b = 0; b < stride; ++b) {
      if (in_rows && b >= bx0 && b < bx1) continue;
      h = (h ^ row[b]) * 16777619u;
    }
  }
  return h;
}

bool Canvas::write_pbm(const char* path) const {
  if (!path) return false;
  FILE* fp = fopen(path, "wb");
  if (!fp) return false;
  fprintf(fp, "P4\n%d %d\n", W, H);
  uint8_t row[W / 8];
  for (int y = 0; y < H; ++y) {
    memset(row, 0, sizeof(row));
    for (int x = 0; x < W; ++x) if (pixel(x, y)) row[x / 8] |= (uint8_t)(0x80 >> (x % 8));
    fwrite(row, 1, sizeof(row), fp);
  }
  fclose(fp);
  return true;
}

bool Canvas::write_pgm(const char* path) const {
  if (!path) return false;
  FILE* fp = fopen(path, "wb");
  if (!fp) return false;
  fprintf(fp, "P5\n%d %d\n255\n", W, H);
  uint8_t row[W];
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) row[x] = (uint8_t)(255 - 85 * coverage(x, y));
    fwrite(row, 1, sizeof(row), fp);
  }
  fclose(fp);
  return true;
}

double Canvas::black_ratio(int x, int y, int w, int h) const {
  if (w <= 0 || h <= 0) return 0;
  long black = 0, total = 0;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      int px = x + i, py = y + j;
      if (px < 0 || py < 0 || px >= W || py >= H) continue;
      total++;
      if (pixel(px, py)) black++;
    }
  return total ? (double)black / (double)total : 0;
}

void FitText(const font::BitmapFont& f, const char* text, const char* suffix,
             int box_w_px, char* out, size_t out_cap) {
  if (!out || !out_cap) return;
  out[0] = '\0';
  if (!text) text = "";
  if (!suffix) suffix = "";
  int sw = font::Measure(f, suffix);
  // Worst case: suffix alone does not fit -> truncate suffix with ellipsis.
  if (sw > box_w_px) {
    // Byte-truncate suffix to ~box width (ASCII-safe cut at UTF-8 boundary).
    size_t n = strlen(suffix);
    while (n > 0 && font::Measure(f, suffix) > box_w_px) {
      // shrink by one codepoint from the end
      size_t m = n - 1;
      while (m > 0 && (suffix[m] & 0xC0) == 0x80) m--;
      n = m;
      if (n == 0) break;
      char tmp[256];
      size_t c = n < sizeof(tmp) - 4 ? n : sizeof(tmp) - 4;
      memcpy(tmp, suffix, c);
      tmp[c] = '\0';
      if (font::Measure(f, tmp) <= box_w_px) {
        strncpy(out, tmp, out_cap - 1);
        out[out_cap - 1] = '\0';  // tiny-cap hardening
        return;
      }
    }
    out[0] = 0;  // (was strncpy of empty string)
    return;
  }
  if (font::Measure(f, text) + sw <= box_w_px) {
    snprintf(out, out_cap, "%s%s", text, suffix);
    return;
  }
  // Shrink text codepoint by codepoint, keep suffix, add "…".
  const char* ell = "…";
  int ellw = font::Measure(f, ell);
  size_t tn = strlen(text);
  char tbuf[512];
  while (tn > 0) {
    size_t c = tn < sizeof(tbuf) - 1 ? tn : sizeof(tbuf) - 1;
    memcpy(tbuf, text, c);
    tbuf[c] = '\0';
    if (font::Measure(f, tbuf) + ellw + sw <= box_w_px) {
      snprintf(out, out_cap, "%s%s%s", tbuf, ell, suffix);
      return;
    }
    // drop last codepoint
    size_t m = tn - 1;
    while (m > 0 && (text[m] & 0xC0) == 0x80) m--;
    if (m >= tn) break;
    tn = m;
  }
  snprintf(out, out_cap, "%s%s", ell, suffix);
}

}  // namespace ui
}  // namespace kdash
