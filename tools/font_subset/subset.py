#!/usr/bin/env python3
"""Korean font subset builder (05/U-10).

Builds a device-sized glyph subset from an OFL-licensed Korean font.
The original font file is NEVER committed (see .gitignore); only the subset
recipe (this script + charset note) and the size report are recorded.

Usage:
  python tools/font_subset/subset.py --font URL-or-path --out build/font --preset kdash

Presets:
  kdash : ASCII + digits + units + required UI labels (docs/05) +
          KST date glyphs + 200 common dong/region chars (extend via --extra-chars).

Requires: pip install fonttools brotli
License check: verifies the font declares OFL in its name table before subsetting.
"""

import argparse
import os
import urllib.request

KDASH_LABELS = (
    "기온 강수확률 초미세먼지 실내 환율(고시) 전일 종가 현재가 "
    "마지막 수신 관측 오래된 정보 확인 필요 미연결 DEMO 마지막 정상값 "
    "설정 필요 기준일 출처 연결됨 배터리 장마감 휴장 서울 종로구 "
    "0123456789°C%µg/m³.-·:()/+ "
)
KDASH_EXTRA = (
    "세종특별자치시 조치원읍 강남구 강북구 서초구 송파구 마포구 "
    "용산구 성동구 광진구 동대문구 중랑구 성북구 강서구 구로구 "
    "금천구 영등포구 동작구 관악구 서대문구 은평구 도봉구 노원구 "
    "부산 대구 인천 광주 대전 울산 수원 성남 고양 용인 창원 "
    "USD JPY KRW 원 달러 엔 종가 시세 측정소 날씨 예보"
)


def fetch(url: str, dest: str) -> None:
    print("downloading %s ..." % url)
    urllib.request.urlretrieve(url, dest)
    print("saved %d bytes -> %s" % (os.path.getsize(dest), dest))


def check_ofl(font_path: str) -> str:
    from fontTools.ttLib import TTFont

    font = TTFont(font_path)
    lic = ""
    for rec in font["name"].names:
        try:
            s = rec.toUnicode()
        except Exception:
            continue
        if "SIL OPEN FONT LICENSE" in s.upper() or "OPEN FONT LICENSE" in s.upper():
            lic = s.strip().split("\n")[0][:120]
            break
    if not lic:
        raise SystemExit("refusing: no OFL declaration found in %s" % font_path)
    print("license: %s" % lic)
    return lic


def build_subset(font_path: str, out_prefix: str, extra: str = "") -> dict:
    from fontTools import subset

    chars = sorted(set(KDASH_LABELS + KDASH_EXTRA + (extra or "")) - {" "} | {" "})
    text = "".join(chars)
    opts = subset.Options()
    opts.flavor = "woff2"
    opts.with_zopfli = False
    font = subset.load_font(font_path, opts)
    ss = subset.Subsetter(opts)
    ss.populate(text=text)
    ss.subset(font)
    ttf_out = out_prefix + ".subset.ttf"
    font.save(ttf_out)
    size = os.path.getsize(ttf_out)
    glyphs = len(font.getGlyphSet())
    print("charset chars: %d  glyphs: %d  subset: %d bytes" % (len(text), glyphs, size))
    return {"chars": len(text), "glyphs": glyphs, "bytes": size, "file": ttf_out}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--font", required=True, help="local path or http(s) URL of OFL font"
    )
    ap.add_argument("--out", required=True, help="output prefix (adds .subset.ttf)")
    ap.add_argument("--preset", default="kdash", choices=["kdash"])
    ap.add_argument("--extra-chars", default="")
    ap.add_argument("--license-note", default="")
    a = ap.parse_args()
    font_path = a.font
    if a.font.startswith(("http://", "https://")):
        os.makedirs("build", exist_ok=True)
        font_path = os.path.join("build", os.path.basename(a.font).split("?")[0])
        if not os.path.exists(font_path):
            fetch(a.font, font_path)
    if not os.path.exists(font_path):
        raise SystemExit("font not found: %s" % font_path)
    lic = check_ofl(font_path)
    rep = build_subset(font_path, a.out, a.extra_chars)
    with open(a.out + ".report.txt", "w", encoding="utf-8") as f:
        f.write(
            "source: %s\nlicense: %s\nnote: %s\n%r\n"
            % (a.font, lic, a.license_note, rep)
        )
    print("report: %s.report.txt" % a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
