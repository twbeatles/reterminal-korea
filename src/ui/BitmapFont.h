#pragma once
// BitmapFont: size-specific 1/2bpp coverage tables. Glyphs sorted by
// codepoint for binary search; missing => fallback (U+25A1 □).
// The generator tools/font_subset/make_bitmap_font.py emits these tables;
// this header defines the runtime view + UTF-8 decoding. Portable.
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace font {

struct GlyphInfo {
  uint32_t cp = 0;
  uint32_t offset = 0;  // byte offset into bitmap
  uint8_t w = 0, h = 0;
  int8_t x_off = 0, y_off = 0;
  uint8_t advance = 0;
};

struct BitmapFont {
  const GlyphInfo* glyphs = nullptr;
  uint16_t count = 0;
  const uint8_t* bitmap = nullptr;
  size_t bitmap_len = 0;
  uint8_t line_height = 0, ascent = 0;
  uint32_t fallback_cp = 0x25A1;
  uint8_t bits_per_pixel = 1;  // legacy 1bpp or four-level antialiased coverage
};

// UTF-8 decode: returns codepoint, advances *p. Truncated/invalid => U+FFFD.
uint32_t Utf8Next(const char*& p);
const GlyphInfo* FindGlyph(const BitmapFont& f, uint32_t cp);
// Advance sum for a NUL-terminated string (fallback advance when missing).
int Measure(const BitmapFont& f, const char* utf8);
// Number of codepoints in utf8 missing from f (for portal warnings).
int MissingGlyphs(const BitmapFont& f, const char* utf8);

}  // namespace font
}  // namespace kdash
