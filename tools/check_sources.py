#!/usr/bin/env python3
"""Source freshness check (F-09): HEAD each URL in docs/10 + ADR 0003.

Reports status code / Last-Modified changes without bypassing TLS
verification. koreaexim TLS failures are reported as-is.
"""

import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def urls_in(path):
    urls = []
    for m in re.finditer(
        r"https?://[^\s)\"'<>]+", path.read_text(encoding="utf-8", errors="replace")
    ):
        urls.append(m.group(0).rstrip(".,;"))
    return sorted(set(urls))


def check(url):
    try:
        req = urllib.request.Request(url, method="HEAD")
        with urllib.request.urlopen(req, timeout=15) as r:
            return "%s %s" % (r.status, r.headers.get("Last-Modified", "-"))
    except Exception as e:
        return "ERR %s" % (e.__class__.__name__,)


def main():
    targets = [
        ROOT / "docs" / "10_SOURCES_AND_OPEN_QUESTIONS.md",
        ROOT / "docs" / "adr" / "0003-live-api-integration.md",
    ]
    n_err = 0
    for t in targets:
        if not t.exists():
            print("missing %s" % t)
            continue
        print("== %s" % t.name)
        for u in urls_in(t):
            res = check(u)
            print("  %-70s %s" % (u[:70], res))
            if res.startswith("ERR"):
                n_err += 1
    print("errors=%d" % n_err)
    return 0


if __name__ == "__main__":
    sys.exit(main())
