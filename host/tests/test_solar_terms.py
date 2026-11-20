"""24절기 상수표(src/domain/SolarTerms.cpp)는 공식 API 응답 픽스처에서 생성된 것과 같아야 한다."""

import datetime
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "test" / "fixtures" / "solar_terms_2026_2028.json"


def test_generated_table_is_up_to_date():
    r = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "gen_solar_terms.py"), "--check"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr


def test_fixture_is_complete_ordered_and_secret_free():
    raw = FIXTURE.read_text(encoding="utf-8")
    assert "serviceKey" not in raw and "http" not in raw
    data = json.loads(raw)
    assert sorted(data) == ["2026", "2027", "2028"]
    for year, items in data.items():
        assert len(items) == 24
        dates = [datetime.datetime.strptime(str(d), "%Y%m%d").date() for _, d in items]
        assert dates == sorted(dates) and all(str(x.year) == year for x in dates)
        # 절기는 약 15일 간격(13~17일)
        gaps = [(b - a).days for a, b in zip(dates, dates[1:])]
        assert all(13 <= g <= 17 for g in gaps), gaps
