"""Domain core mirror (src/domain/Types.h + Freshness.*).

Validity / QuoteKind / Timestamp / DataPoint / FxRate / StockQuote.
Internal time is Unix UTC; KST strings are UI-only.
"""

from __future__ import annotations

import datetime as _dt
from dataclasses import dataclass, field
from enum import Enum


class Validity(str, Enum):
    Valid = "Valid"
    Missing = "Missing"
    ProviderError = "ProviderError"
    Stale = "Stale"
    NotConfigured = "NotConfigured"


class QuoteKind(str, Enum):
    Unknown = "Unknown"
    OfficialDailyFx = "OfficialDailyFx"  # EXIM 고시환율 (daily fixing, NOT live)
    PriorClose = "PriorClose"  # FSC 전일 종가 (as of 거래일)
    IntradayQuote = "IntradayQuote"  # KIS REST snapshot at call time


KST = _dt.timezone(_dt.timedelta(hours=9))
KST_OFFSET_SEC = 9 * 3600


@dataclass
class Timestamp:
    epoch_utc: int = 0
    verified: bool = False


@dataclass
class DataPoint:
    value: object = None
    validity: Validity = Validity.NotConfigured
    observed_at: Timestamp = field(default_factory=Timestamp)
    fetched_at: Timestamp = field(default_factory=Timestamp)
    source: str = ""


@dataclass
class FxRate:
    currency_unit: str = ""  # "USD" or "JPY(100)" — unit is part of the value
    won_scaled: int = 0  # real_rate = won_scaled / scale
    scale: int = 2
    quote_kind: QuoteKind = QuoteKind.OfficialDailyFx

    @property
    def rate(self) -> float:
        return self.won_scaled / (10**self.scale)


@dataclass
class StockQuote:
    symbol: str = ""
    market: str = ""
    price_krw: int = 0
    kind: QuoteKind = QuoteKind.PriorClose
    price_at: Timestamp = field(default_factory=Timestamp)


# --- civil date helpers (independent of platform libc/timezone) ---


def epoch_from_civil(y: int, m: int, d: int) -> int:
    yy = y - (1 if m <= 2 else 0)
    era = (yy if yy >= 0 else yy - 399) // 400
    yoe = yy - era * 400
    mp = (m + 9) % 12
    doy = (153 * mp + 2) // 5 + d - 1
    doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
    return (era * 146097 + doe - 719468) * 86400


def make_utc(y: int, mo: int, d: int, h: int = 0, mi: int = 0) -> int:
    return epoch_from_civil(y, mo, d) + h * 3600 + mi * 60


def break_kst(epoch_utc: int) -> dict:
    kst = epoch_utc + KST_OFFSET_SEC
    days, rem = divmod(kst, 86400)
    dt = _dt.datetime(1970, 1, 1) + _dt.timedelta(days=days)
    return {
        "year": dt.year,
        "month": dt.month,
        "day": dt.day,
        "hour": rem // 3600,
        "minute": (rem % 3600) // 60,
        "weekday": (days + 4) % 7,  # 1970-01-01 was Thursday
    }


def is_kst_weekend(epoch_utc: int) -> bool:
    return break_kst(epoch_utc)["weekday"] in (0, 6)


def kst_datestring(epoch_utc: int) -> str:
    b = break_kst(epoch_utc)
    return f"{b['year']:04d}-{b['month']:02d}-{b['day']:02d}"


def kst_datetime_string(epoch_utc: int) -> str:
    b = break_kst(epoch_utc)
    return (
        f"{b['year']:04d}-{b['month']:02d}-{b['day']:02d} "
        f"{b['hour']:02d}:{b['minute']:02d} KST"
    )


def classify_freshness(
    observed_utc: int, now_utc: int, ttl_sec: int, clock_verified: bool
) -> Validity:
    if not clock_verified:
        return Validity.Stale  # '시간 확인 필요' path
    if observed_utc <= 0 or observed_utc > now_utc + 300:
        return Validity.Stale
    return Validity.Valid if now_utc - observed_utc <= ttl_sec else Validity.Stale


# --- provider outcome + fetch context (src/domain/Types.h) ---


class Outcome(str, Enum):
    Ok = "OK"
    NoData = "NO_DATA"
    AuthError = "AUTH_ERROR"
    QuotaExceeded = "QUOTA_EXCEEDED"
    RateLimited = "RATE_LIMITED"
    ProviderError = "PROVIDER_ERROR"


VALIDITY_NAME = {  # == kdash::ToString(Validity) on the C++ side
    Validity.Valid: "VALID",
    Validity.Missing: "MISSING",
    Validity.ProviderError: "PROVIDER_ERROR",
    Validity.Stale: "STALE",
    Validity.NotConfigured: "NOT_CONFIGURED",
}


@dataclass
class FetchContext:
    fetched_utc: int = 0
    now_utc: int = 0
    clock_verified: bool = False


def device_ts(ctx: FetchContext) -> Timestamp:
    return Timestamp(ctx.fetched_utc, ctx.clock_verified)


def source_ts(epoch_utc: int) -> Timestamp:
    return Timestamp(epoch_utc, epoch_utc > 0)


def kst_to_utc(y: int, mo: int, d: int, h: int = 0, mi: int = 0) -> int:
    """KST wall clock -> UTC epoch; h == 24 means next day 00:00."""
    return make_utc(y, mo, d, h, mi) - KST_OFFSET_SEC


def is_real_date(y: int, mo: int, d: int) -> bool:
    """True calendar date: 02-30 etc. rejected via civil round-trip."""
    if not (1 <= mo <= 12 and 1 <= d <= 31):
        return False
    back = break_kst(epoch_from_civil(y, mo, d))
    return back["year"] == y and back["month"] == mo and back["day"] == d


def kst_ymd_hm_to_utc(yyyymmdd: int, hhmm: int) -> int:
    y, mo, d = yyyymmdd // 10000, (yyyymmdd // 100) % 100, yyyymmdd % 100
    h, mi = hhmm // 100, hhmm % 100
    if not (
        2000 <= y <= 2200 and is_real_date(y, mo, d) and 0 <= h <= 24 and 0 <= mi <= 59
    ) or (h == 24 and mi != 0):
        return -1
    return kst_to_utc(y, mo, d, h, mi)


def kst_str_to_utc(yyyymmdd, hhmm) -> int:
    if not (isinstance(yyyymmdd, str) and isinstance(hhmm, str)):
        return -1
    if len(yyyymmdd) != 8 or len(hhmm) != 4 or not (yyyymmdd + hhmm).isdigit():
        return -1
    return kst_ymd_hm_to_utc(int(yyyymmdd), int(hhmm))
