"""Full-year KASI public holidays, mirrored by providers/HolidayService.cpp.

Primary contract: data.go.kr 15012690 technical attachment v1.4, checked
2026-11-02. Exchange-specific closures are deliberately separate.
"""

import datetime as dt
import json

from . import codes, endpoints
from .models import Outcome


def year_url(key, year):
    if not isinstance(year, int) or not 2000 <= year <= 2099:
        raise ValueError("unsupported year")
    return (
        "https://apis.data.go.kr/B090041/openapi/service/SpcdeInfoService/getRestDeInfo"
        "?ServiceKey=%s&solYear=%04d&pageNo=1&numOfRows=128&_type=json"
        % (endpoints.normalize_service_key(key), year)
    )


def _integer(value):
    if type(value) is int:
        return value
    if (
        isinstance(value, str)
        and value.isascii()
        and value.isdigit()
        and 1 <= len(value) <= 8
    ):
        return int(value)
    raise ValueError("invalid integer")


def parse(body, year, ctx):
    error = {"outcome": Outcome.ProviderError, "loaded": False, "holidays": []}
    if (
        not 2000 <= year <= 2099
        or not ctx.clock_verified
        or len(body.encode("utf-8")) > 65536
    ):
        return error
    if codes.looks_like_xml(body):
        result, _, _ = codes.load_data_go_kr(body)
        found = codes.data_go_kr(result)
        return dict(
            error, outcome=Outcome.ProviderError if found == Outcome.Ok else found
        )
    try:
        root = json.loads(body)["response"]
        result = codes.data_go_kr(root["header"].get("resultCode"))
        if result != Outcome.Ok:
            return dict(error, outcome=result)
        payload = root["body"]
        total = _integer(payload["totalCount"])
        page, rows = _integer(payload["pageNo"]), _integer(payload["numOfRows"])
        if not 0 <= total <= 128 or page != 1 or rows < total:
            return error
        if total == 0:
            return dict(error, outcome=Outcome.NoData)
        items = payload["items"]["item"]
        items = [items] if isinstance(items, dict) else items
        if not isinstance(items, list) or len(items) != total:
            return error
        dates = set()
        for item in items:
            value = _integer(item["locdate"])
            date = dt.date(value // 10000, value // 100 % 100, value % 100)
            if date.year != year or item["isHoliday"] not in ("Y", "N"):
                return error
            if item["isHoliday"] == "Y":
                dates.add(value)
        if not dates:
            return dict(error, outcome=Outcome.NoData)
        return {
            "outcome": Outcome.Ok,
            "loaded": True,
            "holidays": sorted(dates),
            "fetched_utc": ctx.fetched_utc,
            "records": total,
        }
    except (ValueError, KeyError, TypeError, AttributeError, OverflowError):
        return error
