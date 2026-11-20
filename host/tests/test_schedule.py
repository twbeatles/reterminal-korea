"""Schedule/power/backoff + KST/timezone oracles (06 + 07 PWR)."""

from kdash import models as M
from kdash import policy as P


def test_base_intervals_usb_vs_battery():
    assert P.BASE_INTERVALS["kma_now"] == (1800, 3600)
    assert P.BASE_INTERVALS["sht40"] == (300, 900)
    assert P.BASE_INTERVALS["exim"] == (12 * 3600, 24 * 3600)


def test_next_due_never_fetched_is_now():
    assert P.next_due(0, 3600) == 0
    assert P.next_due(1000, 3600) == 4600


def test_backoff_grows_capped_with_jitter():
    assert P.backoff_sec(0, 0) == 2
    assert P.backoff_sec(1, 0) == 4
    assert P.backoff_sec(8, 0) == 300
    assert P.backoff_sec(20, 0) == 300
    assert P.backoff_sec(1, 999) >= 4  # jitter only adds


def test_retry_budgets_auth_strict():
    assert P.retry_allowed("kma_now", 0, False) is True
    assert P.retry_allowed("kma_now", 1, False) is True
    assert P.retry_allowed("kma_now", 2, False) is False
    assert P.retry_allowed("kis_price", 1, True) is False  # 401: 1회 이하
    assert P.retry_allowed("exim", 0, True) is True
    assert P.retry_allowed("exim", 1, True) is False


def test_rate_limit_respects_retry_after():
    assert P.rate_limit_delay("120", 3) == 120
    assert P.rate_limit_delay(None, 1, 0) == 4
    assert P.rate_limit_delay("not-a-date", 0, 0) == 2


def test_power_machine_single_pass_terminates():
    s = "ACTIVE_FETCH"
    seen = [s]
    for _ in range(6):  # PWR-01 miniature: full cycle, no infinite loop
        r = P.advance_power(s, data_changed=True, usb=False)
        s = r["next"]
        seen.append(s)
    assert seen == [
        "ACTIVE_FETCH",
        "RENDER_IF_CHANGED",
        "SET_WAKE_ALARM",
        "DEEP_SLEEP",
        "WAKE_BOOT",
        "ACTIVE_FETCH",
        "RENDER_IF_CHANGED",
    ]
    idle = P.advance_power("RENDER_IF_CHANGED", False, usb=True)
    assert idle["next"] == "IDLE_USB" and idle["render"] is False


def test_100_sleep_wake_loops_stable():
    s = "DEEP_SLEEP"
    for _ in range(100):
        assert P.advance_power(s, False, False)["next"] == "WAKE_BOOT"
        s = P.advance_power("WAKE_BOOT", False, False)["next"]
        assert s == "ACTIVE_FETCH"
        s = P.advance_power(P.advance_power(s, False, False)["next"], False, False)[
            "next"
        ]
        assert s in ("IDLE_USB", "SET_WAKE_ALARM")
        s = "DEEP_SLEEP"


def test_kst_oracle_matches_stdlib():
    import datetime

    for h in (0, 8, 15, 23):
        e = M.make_utc(2026, 11, 1, h, 30)
        expect = datetime.datetime(1970, 1, 1) + datetime.timedelta(
            seconds=e + 9 * 3600
        )
        b = M.break_kst(e)
        assert (b["year"], b["month"], b["day"], b["hour"], b["minute"]) == (
            expect.year,
            expect.month,
            expect.day,
            expect.hour,
            expect.minute,
        )
