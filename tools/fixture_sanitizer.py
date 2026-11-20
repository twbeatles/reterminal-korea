"""Strip secrets/PII from captured API responses into shareable fixtures.

Usage:
  python tools/fixture_sanitizer.py raw.json > test/fixtures/x.json
  python tools/fixture_sanitizer.py < raw.json > test/fixtures/x.json
  python tools/fixture_sanitizer.py --text raw.xml > test/fixtures/x.xml

Removes: values of secret-named keys (serviceKey/authkey/appkey/appsecret/
*token*/password/authorization/cookie/session/account...), secret query
parameters inside strings (…?serviceKey=…&), Bearer/JWT tokens, e-mails,
phone numbers. Keeps structure, result codes, units, timestamps.
Refuses (exit 1) to write output that still looks like it holds a secret.
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "host"))
from kdash.secret_scan import find_secrets, redact_text, scrub  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", nargs="?", help="input file (default: stdin)")
    ap.add_argument(
        "--text",
        action="store_true",
        help="treat input as text (XML/HTML/log), not JSON",
    )
    a = ap.parse_args()
    if a.path:
        with open(a.path, encoding="utf-8-sig") as f:
            raw = f.read()
    else:
        raw = sys.stdin.read()
    # Windows PowerShell 5.1 pipes text with a UTF-8 BOM; captures saved by
    # Notepad may carry one too. It is never part of the payload.
    raw = raw.lstrip("﻿")
    if a.text:
        out = redact_text(raw)
    else:
        try:
            data = json.loads(raw)
        except json.JSONDecodeError as e:
            print("not JSON (use --text for XML/HTML/logs): %s" % e, file=sys.stderr)
            return 2
        out = json.dumps(scrub(data), ensure_ascii=False, separators=(",", ":"))
    left = find_secrets(out)
    if left:
        kinds = sorted({k for _, k in left})
        print(
            "refusing: output still contains secret-like data: %s" % kinds,
            file=sys.stderr,
        )
        return 1
    sys.stdout.write(out.rstrip("\n") + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
