#include "BitmapFont.h"

namespace kdash {
namespace font {

uint32_t Utf8Next(const char*& p) {
  if (!p || !*p) return 0;
  const unsigned char* s = (const unsigned char*)p;
  unsigned char c = *s;
  if (c < 0x80) {
    p += 1;
    return c;
  }
  uint32_t cp = 0;
  int need = 0;
  if ((c & 0xE0) == 0xC0) {
    cp = c & 0x1F;
    need = 1;
  } else if ((c & 0xF0) == 0xE0) {
    cp = c & 0x0F;
    need = 2;
  } else if ((c & 0xF8) == 0xF0) {
    cp = c & 0x07;
    need = 3;
  } else {
    p += 1;
    return 0xFFFD;
  }
  for (int i = 1; i <= need; i++) {
    unsigned char d = s[i];
    if (d == 0 || (d & 0xC0) != 0x80) {
      p += 1;  // truncated/invalid: consume one byte, emit FFFD
      return 0xFFFD;
    }
    cp = (cp << 6) | (d & 0x3F);
  }
  p += 1 + need;
  // Reject overlongs/surrogates/out-of-range.
  if ((need == 1 && cp < 0x80) || (need == 2 && cp < 0x800) ||
      (need == 3 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF) ||
      cp > 0x10FFFF)
    return 0xFFFD;
  return cp;
}

const GlyphInfo* FindGlyph(const BitmapFont& f, uint32_t cp) {
  if (!f.glyphs || !f.count) return nullptr;
  int lo = 0, hi = (int)f.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (f.glyphs[mid].cp == cp) return &f.glyphs[mid];
    if (f.glyphs[mid].cp < cp)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  // Fallback lookup (non-recursive).
  if (cp != f.fallback_cp) {
    lo = 0;
    hi = (int)f.count - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      if (f.glyphs[mid].cp == f.fallback_cp) return &f.glyphs[mid];
      if (f.glyphs[mid].cp < f.fallback_cp)
        lo = mid + 1;
      else
        hi = mid - 1;
    }
  }
  return nullptr;
}

int Measure(const BitmapFont& f, const char* utf8) {
  if (!utf8) return 0;
  int w = 0;
  const char* p = utf8;
  while (*p) {
    uint32_t cp = Utf8Next(p);
    if (!cp) break;
    const GlyphInfo* g = FindGlyph(f, cp);
    w += g ? g->advance : f.line_height / 2;
  }
  return w;
}

int MissingGlyphs(const BitmapFont& f, const char* utf8) {
  if (!utf8) return 0;
  int n = 0;
  const char* p = utf8;
  while (*p) {
    uint32_t cp = Utf8Next(p);
    if (!cp) break;
    if (cp == 0xFFFD) continue;
    const GlyphInfo* g = nullptr;
    if (f.glyphs && f.count) {
      int lo = 0, hi = (int)f.count - 1;
      while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f.glyphs[mid].cp == cp) {
          g = &f.glyphs[mid];
          break;
        }
        if (f.glyphs[mid].cp < cp)
          lo = mid + 1;
        else
          hi = mid - 1;
      }
    }
    if (!g) n++;
  }
  return n;
}

}  // namespace font
}  // namespace kdash
