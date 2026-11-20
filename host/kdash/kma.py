"""KMA mirror (src/providers/KmaParser.* + ProviderJson KMA adapters).

Takes the raw response body, exactly like the device adapter. Semantics are
pinned by test/golden/provider_cases.json, shared with the C++ tests.
"""

from __future__ import annotations

import re

from . import codes as C
from . import models as M
from .models import DataPoint, FetchContext, Outcome, Validity

SOURCE = "기상청"
ULTRA_DELAY_MIN = 40  # 정시 회차, 출발값 (U-05)
VILLAGE_DELAY_MIN = 10  # 출발값 (U-05)
VILLAGE_SLOTS = (2, 5, 8, 11, 14, 17, 20, 23)
ULTRA_TTL_SEC = 2 * 3600
VILLAGE_TTL_SEC = 12 * 3600
VILLAGE_NUM_ROWS = 72
ULTRA_NUM_ROWS = 16

_MISSING = {"-", "", "null", "NULL", "강수없음"}
_NUM = re.compile(r"^[+-]?(\d+\.?\d*|\.\d+)$")


def is_missing_token(v) -> bool:
    return v is None or str(v).strip() in _MISSING


def parse_decimal(v):
    if is_missing_token(v):
        return None
    s = str(v).strip()
    if not _NUM.match(s):
        return None
    r = float(s)
    if r >= 900.0 or r <= -900.0:  # KMA missing-value sentinels
        return None
    return r


def _prev_day(y, mo, d):
    days = M.epoch_from_civil(y, mo, d) // 86400 - 1
    import datetime as dt

    x = dt.date(1970, 1, 1) + dt.timedelta(days=days)
    return x.year, x.month, x.day


def select_ultra_now_base(now_utc: int) -> tuple[int, int]:
    """Latest requestable 초단기실황 회차 (정시 HH00). Never future (KMA-03)."""
    b = M.break_kst(now_utc)
    hh, yy, mo, dd = b["hour"], b["year"], b["month"], b["day"]
    if b["minute"] < ULTRA_DELAY_MIN:
        hh -= 1
        if hh < 0:
            hh = 23
            yy, mo, dd = _prev_day(yy, mo, dd)
    return yy * 10000 + mo * 100 + dd, hh * 100


def select_village_base(now_utc: int) -> tuple[int, int]:
    b = M.break_kst(now_utc)
    slot = -1
    for s in VILLAGE_SLOTS:
        if b["hour"] > s or (b["hour"] == s and b["minute"] >= VILLAGE_DELAY_MIN):
            slot = s
    yy, mo, dd = b["year"], b["month"], b["day"]
    if slot < 0:  # before 02:10 -> yesterday 23:00 issue (midnight rule)
        slot = 23
        yy, mo, dd = _prev_day(yy, mo, dd)
    return yy * 10000 + mo * 100 + dd, slot * 100


def _empty(v: Validity, ctx: FetchContext) -> DataPoint:
    return DataPoint(validity=v, source=SOURCE, fetched_at=M.device_ts(ctx))


def _resolve(body):
    code, is_xml, root = C.load_data_go_kr(body)
    o = C.data_go_kr(code)
    if is_xml and o is Outcome.Ok:
        o = Outcome.ProviderError
    return o, root


def _failure(o: Outcome) -> Validity:
    return Validity.Missing if o is Outcome.NoData else Validity.ProviderError


def _items(root) -> list:
    body = (root or {}).get("body") or {}
    items = body.get("items") if isinstance(body, dict) else None
    return C.items_of(items.get("item") if isinstance(items, dict) else None)


def _source_matches(items, date, time, nx, ny):
    requested = M.kst_ymd_hm_to_utc(date, time)
    grid = None
    for item in items:
        if (
            M.kst_str_to_utc(C.text(item.get("baseDate")), C.text(item.get("baseTime")))
            != requested
            or requested <= 0
        ):
            return False
        xy = tuple(C.text(item.get(k)) for k in ("nx", "ny"))
        if any(
            not s or not s.isascii() or not s.isdigit() or not 1 <= int(s) <= 200
            for s in xy
        ):
            return False
        xy = tuple(map(int, xy))
        if (
            (grid is not None and xy != grid)
            or (nx >= 0 and xy[0] != nx)
            or (ny >= 0 and xy[1] != ny)
        ):
            return False
        grid = xy
    return len(items) <= 96


def parse_ultra_now(
    body: str,
    base_ymd: int,
    base_hm: int,
    ctx: FetchContext,
    expected_nx=-1,
    expected_ny=-1,
) -> dict:
    o, root = _resolve(body)
    if o is Outcome.Ok and not _source_matches(
        _items(root), base_ymd, base_hm, expected_nx, expected_ny
    ):
        o = Outcome.ProviderError
    base_utc = M.kst_ymd_hm_to_utc(base_ymd, base_hm)
    keys = (("temp_c", "T1H"), ("humidity", "REH"), ("rain_mm", "RN1"))
    if o is not Outcome.Ok:
        out = {k: _empty(_failure(o), ctx) for k, _ in keys}
        out.update(base_utc=base_utc, outcome=o)
        return out
    out = {k: _empty(Validity.Missing, ctx) for k, _ in keys}
    items = _items(root)
    if not items:
        out.update(base_utc=base_utc, outcome=Outcome.NoData)
        return out
    fresh = M.classify_freshness(
        base_utc, ctx.now_utc, ULTRA_TTL_SEC, ctx.clock_verified
    )
    for key, cat in keys:
        for it in items:  # first parseable occurrence wins ("-" stays Missing)
            if it.get("category") != cat:
                continue
            raw = it.get("obsrValue", it.get("fcstValue"))
            v = parse_decimal(C.text(raw))
            if v is not None:
                out[key] = DataPoint(
                    value=v,
                    validity=fresh,
                    source=SOURCE,
                    observed_at=M.source_ts(base_utc),
                    fetched_at=M.device_ts(ctx),
                )
                break
    out.update(base_utc=base_utc, outcome=Outcome.Ok)
    return out


def parse_village(
    body: str,
    base_ymd: int,
    base_hm: int,
    ctx: FetchContext,
    expected_nx=-1,
    expected_ny=-1,
) -> dict:
    o, root = _resolve(body)
    if o is Outcome.Ok and not _source_matches(
        _items(root), base_ymd, base_hm, expected_nx, expected_ny
    ):
        o = Outcome.ProviderError
    base_utc = M.kst_ymd_hm_to_utc(base_ymd, base_hm)
    keys = (("tmp", "TMP"), ("pop", "POP"), ("pty", "PTY"), ("sky", "SKY"))
    if o is not Outcome.Ok:
        out = {k: _empty(_failure(o), ctx) for k, _ in keys}
        out.update(base_utc=base_utc, fcst_utc=0, outcome=o)
        return out
    out = {k: _empty(Validity.Missing, ctx) for k, _ in keys}
    items = _items(root)
    hour_floor = ctx.now_utc - ctx.now_utc % 3600

    def slot(it):
        return M.kst_str_to_utc(C.text(it.get("fcstDate")), C.text(it.get("fcstTime")))

    future = [t for t in (slot(it) for it in items) if t >= 0 and t >= hour_floor]
    if not future:
        out.update(base_utc=base_utc, fcst_utc=0, outcome=Outcome.NoData)
        return out
    target = min(future)  # earliest slot >= current hour (ISSUE-001)
    fresh = M.classify_freshness(
        base_utc, ctx.now_utc, VILLAGE_TTL_SEC, ctx.clock_verified
    )
    for it in items:
        if slot(it) != target:
            continue
        v = parse_decimal(C.text(it.get("fcstValue")))
        if v is None:
            continue
        for key, cat in keys:
            if it.get("category") == cat and out[key].value is None:
                out[key] = DataPoint(
                    value=(v if key == "tmp" else int(v)),
                    validity=fresh,
                    source=SOURCE,
                    observed_at=M.source_ts(base_utc),
                    fetched_at=M.device_ts(ctx),
                )
    out.update(base_utc=base_utc, fcst_utc=target, outcome=Outcome.Ok)
    return out
