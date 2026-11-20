#!/usr/bin/env python3
"""Bitmap font generator: TTF -> size-specific 1/2bpp C coverage arrays.

Usage:
  python tools/font_subset/make_bitmap_font.py --ttf <path-or-url> \\
      --size 14 --out src/ui/fonts/font_kr_14.h [--chars FILE]

Notes:
  - The OFL TTF itself is never committed (reuse subset.py download +
    license check). Only the generated .h lands in the tree.
  - Raster: Pillow ImageFont.truetype -> binary or four-level coverage, per-glyph
    bbox crop. Deterministic: same input bytes -> same output bytes.
  - Output struct matches src/ui/BitmapFont.h (GlyphInfo sorted by cp).
  - Prints a capacity report line (also savable to evidence/p4/...).
"""

import argparse
import hashlib
import os
import re
import sys
from pathlib import Path

CHARSETS = {
    14: "ascii+symbols+korean2350",
    18: "ascii+symbols+fixed-phrases+korean2350",
    40: "digits+symbols",
    64: "digits+symbols",
}

ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
# ㎍/㎥ (U+338D/U+33A5) are native IBM Plex Sans KR glyphs; the source font
# lacks U+00B5, whose hand-drawn fallback below stays only for old strings.
SYMBOLS = "°·µ³㎍㎥%/–·□▶◀●○….,-+:%C"
FIXED = "기온습도강수확률미세먼지초미실내환율고시전일종가현재가관측수신마지막설정필요확인오래된정보정상값시간월화수목금토일맑음구름많음흐림비눈소나기없음좋음보통나쁨매우장중장마감휴장가능DEMO예시수치기기연결끊김배터리시계장치"


def charset_for(size, extra=""):
    if size in (40, 64):
        return "0123456789.,-+:%°C" + extra
    base = ASCII + SYMBOLS + FIXED + extra
    if size in (14, 18):
        try:
            from fontTools import subset  # noqa: F401
        except Exception:
            pass
        # KS X 1001 rows 16..40: exactly 2,350 Hangul syllables, not the
        # entire 11,172-syllable Unicode block.
        base += "".join(
            bytes((lead, trail)).decode("euc_kr")
            for lead in range(0xB0, 0xC9)
            for trail in range(0xA1, 0xFF)
        )
    return "".join(sorted(set(base)))


def pack_coverage(image, bpp):
    """MSB-first rows, zero padding; 2bpp retains native FreeType edge coverage."""
    if bpp not in (1, 2):
        raise ValueError("coverage bpp must be 1 or 2")
    pixels = image.convert("L")
    out = bytearray()
    for y in range(pixels.height):
        acc = bits = 0
        for x in range(pixels.width):
            value = pixels.getpixel((x, y))
            ink = int(value >= 128) if bpp == 1 else (value * 3 + 127) // 255
            acc = (acc << bpp) | ink
            bits += bpp
            if bits == 8:
                out.append(acc)
                acc = bits = 0
        if bits:
            out.append(acc << (8 - bits))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ttf", required=True)
    ap.add_argument("--size", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--chars", default="")
    ap.add_argument("--name", default="")
    ap.add_argument("--bpp", type=int, choices=(1, 2), default=1)
    a = ap.parse_args()
    if a.size not in CHARSETS:
        ap.error("size must be 14, 18, 40 or 64")
    name = a.name or ("font_kr_%d" % a.size)
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        ap.error("name must be a C++ identifier")
    from subset import check_ofl

    check_ofl(a.ttf)
    extra = ""
    if a.chars and os.path.exists(a.chars):
        with open(a.chars, encoding="utf-8") as f:
            extra = f.read()
    text = charset_for(a.size, extra)
    try:
        from PIL import Image, ImageFont
    except ImportError:
        print("need Pillow: pip install pillow", file=sys.stderr)
        return 2
    font = ImageFont.truetype(a.ttf, a.size)
    glyphs = []
    bitmap = bytearray()
    for ch in text:
        cp = ord(ch)
        if ch == "µ":
            # Original code-native micro-sign raster: the OFL source lacks
            # U+00B5. Do not silently emit the font's .notdef box as a unit.
            rows = (
                0b10001,
                0b10001,
                0b10001,
                0b10001,
                0b11011,
                0b10101,
                0b10000,
                0b10000,
            )
            from PIL import Image

            micro = Image.new("1", (5, 8), 0)
            for yy, row in enumerate(rows):
                for xx in range(5):
                    micro.putpixel((xx, yy), 1 if row & (16 >> xx) else 0)
            width, height = max(5, a.size // 2), max(8, a.size * 3 // 4)
            micro = micro.resize((width, height), Image.Resampling.NEAREST)
            offset = len(bitmap)
            bitmap.extend(pack_coverage(micro, a.bpp))
            glyphs.append((cp, offset, width, height, 0, a.size - height, width + 1))
            continue
        try:
            bbox = font.getbbox(ch, anchor="lt")
        except Exception:
            continue
        x0, y0, x1, y1 = bbox
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        if w == 0 or h == 0:
            glyphs.append(
                (cp, len(bitmap), 0, 0, 0, 0, max(1, round(font.getlength(ch))))
            )
            continue
        img = Image.new("L", (w, h), 0)
        from PIL import ImageDraw

        d = ImageDraw.Draw(img)
        d.text((-x0, -y0), ch, font=font, fill=255, anchor="lt")
        off = len(bitmap)
        bitmap.extend(pack_coverage(img, a.bpp))
        adv = max(w + max(x0, 0), round(font.getlength(ch)))
        if w > 255 or h > a.size or adv > 255:
            raise ValueError("glyph metrics exceed target line: U+%04X" % cp)
        glyphs.append((cp, off, w, h, x0, max(0, a.size - h), adv))
    glyphs.sort()
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("// Auto-generated by tools/font_subset/make_bitmap_font.py\n")
        f.write(
            "// K-Dash Raster (derived from IBM font software); OFL-1.1. See OFL.txt.\n"
        )
        f.write(
            "// Input SHA256: %s\n"
            % hashlib.sha256(Path(a.ttf).read_bytes()).hexdigest()
        )
        f.write(
            "// size=%d glyphs=%d bitmap=%d bytes bpp=%d\n"
            % (a.size, len(glyphs), len(bitmap), a.bpp)
        )
        f.write('#pragma once\n#include "../BitmapFont.h"\n\n')
        f.write("namespace kdash { namespace fonts {\n\n")
        f.write("static const font::GlyphInfo k%s_glyphs[] = {\n" % name)
        for cp, off, w, h, xo, yo, adv in glyphs:
            f.write(
                "  {0x%X, %d, %d, %d, %d, %d, %d},\n" % (cp, off, w, h, xo, yo, adv)
            )
        f.write("};\n\nstatic const uint8_t k%s_bits[] = {\n" % name)
        for i in range(0, len(bitmap), 16):
            f.write("  " + ", ".join("0x%02X" % b for b in bitmap[i : i + 16]) + ",\n")
        f.write("};\n\nstatic const font::BitmapFont k%s = {\n" % name)
        f.write(
            "  k%s_glyphs, %d, k%s_bits, sizeof(k%s_bits), %d, %d, 0x25A1, %d,\n"
            % (name, len(glyphs), name, name, a.size, a.size - 2, a.bpp)
        )
        f.write("};\n\n}  // namespace fonts\n}  // namespace kdash\n")
    digest = hashlib.sha256(bitmap).hexdigest()[:16]
    print(
        "font-report: size=%d glyphs=%d bitmap=%dB sha=%s charset=%s"
        % (a.size, len(glyphs), len(bitmap), digest, CHARSETS.get(a.size, "?"))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
