"""Finance mirror (src/providers/FinanceParsers.* + ProviderJson). P5, read-only."""

from __future__ import annotations

import json

from . import codes as C
from . import models as M
from .models import (
    DataPoint,
    FetchContext,
    Outcome,
    QuoteKind,
    StockQuote,
    Timestamp,
    Validity,
)

FSC_SOURCE = "금융위원회"
KIS_SOURCE = "한국투자증권"
FSC_TTL_SEC = 5 * 24 * 3600
KIS_TOKEN_MARGIN_SEC = 10 * 60


def _parse_krw(text):
    if text is None:
        return None
    s = "".join(ch for ch in str(text) if ch not in (",", " ", "\t"))
    if not s or len(s) > 19 or not s.isdigit() or not s.isascii():
        return None
    n = int(s)
    return n if n > 0 else None  # 0원은 실값이 아님


def parse_fsc(body: str, symbol: str, market: str, ctx: FetchContext) -> dict:
    code, is_xml, root = C.load_data_go_kr(body)
    o = C.data_go_kr(code)
    if is_xml and o is Outcome.Ok:
        o = Outcome.ProviderError
    q = DataPoint(
        validity=Validity.Missing
        if o in (Outcome.Ok, Outcome.NoData)
        else Validity.ProviderError,
        source=FSC_SOURCE,
        fetched_at=M.device_ts(ctx),
    )
    if o is not Outcome.Ok:
        return {"quote": q, "outcome": o}
    body_ = (root or {}).get("body") or {}
    items = body_.get("items") if isinstance(body_, dict) else None
    rows = C.items_of(items.get("item") if isinstance(items, dict) else None)
    best_day, best_price = -1, None
    for r in rows:
        code_ = C.text(r.get("srtnCd"))
        if code_ and symbol and code_ != symbol:
            continue
        price = _parse_krw(C.text(r.get("clpr")))
        day = M.kst_str_to_utc(C.text(r.get("basDt")), "0000")
        if price is None or day < 0 or day <= best_day:
            continue
        best_day, best_price = day, price
    if best_day < 0:
        return {"quote": q, "outcome": Outcome.NoData}
    ts = Timestamp(best_day, True)
    return {
        "quote": DataPoint(
            value=StockQuote(symbol, market, best_price, QuoteKind.PriorClose, ts),
            validity=M.classify_freshness(
                best_day, ctx.now_utc, FSC_TTL_SEC, ctx.clock_verified
            ),
            source=FSC_SOURCE,
            observed_at=ts,
            fetched_at=M.device_ts(ctx),
        ),
        "outcome": Outcome.Ok,
    }


def _json(body):
    try:
        doc = json.loads(body) if body else None
    except ValueError:
        return None
    return doc if isinstance(doc, dict) else None


def parse_kis(
    http_status: int, body: str, symbol: str, market: str, ctx: FetchContext
) -> dict:
    doc = _json(body) or {}
    msg_cd = doc.get("msg_cd") or doc.get("error_code")
    o = C.kis(http_status, doc.get("rt_cd"), msg_cd)
    out = {"token_invalid": C.kis_token_invalid(http_status, msg_cd), "outcome": o}
    if o is not Outcome.Ok:
        out["quote"] = DataPoint(
            validity=Validity.ProviderError,
            source=KIS_SOURCE,
            fetched_at=M.device_ts(ctx),
        )
        return out
    price = _parse_krw(C.text((doc.get("output") or {}).get("stck_prpr")))
    if price is None:
        out.update(
            quote=DataPoint(
                validity=Validity.Missing,
                source=KIS_SOURCE,
                fetched_at=M.device_ts(ctx),
            ),
            outcome=Outcome.NoData,
        )
        return out
    ts = M.device_ts(ctx)  # REST 호출 시점 현재가 (체결 스트림 아님)
    out["quote"] = DataPoint(
        value=StockQuote(symbol, market, price, QuoteKind.IntradayQuote, ts),
        validity=Validity.Valid if ctx.clock_verified else Validity.Stale,
        source=KIS_SOURCE,
        observed_at=ts,
        fetched_at=ts,
    )
    return out


def kis_token(http_status: int, body: str, ctx: FetchContext):
    """-> (expiry_utc | -1, Outcome). Token text is never returned/logged."""
    doc = _json(body)
    if doc is None:
        o = C.kis(http_status, None, None)
        return -1, (Outcome.ProviderError if o is Outcome.Ok else o)
    err = doc.get("error_code") or doc.get("msg_cd")
    tok = doc.get("access_token")
    exp = doc.get("expires_in") or 0
    if (
        http_status != 200
        or not isinstance(tok, str)
        or len(tok) < 16
        or not isinstance(exp, int)
        or exp <= KIS_TOKEN_MARGIN_SEC
    ):
        o = C.kis(http_status, None, err)
        if o is Outcome.Ok:
            o = Outcome.ProviderError
        if http_status == 403 and o is not Outcome.RateLimited:
            o = Outcome.AuthError
        return -1, o
    return ctx.fetched_utc + exp - KIS_TOKEN_MARGIN_SEC, Outcome.Ok


def is_krx_open(epoch_utc: int) -> bool:
    b = M.break_kst(epoch_utc)
    if b["weekday"] in (0, 6):
        return False
    mins = b["hour"] * 60 + b["minute"]
    return 9 * 60 <= mins < 15 * 60 + 30


def fsc_fetch_due(now_utc: int, last_success_utc: int) -> bool:
    """FSC 평일 13:00 KST 이후 1회. 성공 이력이 없으면 13시 전에도 1회(ADR 0017)."""
    b = M.break_kst(now_utc)
    if b["weekday"] in (0, 6):
        return False
    boundary = M.kst_to_utc(b["year"], b["month"], b["day"], 13, 0)
    if now_utc < boundary:
        # 성공 이력이 없을 때만 13시 전에도 1회 허용(14일 창의 최신 종가=직전 영업일).
        return last_success_utc <= 0
    return last_success_utc < boundary


def is_valid_symbol(s) -> bool:
    return isinstance(s, str) and len(s) == 6 and s.isdigit() and s.isascii()


def is_valid_market(m) -> bool:
    return m in ("KRX", "NXT", "통합")
