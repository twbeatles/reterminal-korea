"""AirKorea mirror (src/providers/AirKoreaParser.* + ProviderJson adapter)."""

from __future__ import annotations

import re

from . import codes as C
from . import models as M
from .models import DataPoint, FetchContext, Outcome, Validity

SOURCE = "에어코리아"
TTL_SEC = 3 * 3600
_DT = re.compile(r"^\s*(\d{1,4})-(\d{1,2})-(\d{1,2})\s*(\d{1,2}):(\d{1,2})$")


def parse_int(v):
    if v is None:
        return None
    s = str(v).strip()
    if s.startswith("+"):
        s = s[1:]
    if not s.isdigit() or not s.isascii():
        return None  # "-", "", 점검/통신장애 문구, 음수 => Missing (AIR-01)
    n = int(s)
    return n if n <= 100000 else None


def parse_data_time_kst(dt) -> int:
    """'YYYY-MM-DD HH:MM' KST -> UTC; '24:00' == next day 00:00; -1 bad."""
    m = _DT.match(dt or "")
    if not m:
        return -1
    y, mo, d, h, mi = map(int, m.groups())
    if not (
        2000 <= y and M.is_real_date(y, mo, d) and 0 <= h <= 24 and 0 <= mi <= 59
    ) or (h == 24 and mi != 0):
        return -1
    return M.kst_to_utc(y, mo, d, h, mi)


def _station_matches(row: dict, station: str) -> bool:
    name = row.get("stationName")
    if not station or not name:
        return True  # per-station operation may omit stationName (U-06)
    return name == station


def parse(body: str, station: str, ctx: FetchContext) -> dict:
    code, is_xml, root = C.load_data_go_kr(body)
    o = C.data_go_kr(code)
    if is_xml and o is Outcome.Ok:
        o = Outcome.ProviderError
    out = {"station": station or "", "data_time_utc": -1, "outcome": o}
    if o is not Outcome.Ok:
        v = Validity.Missing if o is Outcome.NoData else Validity.ProviderError
        out["pm10"] = DataPoint(validity=v, source=SOURCE, fetched_at=M.device_ts(ctx))
        out["pm25"] = DataPoint(validity=v, source=SOURCE, fetched_at=M.device_ts(ctx))
        return out
    for key in ("pm10", "pm25"):
        out[key] = DataPoint(
            validity=Validity.Missing, source=SOURCE, fetched_at=M.device_ts(ctx)
        )
    body_ = (root or {}).get("body") or {}
    items = body_.get("items") if isinstance(body_, dict) else None
    if isinstance(items, dict) and items.get("item") is not None:
        items = items["item"]
    rows = [r for r in C.items_of(items) if _station_matches(r, station)]
    if not rows:
        out["outcome"] = Outcome.NoData  # AIR-02: configured station absent
        return out
    out["data_time_utc"] = max(parse_data_time_kst(r.get("dataTime")) for r in rows)
    for key, field in (("pm10", "pm10Value"), ("pm25", "pm25Value")):
        for r in sorted(
            rows, key=lambda r: parse_data_time_kst(r.get("dataTime")), reverse=True
        ):
            v = parse_int(C.text(r.get(field)))
            if v is None:
                continue
            t = parse_data_time_kst(r.get("dataTime"))
            out[key] = DataPoint(
                value=v,
                validity=M.classify_freshness(
                    t, ctx.now_utc, TTL_SEC, ctx.clock_verified
                ),
                source=SOURCE,
                observed_at=M.source_ts(t if t > 0 else 0),
                fetched_at=M.device_ts(ctx),
            )
            break
    return out
