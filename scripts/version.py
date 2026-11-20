#!/usr/bin/env python3
"""Firmware version stamp (P6-01/02).

Writes version.json: semver + firmware build id + source git SHA + board
model + flash layout note + build time (UTC). No secrets are collected.
Usage: python scripts/version.py --version 0.2.0 --out build/version.json
"""

import argparse
import datetime
import json
import subprocess


def git_sha() -> str:
    try:
        return subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            timeout=20,
        ).stdout.strip()
    except Exception:
        return "unknown"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--board", default="reterminal_e1001")
    a = ap.parse_args()
    parts = a.version.split(".")
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        raise SystemExit("version must be semver X.Y.Z")
    if a.board != "reterminal_e1001":
        raise SystemExit("refusing: only reterminal_e1001 is packaged (08)")
    doc = {
        "version": a.version,
        "board": a.board,
        "git_sha": git_sha(),
        "build_utc": datetime.datetime.now(datetime.timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"
        ),
        "flash_layout": "UNDECIDED-pending-32MB-verification",
        "forbidden_boards": ["e1002", "e1003", "e1004", "xiao"],
    }
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
    print("wrote %s" % a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
