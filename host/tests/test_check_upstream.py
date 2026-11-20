"""tools/check_upstream.py: pinned Seeed SHA drift classification (offline, fake API)."""

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "check_upstream", ROOT / "tools/check_upstream.py"
)
cu = importlib.util.module_from_spec(spec)
sys.modules["check_upstream"] = cu
spec.loader.exec_module(cu)


def test_pinned_sha_is_read_from_board_doc():
    sha = cu.pinned_sha(ROOT / "hardware" / "board_e1001.md")
    assert len(sha) == 40 and all(c in "0123456789abcdef" for c in sha)


def test_web_only_changes_are_not_relevant():
    files = ["README.md", "web/js/firmwares.js", "web/assets/platforms/x.svg"]
    assert cu.relevant(files) == []


def test_e1001_firmware_sources_are_relevant():
    files = [
        "examples/base/Battery_Monitor/Battery_Monitor.ino",
        "examples/official/LVGLePaperStatusPanel/platformio.ini",
        "examples/official/SenseCraft_HMI/src/boards/reterminal_e1001/config.h",
        "examples/official/SenseCraft_HMI/lib/SY6974/SY6974.cpp",
        "examples/official/SenseCraft_HMI/src/boards/reterminal_e1004/config.h",
        "web/js/firmwares.js",
    ]
    assert cu.relevant(files) == files[:4]


def test_report_uses_compare_result_without_network():
    def fake_get(url):
        assert "/compare/" in url
        return {
            "status": "ahead",
            "ahead_by": 1,
            "files": [
                {"filename": "web/js/firmwares.js"},
                {"filename": "examples/base/UserButtons/UserButtons.ino"},
            ],
        }

    code, lines = cu.report("a" * 40, fake_get)
    assert code == 1
    assert any("UserButtons" in line for line in lines)


def test_report_ok_when_head_equals_pin():
    code, lines = cu.report(
        "b" * 40, lambda url: {"status": "identical", "ahead_by": 0, "files": []}
    )
    assert code == 0 and "identical" in lines[0]
