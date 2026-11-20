"""UI-01/02 + SEC-01 + layout geometry (05/07).

Renders the 10 golden snapshots into a pytest temp dir (the committed
evidence/p1/host-preview/ is regenerated only by tools/generate_preview.py —
tests never rewrite evidence, G-13) and asserts 800x480 geometry, DEMO
badges, and no secrets / TLS bypass anywhere in the repo (ISSUE-002).
"""

import os
import re

from kdash import ui_preview as U
from kdash.secret_scan import find_secrets

# TLS-bypass / downgrade markers (code-only rule, see below).
POLICY_PATTERNS = [
    r"setInsecure\s*\(",
    r"http://apis\.data\.go\.kr",
    r"http://oapi\.koreaexim",
]


def _footer(src="src-x", obs="obs 14:00", rx="rx 14:10"):
    return "src %s obs %s rx %s" % (src, obs, rx)


def test_geometry_constants_800x480():
    assert (U.W, U.H) == (800, 480)
    assert U.HEADER == (20, 12, 780, 48)
    assert U.FOOTER == (20, 440, 780, 468)
    assert U.BODY_L[2] - U.BODY_L[0] == 372
    assert U.BODY_R[2] - U.BODY_R[0] == 372


def _life_lines(temp="21.5"):
    return [
        ("temp " + temp + " C", "pop 60pct"),
        ("pm10 45", "pm25 18"),
        ("indoor 22.0C 48pct", "obs 14:00"),
        ("last-rx 14:10", "src KMA"),
    ]


def test_golden_snapshots_all_pages_and_states(tmp_path):
    EVID = str(tmp_path)
    pages = {
        "Day": (_life_lines(), _footer(), "Jongno 11-01 14:10 online", False, ()),
        "Night": (
            _life_lines("18.2"),
            _footer(),
            "Jongno 11-01 23:10 online",
            False,
            (),
        ),
        "Weekend": (
            [
                ("FX USD 1384.50", "JPY100 921.30"),
                ("close 005930 78500", "basedate 2026-10-30"),
                ("market closed", "src EXIM"),
            ],
            "src EXIM basedate 2026-10-30",
            "market weekend online",
            False,
            (),
        ),
        "NoNetwork": (
            [
                ("Wi-Fi disconnected", "check required"),
                ("last-good 2h ago", "time unverified"),
            ],
            "src cache last-rx 2h ago",
            "device offline",
            False,
            ("stale",),
        ),
        "NoData": (
            [("temp --", "pop --"), ("setup required", "key missing")],
            "no source setup required",
            "setup required",
            False,
            (),
        ),
        "LongKorean": (
            [
                ("temp 21.5C", "pop 60pct"),
                ("long-district-name-trunc", "pm25 18"),
                ("unit-basedate kept", "ellipsis + portal full"),
            ],
            "src KMA obs 14:00 rx 14:10",
            "long-name clipping",
            False,
            (),
        ),
        "WrongTime": (
            [("temp 21.5C", "time verify needed"), ("TLS on hold", "NTP/RTC resync")],
            "src cache time-unverified",
            "device time-check",
            False,
            ("verify",),
        ),
        "LowBattery": (
            [
                ("wifi online", "battery 15pct"),
                ("powersave cadence", "next wake 60min"),
            ],
            "src cache powersave",
            "device battery",
            False,
            (),
        ),
        "KisDisabled": (
            [
                ("close 005930 78500", "basedate 2026-10-30"),
                ("KIS off, no DEMO", "never shown as live"),
            ],
            "src FSC next-bizday 13:00+",
            "market KIS-off",
            False,
            (),
        ),
        "Demo": (_life_lines("22.0"), "sample values DEMO", "Jongno DEMO", True, ()),
    }
    assert set(pages) == set(U.SCENARIOS)
    for name, (lines, footer, header, demo, badges) in pages.items():
        img = U.frame(lines, footer, header, demo=demo, stale_badges=badges)
        assert img.size == (800, 480), name
        assert U.check_clip(img) == [], name
        img.save(os.path.join(EVID, name + ".png"))
        img.save(os.path.join(EVID, name + ".pgm"))  # 02: PNG/PGM 저장
    got = sorted(f for f in os.listdir(EVID) if f.endswith(".png"))
    assert len(got) == 10, got
    got_pgm = sorted(f for f in os.listdir(EVID) if f.endswith(".pgm"))
    assert len(got_pgm) == 10, got_pgm


def test_demo_and_stale_badges_visible():
    img = U.frame(_life_lines(), _footer(), demo=True, stale_badges=("stale",))
    px = img.load()
    dark = sum(1 for x in range(U.W) for y in range(U.H) if px[x, y] < 128)
    assert dark > 100  # text + DEMO badge + borders actually drawn


def test_required_korean_labels_in_layout_source():
    # UI-01/UI-02 label contract at source level (raster CJK needs the real
    # subset font on device/toolchain; U-10). Preview uses ASCII placeholders.
    src = os.path.join(os.path.dirname(__file__), "..", "..", "src", "ui", "Layout.h")
    with open(src, encoding="utf-8") as fh:
        text = fh.read()
    for label in (
        "기온",
        "강수확률",
        "초미세먼지",
        "미세먼지",
        "실내",
        "환율(고시)",
        "전일 종가",
        "현재가",
        "마지막 수신",
        "관측",
        "오래된 정보",
        "확인 필요",
        "Wi-Fi 미연결",
        "DEMO",
        "마지막 정상값",
        "설정 필요",
    ):
        assert label in text, label


ALLOW_MARKS = (
    "금지",
    "FORBIDDEN",
    "forbidden",
    "NEVER",
    "must not",
    "prohibit",
    "거부",
    "차단",
    "금지됨",
)
CODE_ONLY_DIRS = ("src", "include", "tools", "scripts")


SCAN_EXT = (
    ".cpp",
    ".h",
    ".c",
    ".py",
    ".json",
    ".md",
    ".yml",
    ".yaml",
    ".ps1",
    ".sh",
    ".ini",
    ".txt",
    ".log",
    ".csv",
    ".xml",
    ".html",
)
SKIP_DIRS = {
    ".git",
    "__pycache__",
    ".pytest_cache",
    "host-preview",
    "vendor-upstream",
    ".pio",
    ".codegraph",
    "build",
    "node_modules",
}
# The detector's own pattern table and its probe tests describe secrets by
# design; they are reviewed by hand instead.
SKIP_FILES = {
    os.path.join("host", "kdash", "secret_scan.py"),
    os.path.join("host", "tests", "test_secret_scan.py"),
}


def _repo_files():
    root = os.path.join(os.path.dirname(__file__), "..", "..")
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in SKIP_DIRS]
        for f in fn:
            if f.endswith(SCAN_EXT):
                p = os.path.join(dp, f)
                rel = os.path.relpath(p, root)
                if rel not in SKIP_FILES:
                    yield p, rel


def test_sec01_no_secrets_or_tls_bypass_in_repo():
    hits = []
    for p, rel in _repo_files():
        with open(p, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        # 1) Real secrets in ANY scanned file type, incl. logs (ISSUE-002).
        for no, kind in find_secrets(text):
            hits.append("%s:%d:%s" % (rel, no, kind))
        # 2) TLS bypass / HTTP downgrade in code (prohibition notices allowed).
        for i, line in enumerate(text.splitlines(), 1):
            for pat in POLICY_PATTERNS:
                if not re.search(pat, line):
                    continue
                if any(m in line for m in ALLOW_MARKS):
                    continue
                if pat.startswith("http://") and not rel.replace("\\", "/").startswith(
                    CODE_ONLY_DIRS
                ):
                    continue
                if "test_" in rel.replace("\\", "/") and "assert" in line:
                    continue
                hits.append("%s:%d:%s" % (rel, i, pat))
    assert hits == [], "SEC-01 violations: %r" % (hits,)
