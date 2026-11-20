"""Coverage and reproducibility of the shipped bitmap fonts (no device IO)."""

import importlib.util
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "tools/font_subset/make_bitmap_font.py"


def generator():
    spec = importlib.util.spec_from_file_location("kdash_font_generator", GEN)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_charset_is_ks2350_and_generated_tables_are_sorted():
    mod = generator()
    text = mod.charset_for(14)
    hangul = {c for c in text if 0xAC00 <= ord(c) <= 0xD7A3}
    expected = {
        bytes((a, b)).decode("euc_kr")
        for a in range(0xB0, 0xC9)
        for b in range(0xA1, 0xFF)
    }
    assert len(expected) == 2350
    assert expected <= hangul
    assert len(hangul) < 2500
    for size in (14, 18, 64):
        data = (ROOT / f"src/ui/fonts/font_kr_{size}.h").read_text(encoding="utf-8")
        points = [int(cp, 16) for cp in re.findall(r"\{0x([0-9A-F]+),", data)]
        assert points == sorted(set(points))
        assert points
    assert "SIL OPEN FONT LICENSE" in (ROOT / "src/ui/fonts/OFL.txt").read_text(
        encoding="utf-8"
    )


def test_bitmap_generation_is_reproducible(tmp_path):
    # 64px digits ship from SemiBold (14/18px from Medium) since 2026-11-11.
    ttf = ROOT / "build/IBMPlexSansKR-SemiBold.ttf"
    if not ttf.exists():
        pytest.skip(
            "optional original OFL font is not distributed; generated tables are checked"
        )
    outputs = [tmp_path / "one.h", tmp_path / "two.h"]
    for out in outputs:
        subprocess.run(
            [
                sys.executable,
                str(GEN),
                "--ttf",
                str(ttf),
                "--size",
                "64",
                "--out",
                str(out),
            ],
            check=True,
            capture_output=True,
        )
    assert outputs[0].read_bytes() == outputs[1].read_bytes()


def test_coverage_quantization_and_row_padding():
    from PIL import Image

    image = Image.new("L", (5, 2))
    image.putdata([0, 85, 170, 255, 85, 255, 170, 85, 0, 170])
    mod = generator()
    assert mod.pack_coverage(image, 2) == bytes([0x1B, 0x40, 0xE4, 0x80])
    assert mod.pack_coverage(image, 1) == bytes([0x30, 0xC8])
    with pytest.raises(ValueError):
        mod.pack_coverage(image, 3)
