"""E1001 shared SPI bus guard (ADR 0013): source audit, no device IO.

The microSD slot shares SCK/MOSI (GPIO7/9) with the UC8179 panel. On real
E1001 hardware (2026-11-11) only the first frame reached the panel until the
SD CS/EN pins were pinned before any panel traffic. Keep that order.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def _begin_body() -> str:
    src = (ROOT / "src/drivers/EpdPanel.cpp").read_text(encoding="utf-8")
    m = re.search(r"void EpdPanel::begin\(\) \{(.*?)\n\}", src, re.S)
    assert m, "EpdPanel::begin not found"
    return m.group(1)


def test_sd_pins_match_official_wiki_map():
    hdr = (ROOT / "src/drivers/EpdPanel.h").read_text(encoding="utf-8")
    assert re.search(r"kPinSdCs\s*=\s*14;", hdr)
    assert re.search(r"kPinSdDet\s*=\s*15;", hdr)
    assert re.search(r"kPinSdEn\s*=\s*16;", hdr)


def test_sd_slot_is_deselected_and_unpowered_before_panel_traffic():
    body = _begin_body()
    cs_high = body.find("digitalWrite(kPinSdCs, HIGH)")
    en_low = body.find("digitalWrite(kPinSdEn, LOW)")
    assert cs_high >= 0 and en_low >= 0
    first_panel = min(
        i for i in (body.find("ProbeOtp()"), body.find("spi.begin(")) if i >= 0
    )
    assert cs_high < first_panel and en_low < first_panel
