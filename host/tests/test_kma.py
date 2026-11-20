"""KMA release calendar + parser edge cases not covered by the golden file."""

from kdash import kma
from kdash import models as M


def kst(y, mo, d, h, mi):
    return M.kst_to_utc(y, mo, d, h, mi)


def test_kma03_ultra_now_is_on_the_hour_with_40min_delay():
    # data.go.kr 15084084: 초단기실황 base_time 정시(HH00). U-05: 40분 출발값.
    assert kma.select_ultra_now_base(kst(2026, 11, 1, 14, 39)) == (20261101, 1300)
    assert kma.select_ultra_now_base(kst(2026, 11, 1, 14, 40)) == (20261101, 1400)
    assert kma.select_ultra_now_base(kst(2026, 11, 1, 0, 5)) == (20261031, 2300)
    assert kma.select_ultra_now_base(kst(2026, 1, 1, 0, 5)) == (20251231, 2300)


def test_kma03_village_midnight_and_month_rules():
    assert kma.select_village_base(kst(2026, 11, 1, 0, 5)) == (20261031, 2300)
    assert kma.select_village_base(kst(2026, 11, 1, 14, 9)) == (20261101, 1100)
    assert kma.select_village_base(kst(2026, 11, 1, 14, 10)) == (20261101, 1400)
    assert kma.select_village_base(kst(2026, 3, 1, 1, 0)) == (20260228, 2300)


def test_decimal_and_missing_tokens():
    for bad in ("-", "", "강수없음", "12a", "1e5", "nan", "inf", "-999", "900"):
        assert kma.parse_decimal(bad) is None, bad
    assert kma.parse_decimal(" -3.5 ") == -3.5
    assert kma.parse_decimal("0") == 0.0  # real 0mm stays a value


def test_first_parseable_occurrence_wins():
    import json

    body = (
        '{"response":{"header":{"resultCode":"00"},"body":{"items":{"item":['
        '{"category":"T1H","obsrValue":"-"},{"category":"T1H","obsrValue":"7"},'
        '{"category":"T1H","obsrValue":"9"}]}}}}'
    )
    doc = json.loads(body)
    for row in doc["response"]["body"]["items"]["item"]:
        row.update(baseDate="20261101", baseTime="1400", nx=60, ny=127)
    body = json.dumps(doc)
    now = kst(2026, 11, 1, 14, 50)
    out = kma.parse_ultra_now(body, 20261101, 1400, M.FetchContext(now, now, True))
    assert out["temp_c"].value == 7.0
