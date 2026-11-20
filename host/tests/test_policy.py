"""CACHE-01 + validation + provision + NET policy (02/04/07)."""

import random

from kdash import policy as P


def test_cache01_roundtrip_and_bitflip_rejected():
    blob = P.cache_encode(b'{"t":21.5}', 1, 1000, 1100)
    ok = P.cache_decode(blob)
    assert ok and ok["payload"] == b'{"t":21.5}' and ok["observed_utc"] == 1000
    bad = bytearray(blob)
    bad[-1] ^= 0x01  # fault injection: single bit flip
    assert P.cache_decode(bytes(bad)) is None  # last-known-good/리셋 경로로
    assert P.cache_decode(blob[:5]) is None
    assert P.cache_decode(b"\x00" * 40) is None  # bad magic


def test_settings_validation_bytes_not_chars():
    assert P.validate_location(60, 127, "Seoul", "Jongno") == "OK"
    assert P.validate_location(0, 127, "x", "y") == "격자(nx,ny) 범위 오류"
    # ISSUE-007: limits are UTF-8 bytes (device buffers), not characters.
    assert P.validate_location(60, 127, "가" * 21, "종로구") == "OK"
    assert P.validate_location(60, 127, "가" * 22, "종로구") != "OK"
    assert P.validate_location(60, 127, "a" * 64, "종로구") == "OK"
    assert P.validate_location(60, 127, "a" * 65, "종로구") != "OK"
    assert P.validate_wifi("", None) == "SSID 오류"
    assert P.validate_wifi("home", "") == "OK"  # open network
    assert P.validate_wifi("home", "pw") != "OK"  # WPA2: 8..63 (G-7)
    assert P.validate_wifi("home", "12345678") == "OK"
    assert P.validate_wifi("home", "a" * 64) == "OK"  # 64 hex PSK
    assert P.validate_wifi("home", "z" * 64) != "OK"
    assert P.validate_key("K" * 256) == "OK"  # KIS App Secret (~180) fits
    assert P.validate_key("K" * 257) != "OK"
    assert P.validate_key("has space") != "OK"
    assert P.validate_watch_item("005930", "KRX") == "OK"
    assert P.validate_watch_item("5930", "KRX") == "종목코드 오류(6자리 숫자)"
    assert P.validate_watch_item("005930", "NYSE") == "시장구분 오류"
    assert P.registered_label("") == "미등록"
    assert P.registered_label("abc") == "등록됨"  # 값 자체는 절대 노출 금지


def test_watchlist_parsing_g8():
    assert P.parse_watchlist(" 005930 , 000660:NXT") == (
        "OK",
        [("005930", "KRX"), ("000660", "NXT")],
    )
    assert P.parse_watchlist("") == ("OK", [])
    for bad in (
        "005930,",
        ",,,",
        "005930,,000660",
        "000001,000002,000003,000004,000005",
        "5930",
        "005930:NYSE",
    ):
        assert P.parse_watchlist(bad)[0] != "OK", bad


def test_provision_portal_guards():
    assert P.validate_field("ssid", "") != "OK"
    assert P.validate_field("grid_x", "60") == "OK"
    assert P.validate_field("grid_x", "999") != "OK"
    assert P.validate_field("watchlist", "005930,000660") == "OK"
    assert P.validate_field("watchlist", "005930,") != "OK"
    assert P.validate_field("exim_key", "x" * 257) != "OK"
    assert P.validate_field("kis_app_secret", "S" * 180) == "OK"
    assert P.escape_html('<ssid>"&') == "&lt;ssid&gt;&quot;&amp;"
    assert P.escape_html("abcd<", out_cap=8) == "abcd"  # no partial entity


def test_ap_password_and_csrf_from_rng_port():
    rng = random.Random(1)
    pw = P.make_ap_password(lambda: rng.getrandbits(32))
    assert len(pw) == 12 and all(c not in "0O1IL" for c in pw)
    rng2 = random.Random(1)
    assert P.make_ap_password(lambda: rng2.getrandbits(32)) == pw  # only via port
    tok = P.issue_csrf_token(lambda: rng.getrandbits(32))
    assert len(tok) == 32 and P.check_csrf_token(tok, tok)
    assert not P.check_csrf_token("A" * 32, tok)


def test_net_policy_allowlist_and_budgets():
    assert P.host_allowed("https://apis.data.go.kr/1360000/x") is True
    assert P.host_allowed("https://APIS.DATA.GO.KR/x") is True
    assert P.host_allowed("https://oapi.koreaexim.go.kr/site/x") is True
    assert P.host_allowed("https://openapi.koreainvestment.com:9443/x") is True
    assert P.host_allowed("https://openapi.koreainvestment.com/x") is False
    assert P.host_allowed("http://apis.data.go.kr/x") is False  # 강등 금지
    assert P.host_allowed("https://evil.example.com/x") is False
    # ISSUE-009: userinfo / backslash tricks
    assert P.host_allowed("https://apis.data.go.kr:443@evil.example/x") is False
    assert P.host_allowed("https://apis.data.go.kr@evil.example/x") is False
    assert P.host_allowed("https://evil.example\\@apis.data.go.kr/x") is False
    assert P.parse_retry_after("120") == 120
    assert P.parse_retry_after("Wed, 21 Oct 2015 07:28:00 GMT") == -1
    assert P.parse_retry_after(None) == -1
    assert P.body_size_allowed(65536, 65536) is True
    assert P.body_size_allowed(65537, 65536) is False
    assert P.check_sensor_range(22.0, 101.0) == "NoData"
    assert P.check_sensor_range(-50.0, 10.0) == "NoData"  # 센서 범위 밖
    assert P.is_indoor_plausible(22.0) is True
    assert P.is_indoor_plausible(80.0) is False  # 찜질방 오해 방지
