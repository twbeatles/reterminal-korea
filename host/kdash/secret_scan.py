"""Secret detection + redaction shared by tools/fixture_sanitizer.py and the
SEC-01 repository scan (ISSUE-002).

Catches the forms real captures actually contain:
  - URL query parameters  ...?serviceKey=FAKE%2B...&  / authkey=... (unquoted)
  - JSON/dict keys        "access_token": "...", "appsecret": "...", ...
  - HTTP headers          Authorization: Bearer ..., appkey: ..., Cookie: ...
  - JWT-looking tokens, PEM private keys, e-mail addresses, KR mobile numbers
Values that are obviously placeholders (FAKE/DEMO/EXAMPLE/REDACTED/{x}/%s/
<x>/$X, or shorter than 8 chars) are not secrets.
"""

from __future__ import annotations

import re

REDACTED = "***REDACTED***"

# Dict keys whose VALUE is secret (case-insensitive substring match).
SECRET_KEY_PARTS = (
    "servicekey",
    "authkey",
    "appkey",
    "appsecret",
    "secret",
    "token",
    "password",
    "passwd",
    "authorization",
    "cookie",
    "sessionid",
    "session_id",
    "account",
    "acctno",
    "apikey",
    "api_key",
    "approval_key",
    "private_key",
)

_PARAM_NAMES = (
    r"serviceKey|service_key|authkey|appkey|appsecret|app_secret|"
    r"access_token|refresh_token|token|api_key|apikey|approval_key|"
    r"password|passwd|secret"
)
QUERY_PARAM = re.compile(
    r"(?i)\b(" + _PARAM_NAMES + r")\s*=\s*['\"]?([^&\s'\"`#,;)}\]]+)"
)
HEADER = re.compile(
    r"(?i)\b(authorization|appkey|appsecret|cookie|set-cookie|"
    r"x-api-key)\s*:\s*(?:bearer\s+)?([^\s'\",;]+)"
)
JSON_KV = re.compile(
    r"(?i)['\"]([A-Za-z0-9_\-]*(?:"
    + "|".join(SECRET_KEY_PARTS)
    + r")[A-Za-z0-9_\-]*)['\"]\s*:\s*['\"]([^'\"]+)['\"]"
)
BEARER = re.compile(r"(?i)\bbearer\s+([A-Za-z0-9._~+/=\-]{8,})")
JWT = re.compile(r"\beyJ[A-Za-z0-9_\-]{8,}\.[A-Za-z0-9_\-]{8,}")
PEM = re.compile(r"BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY")
EMAIL = re.compile(r"[\w.+-]+@(?:[\w-]+\.)+[A-Za-z]{2,}")
PHONE = re.compile(r"\b01[016789]-?\d{3,4}-?\d{4}\b")
DEV_DEFINE = re.compile(
    r'#\s*define\s+KDASH_DEV_(?:[A-Z_]*KEY|[A-Z_]*SECRET|WIFI_SSID)\s+"([^"\r\n]+)"'
)

_PLACEHOLDER = re.compile(
    r"(?i)(fake|demo|example|redacted|placeholder|dummy|"
    r"xxxx|your[_-]?|sample|test[_-]?key|\*\*\*)"
)


def is_placeholder(value: str) -> bool:
    v = value.strip()
    if len(v) < 8:
        return True
    if v[0] in "{<$%" or v.startswith("%s") or "%s" in v:
        return True
    if re.fullmatch(r"[\d\s:./\-TZ+]+", v):  # dates/times/numbers (expiry etc.)
        return True
    return bool(_PLACEHOLDER.search(v))


def find_secrets(text: str):
    """-> list of (line_no, kind) for non-placeholder secrets in text."""
    hits = []
    for no, line in enumerate(text.splitlines(), 1):
        for kind, rx, group in (
            ("query", QUERY_PARAM, 2),
            ("header", HEADER, 2),
            ("json", JSON_KV, 2),
            ("bearer", BEARER, 1),
            ("jwt", JWT, 0),
        ):
            for m in rx.finditer(line):
                if not is_placeholder(m.group(group)):
                    hits.append((no, kind))
        if PEM.search(line):
            hits.append((no, "pem"))
        for m in EMAIL.finditer(line):
            if not is_placeholder(m.group(0)) and not m.group(0).endswith(
                ("@anthropic.com", "@example.com", "@users.noreply.github.com")
            ):
                hits.append((no, "email"))
        if PHONE.search(line):
            hits.append((no, "phone"))
        for m in DEV_DEFINE.finditer(line):
            if not is_placeholder(m.group(1)):
                hits.append((no, "credential_define"))
    return hits


def redact_text(s: str) -> str:
    s = QUERY_PARAM.sub(lambda m: m.group(0).replace(m.group(2), REDACTED), s)
    s = BEARER.sub(lambda m: m.group(0).replace(m.group(1), REDACTED), s)
    s = JWT.sub(REDACTED, s)
    s = EMAIL.sub(REDACTED, s)
    s = PHONE.sub(REDACTED, s)
    return s


def _secret_key(k: str) -> bool:
    k = k.lower().replace("-", "_")
    return any(part in k for part in SECRET_KEY_PARTS)


def scrub(obj):
    """Recursively redact secret-named keys and secret-looking string values."""
    if isinstance(obj, dict):
        return {
            k: (
                REDACTED
                if isinstance(k, str) and _secret_key(k) and v is not None
                else scrub(v)
            )
            for k, v in obj.items()
        }
    if isinstance(obj, list):
        return [scrub(v) for v in obj]
    if isinstance(obj, str):
        return redact_text(obj)
    return obj
