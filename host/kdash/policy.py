"""Schedule/power/cache/settings/net/provision mirrors."""

from __future__ import annotations

import re
import struct as _st
import zlib

BASE_INTERVALS = {  # seconds: (usb, battery) — 06 기본 계획값
    "sht40": (5 * 60, 15 * 60),
    "kma_now": (30 * 60, 60 * 60),
    "kma_forecast": (3 * 3600, 6 * 3600),  # 상한; 실제론 발표 회차 구동
    "airkorea": (3600, 7200),
    "exim": (12 * 3600, 24 * 3600),
    "fsc_close": (24 * 3600, 24 * 3600),
    "kis_price": (5 * 60, 15 * 60),  # 장중만
}


def next_due(last_success_utc: int, interval_sec: int) -> int:
    return 0 if last_success_utc <= 0 else last_success_utc + interval_sec


def backoff_sec(attempt: int, jitter_slot_ms: int = 0) -> int:
    attempt = max(0, min(8, attempt))
    base = min(2 << attempt, 300)
    return base + (jitter_slot_ms % 1000) * base // 1000 // 2


def retry_allowed(provider: str, attempt: int, auth_error: bool) -> bool:
    if auth_error:
        return attempt < 1
    return attempt < 2


def rate_limit_delay(retry_after, attempt: int, jitter: int = 0) -> int:
    try:
        v = int(str(retry_after).strip())
        if 0 < v <= 3600:
            return v
    except (ValueError, TypeError, AttributeError):
        pass
    return backoff_sec(attempt, jitter)


# --- power state machine ---


def advance_power(state: str, data_changed: bool, usb: bool) -> dict:
    table = {
        "ACTIVE_FETCH": ("RENDER_IF_CHANGED", True, False, False),
        "RENDER_IF_CHANGED": (
            ("IDLE_USB" if usb else "SET_WAKE_ALARM"),
            False,
            data_changed,
            False,
        ),
        "IDLE_USB": ("ACTIVE_FETCH", False, False, False),
        "SET_WAKE_ALARM": ("DEEP_SLEEP", False, False, True),
        "DEEP_SLEEP": ("WAKE_BOOT", False, False, False),
        "WAKE_BOOT": ("ACTIVE_FETCH", False, False, False),
    }
    nxt, fetch, render, sleep = table[state]
    return {"next": nxt, "fetch": fetch, "render": render, "sleep": sleep}


# --- cache: magic + schema + crc32 framing ---

CACHE_MAGIC = 0x4B444341
CACHE_SCHEMA = 1
_CRC_FMT = "<IHHqqII"  # magic, schema, kind, observed, fetched, len, crc

HEADER_LEN = _st.calcsize(_CRC_FMT)


def cache_encode(payload: bytes, kind: int, observed_utc: int, fetch_utc: int) -> bytes:
    hdr = _st.pack(
        _CRC_FMT,
        CACHE_MAGIC,
        CACHE_SCHEMA,
        kind,
        observed_utc,
        fetch_utc,
        len(payload),
        0,
    )
    crc = zlib.crc32(hdr + payload) & 0xFFFFFFFF
    return (
        _st.pack(
            _CRC_FMT,
            CACHE_MAGIC,
            CACHE_SCHEMA,
            kind,
            observed_utc,
            fetch_utc,
            len(payload),
            crc,
        )
        + payload
    )


def cache_decode(blob: bytes):
    if len(blob) < HEADER_LEN:
        return None
    magic, schema, kind, obs, fet, ln, want = _st.unpack(_CRC_FMT, blob[:HEADER_LEN])
    if magic != CACHE_MAGIC or schema != CACHE_SCHEMA:
        return None
    if len(blob) != HEADER_LEN + ln:
        return None
    zeroed = _st.pack(_CRC_FMT, magic, schema, kind, obs, fet, ln, 0)
    if zlib.crc32(zeroed + blob[HEADER_LEN:]) & 0xFFFFFFFF != want:
        return None
    return {
        "kind": kind,
        "observed_utc": obs,
        "fetch_utc": fet,
        "payload": blob[HEADER_LEN:],
    }


# --- settings validation / redaction (src/storage/SettingsStore.*) ---
# Lengths are UTF-8 BYTES, like the device buffers (ISSUE-007).

MAX_SSID, MIN_WPA, MAX_SECRET, MAX_KEY = 32, 8, 64, 256
MAX_STATION, MAX_DISPLAY_NAME, MAX_WATCHLIST = 48, 64, 4
MARKETS = ("KRX", "NXT", "통합")


def _bytes(s) -> int:
    return len(str(s).encode("utf-8"))


def _bounded_text(s, max_len: int) -> bool:
    return (
        isinstance(s, str)
        and s != ""
        and _bytes(s) <= max_len
        and all(ord(c) >= 0x20 and ord(c) != 0x7F for c in s)
    )


def validate_location(nx, ny, name, station) -> str:
    if not (isinstance(nx, int) and isinstance(ny, int)) or not (
        1 <= nx <= 200 and 1 <= ny <= 200
    ):
        return "격자(nx,ny) 범위 오류"
    if not _bounded_text(name, MAX_DISPLAY_NAME) or not _bounded_text(
        station, MAX_STATION
    ):
        return "지역명/측정소명 오류"
    return "OK"


def validate_wifi(ssid, secret) -> str:
    if not ssid or _bytes(ssid) > MAX_SSID:
        return "SSID 오류"
    if not secret:
        return "OK"  # open network
    if len(secret) == MAX_SECRET:
        return (
            "OK"
            if all(c in "0123456789abcdefABCDEF" for c in secret)
            else "Wi-Fi 비밀번호 오류(8~63자)"
        )
    if not (MIN_WPA <= len(secret) <= MAX_SECRET - 1) or not all(
        0x20 <= ord(c) <= 0x7E for c in secret
    ):
        return "Wi-Fi 비밀번호 오류(8~63자)"
    return "OK"


def validate_key(key) -> str:
    if not key:
        return "OK"  # feature disabled
    if _bytes(key) > MAX_KEY or not all(
        0x21 <= ord(c) <= 0x7E and c not in ('"', "\\") for c in key
    ):
        return "키 형식 오류"
    return "OK"


def validate_watch_item(symbol, market) -> str:
    if not (
        isinstance(symbol, str)
        and len(symbol) == 6
        and symbol.isdigit()
        and symbol.isascii()
    ):
        return "종목코드 오류(6자리 숫자)"
    if market not in MARKETS:
        return "시장구분 오류"
    return "OK"


def parse_watchlist(text):
    """'005930, 000660:NXT' -> ('OK', [(sym, market)]) or (error, None)."""
    if text is None:
        return "관심종목 형식 오류", None
    if not text.strip():
        return "OK", []
    out = []
    for tok in text.split(","):
        tok = tok.strip()
        if not tok or len(out) >= MAX_WATCHLIST:
            return "관심종목 형식 오류", None
        if _bytes(tok) >= 32:
            return "종목코드 오류(6자리 숫자)", None
        sym, _, market = tok.partition(":")
        market = market if ":" in tok else "KRX"
        err = validate_watch_item(sym, market)
        if err != "OK":
            return err, None
        out.append((sym, market))
    return "OK", out


def registered_label(secret) -> str:
    return "등록됨" if secret else "미등록"


# --- net policy (src/net/NetPolicy.*) ---

ALLOW_HOSTS = (
    ("apis.data.go.kr", 443),
    ("oapi.koreaexim.go.kr", 443),
    ("openapi.koreainvestment.com", 9443),
)
_HOST_CHARS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-")


def uses_https(url: str) -> bool:
    return isinstance(url, str) and url.startswith("https://")


def host_allowed(url: str) -> bool:
    if not uses_https(url):
        return False
    rest = url[8:]
    end = len(rest)
    for ch in "/?#":
        i = rest.find(ch)
        if i >= 0:
            end = min(end, i)
    authority = rest[:end]
    if authority.count(":") > 1 or any(
        c not in _HOST_CHARS and c != ":" for c in authority
    ):
        return False  # userinfo '@', backslash, spaces, non-ASCII (ISSUE-009)
    host, _, port = authority.partition(":")
    if not host or len(host) > 64:
        return False
    if ":" in authority:
        if not port.isdigit() or len(port) > 5:
            return False
        port_n = int(port)
    else:
        port_n = 443
    return (host.lower(), port_n) in ALLOW_HOSTS


def parse_retry_after(value):
    if value is None:
        return -1
    s = str(value).strip()
    if not s or not s[0].isdigit():
        return -1
    m = re.match(r"\d+", s)
    return min(int(m.group(0)), 3600) if m else -1


def body_size_allowed(nbytes: int, cap: int) -> bool:
    return nbytes <= cap


# --- provision portal validation (src/provision/SetupPortal.*) ---

FORM_FIELD_MAX = 256


def validate_field(field: str, value) -> str:
    v = "" if value is None else str(value)
    if _bytes(v) > FORM_FIELD_MAX:
        return "입력이 너무 깁니다"
    if field == "ssid":
        return (
            "OK"
            if validate_wifi(v, None) == "OK"
            else "SSID가 비어 있거나 32바이트를 초과합니다"
        )
    if field == "wifi_secret":
        return (
            "OK"
            if validate_wifi("x", v) == "OK"
            else "Wi-Fi 비밀번호는 8~63자(또는 64자리 16진수)입니다"
        )
    if field in ("kma_key", "air_key", "exim_key", "kis_app_key", "kis_app_secret"):
        return (
            "OK"
            if validate_key(v) == "OK"
            else "키에 공백/따옴표가 있거나 256자를 초과합니다"
        )
    if field in ("grid_x", "grid_y"):
        if not (v.isdigit() and v.isascii() and len(v) <= 3):
            return "격자는 숫자입니다"
        return "OK" if 1 <= int(v) <= 200 else "격자 범위 1–200 오류"
    if field == "display_name":
        return (
            "OK"
            if validate_location(1, 1, v, "x") == "OK"
            else "지역명을 입력하세요(64바이트, 한글 21자 이내)"
        )
    if field == "air_station":
        return (
            "OK"
            if validate_location(1, 1, "x", v) == "OK"
            else "측정소명을 입력하세요(48바이트, 한글 16자 이내)"
        )
    if field == "watchlist":
        err, _ = parse_watchlist(v)
        return "OK" if err == "OK" else err
    return "알 수 없는 필드"


def escape_html(s, out_cap: int = 1 << 30) -> str:
    rep = {"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}
    out, n = [], 0
    for ch in str(s or ""):
        piece = rep.get(ch, ch)
        size = len(piece.encode("utf-8")) if ch not in rep else len(piece)
        if n + size + 1 > out_cap:
            break  # never emit a partial entity
        out.append(piece)
        n += size
    return "".join(out)


_AP_PREFIX = "KDASH-E1001-"
AP_ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"


def make_ap_name(suffix4: str) -> str:
    if not (
        isinstance(suffix4, str)
        and len(suffix4) == 4
        and all(c in "0123456789ABCDEF" for c in suffix4)
    ):
        raise ValueError("suffix must be 4 hex chars (no PII/serial)")
    return _AP_PREFIX + suffix4


def uniform_below(next_u32, bound: int) -> int:
    """Rejection sampling, == kdash::UniformBelow (no modulo bias)."""
    limit = (1 << 32) - ((1 << 32) % bound)
    while True:
        r = next_u32() & 0xFFFFFFFF
        if r < limit:
            return r % bound


def make_ap_password(next_u32, n: int = 12) -> str:
    """AP password from an RNG port (device: hardware RNG). ISSUE-010."""
    return "".join(
        AP_ALPHABET[uniform_below(next_u32, len(AP_ALPHABET))] for _ in range(n)
    )


def issue_csrf_token(next_u32) -> str:
    out = []
    for _ in range(4):
        r = next_u32() & 0xFFFFFFFF
        for _ in range(8):
            out.append("0123456789ABCDEF"[r & 0xF])
            r >>= 4
    return "".join(out)


def check_csrf_token(presented, expected) -> bool:
    if not presented or not expected or len(presented) != len(expected):
        return False
    diff = 0
    for a, b in zip(presented, expected, strict=True):
        diff |= ord(a) ^ ord(b)
    return diff == 0


# --- sensor plausibility (src/drivers/SensorCheck.*) ---


def check_sensor_range(temp_c, humidity_pct: float) -> str:
    try:
        t, h = float(temp_c), float(humidity_pct)
    except (TypeError, ValueError):
        return "NoData"
    if t != t or h != h:  # NaN => 결측
        return "NoData"
    if not (-40.0 <= t <= 125.0) or not (0.0 <= h <= 100.0):
        return "NoData"
    return "Ok"


def is_indoor_plausible(temp_c) -> bool:
    try:
        return -10.0 <= float(temp_c) <= 50.0
    except (TypeError, ValueError):
        return False


# --- refresh policy (src/ui/RefreshPolicy.*) ---

MAX_PARTIALS_BEFORE_FULL = 10


def decide_refresh(
    content_changed: bool,
    region_aligned: bool,
    partials_since_full: int,
    partial_supported: bool = False,
    gray_mode: bool = False,
) -> str:
    if not content_changed:
        return "Skip"
    if not partial_supported or not region_aligned:
        return "Full"
    if gray_mode:
        return "Full"  # Gray4 partial 미검증
    if partials_since_full >= MAX_PARTIALS_BEFORE_FULL:
        return "Full"
    return "Partial"


def note_refresh_done(decision: str, partials_since_full: int) -> int:
    if decision == "Full":
        return 0
    if decision == "Partial":
        return partials_since_full + 1
    return partials_since_full
