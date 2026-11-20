"""Synthetic KASI full-year fixtures; no live calendar facts are asserted."""

import json
from pathlib import Path

import pytest
from kdash import holidays, policy
from kdash.models import FetchContext, Outcome

CTX = FetchContext(1793600000, 1793600000, True)
ROOT = Path(__file__).resolve().parents[2]


def fixture():
    return json.loads(
        (ROOT / "test" / "fixtures" / "holiday_year_demo.json").read_text(
            encoding="utf-8"
        )
    )


def test_holiday_full_year_and_url():
    result = holidays.parse(json.dumps(fixture()), 2026, CTX)
    assert result["outcome"] == Outcome.Ok
    assert result["holidays"] == [20260217, 20260218]
    assert result["records"] == 2 and result["loaded"] is True
    url = holidays.year_url("FAKE+KEY=", 2026)
    assert policy.host_allowed(url)
    assert "ServiceKey=FAKE%2BKEY%3D&solYear=2026" in url


@pytest.mark.parametrize(
    "fault",
    [
        "partial",
        "wrong-year",
        "invalid-day",
        "oversize-total",
        "page",
        "marker",
        "untrusted-clock",
    ],
)
def test_holiday_never_publishes_incomplete_or_invalid_year(fault):
    value = fixture()
    body = value["response"]["body"]
    ctx = CTX
    if fault == "partial":
        body["items"]["item"].pop()
    elif fault == "wrong-year":
        body["items"]["item"][0]["locdate"] = 20270217
    elif fault == "invalid-day":
        body["items"]["item"][0]["locdate"] = 20260230
    elif fault == "oversize-total":
        body["totalCount"] = 200
    elif fault == "page":
        body["pageNo"] = 2
    elif fault == "marker":
        body["items"]["item"][0]["isHoliday"] = "?"
    else:
        ctx = FetchContext(CTX.now_utc, CTX.fetched_utc, False)
    out = holidays.parse(json.dumps(value), 2026, ctx)
    assert out["outcome"] == Outcome.ProviderError and not out["loaded"]


def test_holiday_empty_year_not_published_and_xml_auth():
    value = fixture()
    value["response"]["body"]["totalCount"] = 0
    value["response"]["body"]["items"] = ""
    assert holidays.parse(json.dumps(value), 2026, CTX)["outcome"] == Outcome.NoData
    xml = "<OpenAPI_ServiceResponse><cmmMsgHeader><returnReasonCode>30</returnReasonCode></cmmMsgHeader></OpenAPI_ServiceResponse>"
    assert holidays.parse(xml, 2026, CTX)["outcome"] == Outcome.AuthError


def test_holiday_single_object_and_leap_day():
    value = fixture()
    body = value["response"]["body"]
    body["totalCount"] = 1
    body["items"]["item"] = {
        "locdate": "20280229",
        "isHoliday": "Y",
        "dateName": "DEMO leap holiday",
    }
    result = holidays.parse(json.dumps(value), 2028, CTX)
    assert result["outcome"] == Outcome.Ok and result["holidays"] == [20280229]
