"""P5 guards: symbol/market validation, KRX hours, FSC gate (read-only)."""

from kdash import finance as F
from kdash import models as M


def kst(y, mo, d, h, mi):
    return M.kst_to_utc(y, mo, d, h, mi)


def test_watchlist_and_market_hours():
    assert F.is_valid_symbol("005930") and not F.is_valid_symbol("5930")
    assert not F.is_valid_symbol("00593A") and not F.is_valid_symbol("００５９３０")
    assert F.is_valid_market("KRX") and F.is_valid_market("NXT")
    assert F.is_valid_market("통합") and not F.is_valid_market("NYSE")
    assert F.is_krx_open(kst(2026, 11, 2, 13, 30))
    assert not F.is_krx_open(kst(2026, 10, 31, 10, 0))  # Sat
    assert not F.is_krx_open(kst(2026, 11, 2, 8, 59))
    assert F.is_krx_open(kst(2026, 11, 2, 9, 0))
    assert not F.is_krx_open(kst(2026, 11, 2, 15, 30))


def test_fsc_due_gate():
    assert (
        F.fsc_fetch_due(kst(2026, 11, 2, 12, 0), 0) is True
    )  # 성공 이력 없음: 13시 전에도 1회(직전 영업일 종가)
    assert (
        F.fsc_fetch_due(kst(2026, 11, 2, 12, 0), kst(2026, 10, 30, 14, 0)) is False
    )  # 이력 있으면 13시까지 대기
    assert F.fsc_fetch_due(kst(2026, 11, 2, 13, 30), 0) is True
    assert F.fsc_fetch_due(kst(2026, 11, 2, 13, 30), kst(2026, 11, 2, 13, 31)) is False
    assert F.fsc_fetch_due(kst(2026, 10, 31, 14, 0), 0) is False  # Sat
