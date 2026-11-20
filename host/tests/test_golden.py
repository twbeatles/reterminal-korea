"""Shared golden provider cases (test/golden/provider_cases.json).

The SAME file is executed by test_native/test_core.cpp against the C++
ProviderJson adapters, so the Python mirror and the device code cannot
silently diverge again (ISSUE-003).
"""

import json

import pytest
from conftest import GOLDEN, raw
from kdash import airkorea, exim, finance, kma
from kdash import models as M
from kdash.airkorea import parse_data_time_kst as kst

with open(GOLDEN, encoding="utf-8") as _f:
    CASES = json.load(_f)["cases"]


def _ctx(case):
    now = kst(case["now"])
    return M.FetchContext(
        fetched_utc=now, now_utc=now, clock_verified=bool(case.get("verified", True))
    )


def _point_value(dp):
    v = dp.value
    if isinstance(v, M.FxRate):
        return v.won_scaled
    if isinstance(v, M.StockQuote):
        return v.price_krw
    return v


def _run(case):
    ctx = _ctx(case)
    body = raw(case["fixture"])
    prov = case["provider"]
    if prov == "kma_now":
        return kma.parse_ultra_now(body, *case["base"], ctx)
    if prov == "kma_village":
        return kma.parse_village(body, *case["base"], ctx)
    if prov == "air":
        return airkorea.parse(body, case["station"], ctx)
    if prov == "exim":
        return exim.parse(body, case["searchdate"], ctx)
    if prov == "fsc":
        return finance.parse_fsc(body, case["symbol"], case["market"], ctx)
    if prov == "kis":
        return finance.parse_kis(
            case["http_status"], body, case["symbol"], case["market"], ctx
        )
    if prov == "kis_token":
        exp, o = finance.kis_token(case["http_status"], body, ctx)
        return {"outcome": o, "expiry": exp}
    raise AssertionError("unknown provider " + prov)


def test_golden_file_is_large_enough():
    assert len(CASES) >= 40


@pytest.mark.parametrize("case", CASES, ids=[c["id"] for c in CASES])
def test_golden_case(case):
    out = _run(case)
    ex = case["expect"]
    assert out["outcome"].value == ex["outcome"]
    for name, (validity, value) in ex.get("points", {}).items():
        dp = out[name]
        assert M.VALIDITY_NAME[dp.validity] == validity, name
        if value is None:
            assert dp.value is None, name + ": never 0 for missing"
        else:
            assert _point_value(dp) == pytest.approx(value), name
    if "base" in ex:
        assert out["base_utc"] == kst(ex["base"])
    if "fcst" in ex:
        assert out["fcst_utc"] == kst(ex["fcst"])
    if "data_time" in ex:
        assert out["data_time_utc"] == kst(ex["data_time"])
    if "pm25_observed" in ex:
        assert out["pm25"].observed_at.epoch_utc == kst(ex["pm25_observed"])
    if "empty_response" in ex:
        assert out["empty_response"] is ex["empty_response"]
    if "fixing" in ex:
        assert out["fixing_date_utc"] == kst(ex["fixing"])
    if "observed" in ex:
        assert out["quote"].observed_at.epoch_utc == kst(ex["observed"])
    if "token_invalid" in ex:
        assert out["token_invalid"] is ex["token_invalid"]
    if "fetched_verified" in ex:
        first = next(iter(ex["points"]))
        assert out[first].fetched_at.verified is ex["fetched_verified"]
    if "expires_in_after_margin" in ex:
        assert out["expiry"] - kst(case["now"]) == ex["expires_in_after_margin"]
    if case["provider"] == "air":
        assert out["station"] == case["station"]
    if case["provider"] == "exim":
        for key, unit in (("usd", "USD"), ("jpy100", "JPY(100)")):
            if out[key].value is not None:
                assert out[key].value.currency_unit == unit
                assert out[key].value.quote_kind is M.QuoteKind.OfficialDailyFx
