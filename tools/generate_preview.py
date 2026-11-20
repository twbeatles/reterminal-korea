"""Regenerate the 10 golden 800x480 previews (05 Host Preview).

Usage: python tools/generate_preview.py
Output: evidence/p1/host-preview/{Day,...,Demo}.{png,pbm}
Uses ASCII placeholder strings (CJK raster needs the real subset font, U-10);
geometry, badges, and clipping checks are what this verifies.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "host"))
from kdash import ui_preview as U  # noqa: E402

SCENARIO_LINES = {
    "Day": (
        [
            ("temp 21.5 C", "pop 60pct"),
            ("pm10 45", "pm25 18"),
            ("indoor 22.0C 48pct", "obs 14:00"),
            ("last-rx 14:10", "src KMA"),
        ],
        "src KMA obs 14:00 rx 14:10",
        "Jongno 11-01 14:10 online",
        False,
        (),
    ),
    "Night": (
        [
            ("temp 18.2 C", "pop 20pct"),
            ("pm10 30", "pm25 12"),
            ("indoor 21.0C 45pct", "obs 23:00"),
            ("last-rx 23:10", "src KMA"),
        ],
        "src KMA obs 23:00 rx 23:10",
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
        [("wifi online", "battery 15pct"), ("powersave cadence", "next wake 60min")],
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
    "Demo": (
        [
            ("temp 22.0 C", "pop 60pct"),
            ("pm10 45", "pm25 18"),
            ("indoor 22.0C 48pct", "obs 14:00"),
            ("last-rx 14:10", "src KMA"),
        ],
        "sample values DEMO",
        "Jongno DEMO",
        True,
        (),
    ),
}


def main() -> int:
    outdir = os.path.join(
        os.path.dirname(__file__), "..", "evidence", "p1", "host-preview"
    )
    os.makedirs(outdir, exist_ok=True)
    for name, (lines, footer, header, demo, badges) in SCENARIO_LINES.items():
        img = U.frame(lines, footer, header, demo=demo, stale_badges=badges)
        assert img.size == (800, 480), name
        assert U.check_clip(img) == [], name
        img.save(os.path.join(outdir, name + ".png"))
        img.save(os.path.join(outdir, name + ".pgm"))  # 02: PNG/PGM 저장
        print("wrote %s.{png,pgm}" % name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
