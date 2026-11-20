"""Endpoint builders (03) + portal hardening + sensor/refresh/FSC gates."""

import pytest
from kdash import endpoints as E
from kdash import finance as F
from kdash import models as M
from kdash import policy as P


def test_kma_urls_exact_documented_shape():
    u = E.kma_ultra_url("KEY1", 20261101, 1400, 60, 127)
    assert u == (
        "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/"
        "getUltraSrtNcst?serviceKey=KEY1&pageNo=1&numOfRows=16"
        "&dataType=JSON&base_date=20261101&base_time=1400&nx=60&ny=127"
    )
    v = E.kma_village_url("KEY1", 20261101, 1400, 60, 127)
    assert "/getVilageFcst?" in v and "numOfRows=72" in v and "base_time=1400" in v
    assert P.host_allowed(u) and P.host_allowed(v)


def test_service_key_both_portal_forms():
    # "Encoding" key: verbatim, never re-encoded.
    u = E.kma_ultra_url("FAKE%2BDEF%3D", 20261101, 1400, 60, 127)
    assert "serviceKey=FAKE%2BDEF%3D&" in u and "%25" not in u
    # "Decoding" key: encoded exactly once (G-6).
    u = E.kma_ultra_url("FAKE+DEF/G==", 20261101, 1400, 60, 127)
    assert "serviceKey=FAKE%2BDEF%2FG%3D%3D&" in u
    for bad in ("ABC%2B+DEF", "ABC%ZZ", "ABC DEF", ""):
        with pytest.raises(ValueError):
            E.normalize_service_key(bad)


def test_station_encoded_once_double_encode_rejected():
    u = E.airkorea_candidate_url("K", "종로구")
    assert "stationName=%EC%A2%85%EB%A1%9C%EA%B5%AC" in u
    assert "getMsrstnAcctoRltmMesureDnsty" in u and "returnType=json" in u
    with pytest.raises(ValueError):
        E.encode_once("100%EC%A2%85")


def test_exim_url_new_host_only():
    u = E.exim_url("AUTH", 20261030)
    assert u == (
        "https://oapi.koreaexim.go.kr/site/program/financial/"
        "exchangeJSON?authkey=AUTH&searchdate=20261030&data=AP01"
    )
    assert "www.koreaexim.go.kr" not in u
    assert P.host_allowed(u)


def test_fsc_and_kis_urls_pass_allowlist():
    f = E.fsc_price_url("K", "005930", 20261019)
    assert "/1160100/GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2?" in f
    assert "getStockPriceInfo" in f and "likeSrtnCd=005930" in f and P.host_allowed(f)
    k = E.kis_price_url("005930", "NXT")
    assert k.startswith("https://openapi.koreainvestment.com:9443/uapi/")
    assert "FID_COND_MRKT_DIV_CODE=NX" in k and P.host_allowed(k)
    assert P.host_allowed(E.kis_token_url())


def test_ap_name_no_pii():
    assert P.make_ap_name("1A2B") == "KDASH-E1001-1A2B"
    with pytest.raises(ValueError):
        P.make_ap_name("SN12")  # lowercase/serial 형태 거부
    with pytest.raises(ValueError):
        P.make_ap_name("12345")


def test_sensor_range_gates():
    assert P.check_sensor_range(22.0, 48.0) == "Ok"
    assert P.check_sensor_range(float("nan"), 48.0) == "NoData"
    assert P.check_sensor_range(22.0, 101.0) == "NoData"
    assert P.check_sensor_range(-50.0, 10.0) == "NoData"  # 센서 범위 밖
    assert P.is_indoor_plausible(22.0) is True
    assert P.is_indoor_plausible(80.0) is False  # 찜질방 오해 방지


def test_refresh_policy_skip_partial_full():
    assert P.decide_refresh(False, True, 0, True) == "Skip"
    assert P.decide_refresh(True, True, 0, False) == "Full"  # 미검증=Full
    assert P.decide_refresh(True, True, 0, True) == "Partial"
    assert P.decide_refresh(True, True, 10, True) == "Full"  # 잔상 방지
    assert P.decide_refresh(True, True, 0, True, gray_mode=True) == "Full"
    assert P.decide_refresh(True, False, 0, True) == "Full"  # 정렬 불가
    assert P.note_refresh_done("Full", 9) == 0
    assert P.note_refresh_done("Partial", 9) == 10
    assert P.note_refresh_done("Skip", 9) == 9


def test_fsc_due_gate():
    mon_before = M.make_utc(2026, 11, 2, 3, 0)  # Mon 12:00 KST
    mon_after = M.make_utc(2026, 11, 2, 4, 30)  # Mon 13:30 KST
    assert F.fsc_fetch_due(mon_before, 0) is True  # 성공 이력 없으면 13시 전에도 1회
    assert (
        F.fsc_fetch_due(mon_before, M.make_utc(2026, 10, 30, 6, 0)) is False
    )  # 이력 있으면 13시 이후
    assert F.fsc_fetch_due(mon_after, 0) is True
    assert F.fsc_fetch_due(mon_after, mon_after + 60) is False  # 당일 성공분
    assert F.fsc_fetch_due(M.make_utc(2026, 10, 31, 5, 0), 0) is False  # Sat
