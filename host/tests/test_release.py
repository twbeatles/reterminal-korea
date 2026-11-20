"""Release guards (08/P6): board pinning + version stamping."""

import json
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")


def _read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return f.read()


def test_only_e1001_is_packaged():
    ini = _read("platformio.ini")
    assert "board = seeed_xiao_esp32s3" in ini  # 공식 예제와 동일
    assert "qio_opi" in ini and "RETERMINAL_E1001" in ini
    default = ini.split("[env:reterminal_e1001]")[1].split("[env:")[0]
    for other in ("E1002", "E1003", "E1004"):
        assert other not in default  # 교차 플래시 방지


def test_version_script_stamps_and_rejects():
    out = os.path.join(os.path.dirname(__file__), "version.probe.json")
    r = subprocess.run(
        [sys.executable, "scripts/version.py", "--version", "0.2.0", "--out", out],
        capture_output=True,
        text=True,
        cwd=ROOT,
    )
    assert r.returncode == 0, r.stderr
    with open(out, encoding="utf-8") as f:
        doc = json.load(f)
    assert doc["version"] == "0.2.0" and doc["board"] == "reterminal_e1001"
    assert "flash_layout" in doc and "e1002" in doc["forbidden_boards"]
    os.remove(out)
    bad = subprocess.run(
        [sys.executable, "scripts/version.py", "--version", "1.0", "--out", out],
        capture_output=True,
        text=True,
        cwd=ROOT,
    )
    assert bad.returncode != 0  # semver 강제
    no = subprocess.run(
        [
            sys.executable,
            "scripts/version.py",
            "--version",
            "0.2.0",
            "--out",
            out,
            "--board",
            "e1003",
        ],
        capture_output=True,
        text=True,
        cwd=ROOT,
    )
    assert no.returncode != 0  # E1003 패키징 거부


def test_upstream_pin_table_is_recorded():
    doc = _read("hardware/board_e1001.md")
    for pin in (
        "KEY0",
        "KEY1",
        "KEY2",
        "| 3 |",
        "| 4 |",
        "| 5 |",
        "GPIO0",
        "19 / 20",
        "0x44",
        "0x51",
        "GDEY075T7",
        "520",
        "8d97d891",
    ):
        assert pin in doc, pin
    assert "미확정 플레이스홀더" not in doc and "빈 칸" not in doc
