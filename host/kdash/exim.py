"""EXIM mirror (src/providers/EximParser.* + ProviderJson adapter)."""

from __future__ import annotations

import datetime as _dt
import json

from . import codes as C
from . import models as M
from .models import DataPoint, FetchContext, FxRate, Outcome, QuoteKind, Validity

SOURCE = "한국수출입은행"
FIXING_TTL_SEC = 4 * 24 * 3600


def parse_quoted(text):
    """'1,384.50' -> (won_scaled, scale=2). None on missing/unparseable/0."""
    if text is None:
        return None
    s = "".join(ch for ch in str(text) if ch not in (",", " ", "\t", "\r", "\n"))
    if not s or len(s) > 23 or s.startswith("-"):
        return None
    if s.count(".") > 1 or any(c not in "0123456789." for c in s):
        return None
    ip, _, fp = s.partition(".")
    if len(ip) > 15:
        return None
    won = int(ip or "0") * 100 + int((fp[:2] + "00")[:2])  # truncate, no float
    return (won, 2) if won > 0 else None


def parse_result(v) -> int:
    t = C.text(v)
    if t is None:
        return 0
    t = t.strip()
    return int(t) if t.isdigit() and len(t) < 4 else 0


def _point(unit, ws, sc, fixing_utc, validity, ctx):
    return DataPoint(
        value=FxRate(unit, ws, sc, QuoteKind.OfficialDailyFx),
        validity=validity,
        source=SOURCE,
        observed_at=M.source_ts(fixing_utc),
        fetched_at=M.device_ts(ctx),
    )


def parse(body: str, searchdate: int, ctx: FetchContext) -> dict:
    fixing = M.kst_ymd_hm_to_utc(searchdate, 0)
    missing = lambda v=Validity.Missing: DataPoint(  # noqa: E731
        validity=v, source=SOURCE, fetched_at=M.device_ts(ctx)
    )
    out = {
        "usd": missing(),
        "jpy100": missing(),
        "fixing_date_utc": fixing,
        "empty_response": False,
        "error_result": 0,
    }
    if not body or C.looks_like_xml(body):
        entries = [{"result": -1}]
    else:
        try:
            doc = json.loads(body)
        except ValueError:
            doc = [{"result": -1}]
        if doc is None:  # "null" body == no fixing
            doc = []
        entries = doc if isinstance(doc, list) else [{"result": -1}]
    rows = []
    for e in entries:
        if not isinstance(e, dict):
            continue
        get = lambda a, b, e=e: e.get(a, e.get(b))  # noqa: E731
        r = get("result", "RESULT")
        rows.append(
            (
                r
                if isinstance(r, int) and not isinstance(r, bool) and r == -1
                else parse_result(r),
                get("cur_unit", "CUR_UNIT"),
                C.text(get("deal_bas_r", "DEAL_BAS_R")),
            )
        )
    if not rows:
        out.update(empty_response=True, outcome=Outcome.NoData)
        return out
    for res, _, _ in rows:
        if res != 1:
            out["error_result"] = res if res else -1
            break
    if out["error_result"]:
        o = C.exim(out["error_result"])
        out["outcome"] = Outcome.ProviderError if o is Outcome.Ok else o
        out["usd"] = missing(Validity.ProviderError)
        out["jpy100"] = missing(Validity.ProviderError)
        return out
    fresh = M.classify_freshness(
        fixing, ctx.now_utc, FIXING_TTL_SEC, ctx.clock_verified
    )
    for _, unit, raw in rows:
        q = parse_quoted(raw)
        if q is None:
            continue
        if unit == "USD" and out["usd"].value is None:
            out["usd"] = _point("USD", *q, fixing, fresh, ctx)
        elif unit == "JPY(100)" and out["jpy100"].value is None:
            out["jpy100"] = _point("JPY(100)", *q, fixing, fresh, ctx)
    if out["usd"].value is None and out["jpy100"].value is None:
        out.update(empty_response=True, outcome=Outcome.NoData)
        return out
    out["outcome"] = Outcome.Ok
    return out


def plan_search_dates(start_yyyymmdd: int, max_days: int = 7) -> list[int]:
    y, m, d = (
        start_yyyymmdd // 10000,
        (start_yyyymmdd // 100) % 100,
        start_yyyymmdd % 100,
    )
    base = _dt.date(y, m, d)
    return [
        int((base - _dt.timedelta(days=i)).strftime("%Y%m%d")) for i in range(max_days)
    ]


def fixing_midnight_utc(yyyymmdd: int) -> int:
    return M.kst_ymd_hm_to_utc(yyyymmdd, 0)
