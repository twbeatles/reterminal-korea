"""Icon bitmap generator: determinism, input pin, committed-header drift."""

import importlib.util
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "tools/icon_subset/make_icons.py"
TTF = ROOT / "build/icons/MaterialSymbolsOutlined.ttf"
HEADER = ROOT / "src/ui/icons/IconData.h"


def generator():
    spec = importlib.util.spec_from_file_location("kdash_icon_generator", GEN)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(ttf, out):
    return subprocess.run(
        [sys.executable, str(GEN), "--ttf", str(ttf), "--out", str(out)],
        capture_output=True,
        text=True,
    )


def need_ttf():
    if not TTF.exists():
        pytest.skip("BLOCKED: icon TTF not downloaded (see make_icons.py --help)")


def test_icon_table_order_and_large_set():
    mod = generator()
    names = [n for n, _cp, _large in mod.ICONS]
    assert names[:7] == [
        "Sun",
        "PartlyCloudy",
        "Cloud",
        "Rain",
        "Snow",
        "RainSnow",
        "Unknown",
    ]
    assert len(set(names)) == len(names)
    large = {n for n, _cp, big in mod.ICONS if big}
    assert large == set(names[:7]) | {"Usd", "Jpy"}


def test_committed_header_matches_table_and_budget():
    mod = generator()
    text = HEADER.read_text(encoding="utf-8")
    assert "Apache-2.0" in text and mod.TTF_SHA256 in text
    enum = re.search(r"enum class IconId : uint8_t \{([^}]*)\}", text).group(1)
    ids = [s.strip() for s in enum.split(",") if s.strip()]
    assert ids == ["None"] + [n for n, _c, _l in mod.ICONS] + ["Count"]
    total = sum(
        len(re.findall(r"0x[0-9A-F]{2}", body))
        for body in re.findall(r"static const uint8_t kI\w+\[\] = \{([^}]*)\}", text)
    )
    assert 0 < total < 8 * 1024


def test_generation_is_deterministic_and_matches_committed(tmp_path):
    need_ttf()
    a, b = tmp_path / "a.h", tmp_path / "b.h"
    assert run(TTF, a).returncode == 0
    assert run(TTF, b).returncode == 0
    assert a.read_bytes() == b.read_bytes()
    assert a.read_bytes() == HEADER.read_bytes()


def test_rejects_unpinned_input(tmp_path):
    need_ttf()
    bad = tmp_path / "bad.ttf"
    shutil.copyfile(TTF, bad)
    with open(bad, "r+b") as f:
        f.seek(-1, 2)
        last = f.read(1)
        f.seek(-1, 2)
        f.write(bytes([last[0] ^ 0xFF]))
    r = run(bad, tmp_path / "x.h")
    assert r.returncode != 0
    assert "sha256" in (r.stdout + r.stderr).lower()
