"""L1 pure-logic tests: units, NaN/0/'-' confusion, freshness boundaries."""

from kdash import models as M


def test_kst_midnight_boundary():
    # UTC 15:00 == KST next-day 00:00
    assert M.break_kst(M.make_utc(2026, 10, 31, 15, 0)) == {
        "year": 2026,
        "month": 11,
        "day": 1,
        "hour": 0,
        "minute": 0,
        "weekday": 0,
    }
    assert M.is_kst_weekend(M.make_utc(2026, 10, 31, 15, 0))


def test_not_sunday_no_weekend():
    assert not M.is_kst_weekend(M.make_utc(2026, 10, 29, 3, 0))  # Thu 12:00 KST


def test_freshness_valid_stale_and_unverified_clock():
    now = M.make_utc(2026, 11, 1, 5, 0)
    assert M.classify_freshness(now - 100, now, 3600, True) is M.Validity.Valid
    assert M.classify_freshness(now - 7200, now, 3600, True) is M.Validity.Stale
    assert M.classify_freshness(now - 10, now, 3600, False) is M.Validity.Stale
    assert M.classify_freshness(now + 600, now, 3600, True) is M.Validity.Stale
    assert M.classify_freshness(0, now, 3600, True) is M.Validity.Stale


def test_real_date_rejected_not_normalized():
    assert M.is_real_date(2024, 2, 29)
    assert not M.is_real_date(2023, 2, 29)
    assert not M.is_real_date(2026, 2, 30)
    assert not M.is_real_date(2026, 13, 1)
    assert M.kst_ymd_hm_to_utc(20260230, 1200) == -1
    assert M.kst_ymd_hm_to_utc(20230229, 1200) == -1
    assert M.kst_ymd_hm_to_utc(20240229, 1200) > 0
    assert M.kst_str_to_utc("20260230", "1200") == -1


def test_fx_unit_is_part_of_value():
    a = M.FxRate("USD", 138450, 2)
    b = M.FxRate("JPY(100)", 92130, 2)
    assert a.currency_unit == "USD"
    assert abs(a.rate - 1384.50) < 1e-9
    assert abs(b.rate - 921.30) < 1e-9
    assert a.currency_unit != b.currency_unit
