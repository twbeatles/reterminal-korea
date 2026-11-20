#!/usr/bin/env python3
"""Official Seeed upstream drift check (docs/21, ADR 0032).

Compares the pinned OSHW-reTerminal-Series-E-D commit recorded in
hardware/board_e1001.md with the current default-branch HEAD and lists only
files that can change E1001 firmware facts (pins, PMIC, panel, build flags).
Read-only: anonymous public GitHub API, TLS verification untouched.

exit 0 = no relevant change, 1 = relevant files changed (re-audit), 2 = error.
"""

import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = "Seeed-Projects/OSHW-reTerminal-Series-E-D"
RELEVANT_PREFIXES = (
    "examples/base/",
    "examples/official/LVGLePaperStatusPanel/",
    "examples/official/SenseCraft_HMI/src/boards/reterminal_e1001/",
    "examples/official/SenseCraft_HMI/src/boards/common/",
    "examples/official/SenseCraft_HMI/lib/SY6974/",
    "examples/official/SenseCraft_HMI/src/APP/app_power_manager",
    "examples/official/SenseCraft_HMI/src/hal/",
)


def pinned_sha(doc):
    m = re.search(r"\b([0-9a-f]{40})\b", Path(doc).read_text(encoding="utf-8"))
    if not m:
        raise ValueError("pinned SHA not found in %s" % doc)
    return m.group(1)


def relevant(files):
    return [f for f in files if f.startswith(RELEVANT_PREFIXES)]


def http_get_json(url):
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "kdash-source-check",
            "Accept": "application/vnd.github+json",
        },
    )
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.load(r)


def report(pin, get=http_get_json):
    data = get("https://api.github.com/repos/%s/compare/%s...HEAD" % (REPO, pin))
    files = [f["filename"] for f in data.get("files", [])]
    hits = relevant(files)
    lines = [
        "pinned=%s status=%s ahead_by=%s changed=%d relevant=%d"
        % (pin[:8], data.get("status"), data.get("ahead_by"), len(files), len(hits))
    ]
    lines += ["  RE-AUDIT %s" % f for f in hits]
    return (1 if hits else 0), lines


def main():
    try:
        code, lines = report(pinned_sha(ROOT / "hardware" / "board_e1001.md"))
    except Exception as e:  # network/API failure is reported, never hidden
        print("ERR %s: %s" % (e.__class__.__name__, e))
        return 2
    print("\n".join(lines))
    return code


if __name__ == "__main__":
    sys.exit(main())
