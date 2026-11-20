"""EXIM-01/02 + independent calculator oracle (07 QA matrix).

Oracle: expected scaled values are computed here with Decimal (stdlib),
NOT with kdash.exim.parse_quoted -- a different code path.
"""

from decimal import ROUND_DOWN, Decimal

from kdash import exim


def oracle_scaled(text: str) -> int:
    d = Decimal(text.replace(",", "")) * 100
    return int(d.to_integral_value(rounding=ROUND_DOWN))


def test_exim01_quoted_matches_decimal_oracle():
    for t in ("1,384.50", "921.3", "9.999", "1,612.32", "0.07"):
        assert exim.parse_quoted(t) == (oracle_scaled(t), 2), t
    for bad in ("-", "", "점검중", "0", "0.00", "1.2.3", "12345678901234567"):
        assert exim.parse_quoted(bad) is None, bad


def test_exim02_walk_back_plan_7_days():
    assert exim.plan_search_dates(20261101, 7) == [
        20261101,
        20261031,
        20261030,
        20261029,
        20261028,
        20261027,
        20261026,
    ]
    assert exim.plan_search_dates(20260301, 7)[-1] == 20260223
