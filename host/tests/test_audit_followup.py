import json
from pathlib import Path

import pytest
from kdash import airkorea, kma
from kdash import models as M  # pyright: ignore[reportImplicitRelativeImport] -- isort-canonical split duplicates pre-existing diagnostic on same module; tests run via rootdir sys.path

FIXTURES = Path(__file__).resolve().parents[2] / "test" / "fixtures"
NOW = M.kst_to_utc(2026, 11, 1, 14, 50)
CTX = M.FetchContext(NOW, NOW, True)


@pytest.mark.parametrize("field", ["baseDate", "baseTime", "nx", "ny"])
def test_kma_missing_source_metadata_is_not_success(field):
    doc = json.loads((FIXTURES / "kma_ultra_ok.json").read_text(encoding="utf-8"))
    del doc["response"]["body"]["items"]["item"][0][field]
    out = kma.parse_ultra_now(json.dumps(doc), 20261101, 1400, CTX)
    assert out["outcome"] is M.Outcome.ProviderError


@pytest.mark.parametrize("date", ["20261031", "20261102", "invalid"])
def test_kma_mixed_source_issues_are_rejected(date):
    doc = json.loads((FIXTURES / "kma_ultra_ok.json").read_text(encoding="utf-8"))
    doc["response"]["body"]["items"]["item"][0]["baseDate"] = date
    assert (
        kma.parse_ultra_now(json.dumps(doc), 20261101, 1400, CTX)["outcome"]
        is M.Outcome.ProviderError
    )


def test_air_newest_valid_sample_is_independent_of_row_order():
    rows = [
        {"dataTime": "2026-11-01 13:00", "pm10Value": "80", "pm25Value": "15"},
        {"dataTime": "2026-11-01 14:00", "pm10Value": "-", "pm25Value": "20"},
    ]
    for sequence in (rows, list(reversed(rows))):
        body = json.dumps(
            {"response": {"header": {"resultCode": "00"}, "body": {"items": sequence}}}
        )
        out = airkorea.parse(body, "station", CTX)
        assert out["pm10"].value == 80
        assert out["pm25"].value == 20
        assert out["data_time_utc"] == M.kst_to_utc(2026, 11, 1, 14, 0)
        assert out["pm10"].observed_at.epoch_utc == M.kst_to_utc(2026, 11, 1, 13, 0)


@pytest.mark.parametrize("nx,ny", [(61, 127), (60, 128)])
def test_kma_expected_grid_must_match(nx, ny):
    body = (FIXTURES / "kma_ultra_ok.json").read_text(encoding="utf-8")
    assert (
        kma.parse_ultra_now(body, 20261101, 1400, CTX, nx, ny)["outcome"]
        is M.Outcome.ProviderError
    )
