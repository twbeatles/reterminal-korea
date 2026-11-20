"""Stage-2 pytest mirrors (T02-T12): must agree with the C++ native tests."""

import subprocess
import sys
from pathlib import Path

from kdash.extended import (
    ADC_INTERVAL_MS,
    ADC_SAMPLES,
    ADC_SETTLE_MS,
    CURVE_MV,
    ButtonDecoder,
    YearTable,
    battery_class,
    battery_percent,
    cap_for,
    classify_day,
    clock_decide,
    latlon_to_grid,
    mean_of,
    pcf_year,
    pmic_bus_present,
    pmic_usb_attached,
    should_defer,
    sht_crc8,
    sht_decode,
)

ROOT = Path(__file__).resolve().parent.parent.parent


def test_clock_policy():
    e, ok, src, act = clock_decide(True, False, 1790000000, True, 1790000003)
    assert (ok, src, act) == (True, "NTP", "WRITE_RTC")
    e, ok, src, act = clock_decide(True, False, 1790000000, False, 0)
    assert (ok, src) == (True, "RTC")
    assert clock_decide(True, True, 1790000000, False, 0)[1] is False
    assert clock_decide(True, False, 1000000000, False, 0)[1] is False


def test_calendar():
    t = YearTable(2026)
    t.loaded = True
    t.holidays.update([(2, 17), (2, 18), (2, 19)])
    t.closed.add((12, 31))
    assert classify_day(2026, 2, 17, 2, [t]) == "PublicHoliday"
    assert classify_day(2026, 12, 31, 4, [t]) == "MarketClosed"
    assert classify_day(2027, 11, 1, 1, [t]) == "Unknown"
    assert classify_day(2026, 11, 4, 3, [t]) == "BusinessDay"


def test_geogrid():
    assert latlon_to_grid(37.5665, 126.9780) == (60, 127)
    assert latlon_to_grid(35.1796, 129.0756) == (98, 76)
    assert latlon_to_grid(0.0, 0.0) is None


def test_buttons():
    d = ButtonDecoder()
    assert d.feed(0, 1) == "None"
    assert d.feed(30, 0) == "None"
    d.reset()
    assert d.feed(0, 1) == "None"
    assert d.feed(120, 1) == "None"
    assert d.feed(200, 0) == "ShortKey0"
    d.reset()
    d.feed(0, 1)
    got = "None"
    t = 100
    while t <= 2100:
        e = d.feed(t, 1)
        if e != "None":
            got = e
        t += 100
    assert got == "LongKey0"
    assert d.feed(2300, 0) == "None"


def test_sht4x_battery():
    raw = bytes([0x80, 0x00, 0, 0x80, 0x00, 0])
    frame = bytes(
        [raw[0], raw[1], sht_crc8(raw[0:2]), raw[3], raw[4], sht_crc8(raw[3:5])]
    )
    t, rh = sht_decode(frame)
    assert 40.0 < t < 45.0
    assert 0 <= rh <= 100
    bad = bytearray(frame)
    bad[2] ^= 1
    assert sht_decode(bytes(bad)) is None
    assert battery_class(3800) == "Normal"
    assert battery_class(3400) == "Low"
    assert battery_class(3200) == "Critical"
    assert battery_class(-1) == "Unknown"


def test_budget_caps():
    assert cap_for("Exim") == 7
    assert cap_for("FscClose") == 4
    assert cap_for("KisPrice") == 6
    assert should_defer("Exim", 7, 7) is True
    assert should_defer("KmaNow", 0, 20) is False


def test_official_battery_curve_pmic_century():
    assert battery_percent(2700) == 0
    assert battery_percent(2795) == 0
    assert battery_percent(3642) == 50
    assert battery_percent(4111) == 100
    assert battery_percent(4200) == 100
    assert battery_percent(3464) == 20
    assert battery_percent(3252) == 5
    assert battery_percent(2796) == 1
    assert battery_percent(2990) == 2
    assert battery_percent(3378) == 10
    assert battery_percent(3927) == 90
    assert len(CURVE_MV) == 101
    prev = -1
    for mv in range(2500, 4201):
        p = battery_percent(mv)
        assert 0 <= p <= 100 and p >= prev
        prev = p
    assert (ADC_SAMPLES, ADC_INTERVAL_MS, ADC_SETTLE_MS) == (10, 25, 5)
    assert mean_of([3700, 3710, 3690, 3705, 3702, 3698, 3701, 3699, 3703, 3900]) == 3720
    assert mean_of([]) == -1
    assert pmic_usb_attached(0x80) is True
    assert pmic_usb_attached(0x00) is False
    assert pmic_bus_present(0x00) is False
    assert pmic_bus_present(0x20) is True
    assert pcf_year(0x11, 0x26) == 2026
    assert pcf_year(0x91, 0x26) == 1926
    assert pcf_year(0x92, 0x99) == 1999


def test_release_packaging_script():
    r = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "package_release.py"), "--help"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0
    assert "version" in r.stdout.lower()
    # Invalid versions fail before a compiler or real artifact write, whether
    # this checkout is clean or dirty. Dirty-git cases use isolated fixtures in
    # test_audit_release_probe.py.
    r = subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "package_release.py"),
            "--version",
            "0.0.0-test",
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode != 0
    assert "version" in (r.stdout + r.stderr).lower()
