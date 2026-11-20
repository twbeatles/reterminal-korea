"""ISSUE-002 regression: the sanitizer + SEC-01 detector must catch the
secret shapes real API captures contain. All values below are FAKE-shaped
but deliberately NOT marked as placeholders, so the detector must fire.
(This file is excluded from the repo-wide SEC-01 scan for that reason.)
"""

import json
import os
import subprocess
import sys

from kdash.secret_scan import find_secrets, scrub

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
SANITIZER = os.path.join(ROOT, "tools", "fixture_sanitizer.py")

LEAKS = [
    '{"request_url":"https://apis.data.go.kr/x?serviceKey=AbCdEf1234567890%2B&pageNo=1"}',
    '{"access_token":"eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ0ZXN0In0.c2lnbmF0dXJl"}',
    '{"u":"https://oapi.koreaexim.go.kr/x?authkey=Zq81nB2kLp0Q&searchdate=20261030"}',
    '{"headers":{"Authorization":"Bearer abcdEFGH12345678"}}',
    '{"appsecret":"Qw9Er8Ty7Ui6Op5As4Df3Gh2"}',
]
SAFE = (
    '{"response":{"header":{"resultCode":"00"},"body":{"items":[{"pm10Value":"45"}]}}}'
)


def _run(stdin_text, *args):
    return subprocess.run(
        [sys.executable, SANITIZER, *args],
        input=stdin_text,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


def test_detector_flags_every_leak_shape():
    for leak in LEAKS:
        assert find_secrets(leak), leak
    assert find_secrets("GET /a?serviceKey=AbCdEf1234567890 HTTP/1.1")
    assert find_secrets("Authorization: Bearer abcdEFGH12345678")


def test_detector_ignores_placeholders_and_data():
    for ok in (
        SAFE,
        "serviceKey=%s&pageNo=1",
        "authkey={secret}",
        '"kis_app_key": "FAKEAPPKEY0000000"',
        "serviceKey=K&x=1",
        "appkey: ***REDACTED***",
    ):
        assert find_secrets(ok) == [], ok


def test_scrub_removes_values():
    for leak in LEAKS:
        out = json.dumps(scrub(json.loads(leak)), ensure_ascii=False)
        assert find_secrets(out) == [], out


def test_sanitizer_cli_redacts_and_accepts_file_arg(tmp_path):
    for leak in LEAKS:
        r = _run(leak)
        assert r.returncode == 0, r.stderr
        assert find_secrets(r.stdout) == []
    r = _run(SAFE)
    assert json.loads(r.stdout) == json.loads(SAFE)  # data untouched
    f = tmp_path / "raw.json"
    f.write_text(LEAKS[0], encoding="utf-8")
    r = _run("", str(f))  # documented usage: file argument
    assert r.returncode == 0 and "AbCdEf1234567890" not in r.stdout
    r = _run("GET /a?serviceKey=AbCdEf1234567890 HTTP/1.1\n", "--text")
    assert r.returncode == 0 and "AbCdEf1234567890" not in r.stdout
    # Windows PowerShell 5.1 pipes with a UTF-8 BOM: must still parse.
    r = _run("﻿" + LEAKS[0])
    assert r.returncode == 0 and "AbCdEf1234567890" not in r.stdout
