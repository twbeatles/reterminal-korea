"""AirKorea edge cases beyond the golden file (AIR-01/02, 24:00)."""

from kdash import airkorea as A
from kdash import models as M


def test_midnight_2400_is_next_day():
    assert A.parse_data_time_kst("2026-11-01 24:00") == M.kst_to_utc(2026, 11, 2, 0, 0)
    assert A.parse_data_time_kst("2026-11-01 24:30") == -1
    assert A.parse_data_time_kst("점검중") == -1


def test_nonexistent_date_rejected():
    assert A.parse_data_time_kst("2026-02-30 12:00") == -1
    assert A.parse_data_time_kst("2023-02-29 12:00") == -1
    assert A.parse_data_time_kst("2024-02-29 12:00") > 0


def test_int_parsing_never_zero_for_missing():
    for bad in ("-", "", None, "점검", "-12", "１２"):
        assert A.parse_int(bad) is None, bad
    assert A.parse_int(" 45 ") == 45
