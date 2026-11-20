"""Endpoint builders mirror (src/net/Endpoints.*). Keys normalized, names encoded once."""

from __future__ import annotations

import re

_UNRESERVED = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"
_PCT = re.compile(r"%[0-9A-Fa-f]{2}")

KIS_BASE = "https://openapi.koreainvestment.com:9443"
KIS_MARKET = {"KRX": "J", "NXT": "NX", "통합": "UN"}


def encode_once(text: str) -> str:
    if "%" in text:
        raise ValueError("double-encode guard: '%' in input")
    return "".join(
        c if c in _UNRESERVED else "".join("%%%02X" % b for b in c.encode("utf-8"))
        for c in text
    )


def normalize_service_key(key: str) -> str:
    """data.go.kr 'Encoding' key -> verbatim, 'Decoding' key -> encoded once."""
    if not key or any(ord(c) <= 0x20 or ord(c) >= 0x7F for c in key):
        raise ValueError("bad key")
    if "%" not in key:
        return encode_once(key)
    rest = _PCT.sub("", key)
    if "%" in rest or any(c not in _UNRESERVED for c in rest):
        raise ValueError("malformed encoded key")
    return key


def kma_ultra_url(
    key: str, base_date: int, base_time: int, nx: int, ny: int, num_rows: int = 16
) -> str:
    assert num_rows > 0
    return (
        "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/"
        "getUltraSrtNcst?serviceKey=%s&pageNo=1&numOfRows=%d"
        "&dataType=JSON&base_date=%08d&base_time=%04d&nx=%d&ny=%d"
        % (normalize_service_key(key), num_rows, base_date, base_time, nx, ny)
    )


def kma_village_url(
    key: str, base_date: int, base_time: int, nx: int, ny: int, num_rows: int = 72
) -> str:
    assert num_rows > 0
    return (
        "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/"
        "getVilageFcst?serviceKey=%s&pageNo=1&numOfRows=%d"
        "&dataType=JSON&base_date=%08d&base_time=%04d&nx=%d&ny=%d"
        % (normalize_service_key(key), num_rows, base_date, base_time, nx, ny)
    )


def airkorea_candidate_url(key: str, station: str) -> str:
    assert station
    return (
        "https://apis.data.go.kr/B552584/ArpltnInforInqireSvc/"
        "getMsrstnAcctoRltmMesureDnsty?stationName=%s&dataTerm=DAILY"
        "&pageNo=1&numOfRows=24&returnType=json&serviceKey=%s&ver=1.0"
        % (encode_once(station), normalize_service_key(key))
    )


def exim_url(authkey: str, searchdate: int) -> str:
    assert authkey
    return (
        "https://oapi.koreaexim.go.kr/site/program/financial/"
        "exchangeJSON?authkey=%s&searchdate=%08d&data=AP01"
        % (encode_once(authkey), searchdate)
    )


def fsc_price_url(key: str, symbol: str, begin_yyyymmdd: int) -> str:
    assert len(symbol) == 6 and symbol.isdigit()
    return (
        "https://apis.data.go.kr/1160100/"
        "GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2"
        "?serviceKey=%s&numOfRows=10&pageNo=1&resultType=json"
        "&likeSrtnCd=%s&beginBasDt=%08d"
        % (normalize_service_key(key), symbol, begin_yyyymmdd)
    )


def kis_price_url(symbol: str, market: str) -> str:
    assert len(symbol) == 6 and symbol.isdigit()
    return (
        "%s/uapi/domestic-stock/v1/quotations/inquire-price"
        "?FID_COND_MRKT_DIV_CODE=%s&FID_INPUT_ISCD=%s"
        % (KIS_BASE, KIS_MARKET[market], symbol)
    )


def kis_token_url() -> str:
    return KIS_BASE + "/oauth2/tokenP"
