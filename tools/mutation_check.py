#!/usr/bin/env python3
"""Minimal mutation gate (docs/07 Mutation): every listed mutant must be
killed, i.e. the focused test file must FAIL on the mutated tree.

Mutations run on a throwaway copy under the system temp dir; the working
tree is never modified. Exit 0 only when all mutants are killed.
Exit 1 when a mutant survives, 2 on gate infrastructure failure.
"""

import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

MUTANTS = [
    {
        "id": "airkorea-missing-to-zero",
        "rel": os.path.join("host", "kdash", "airkorea.py"),
        "old": '        return None  # "-", "", '
        "점검/통신장애 문구, 음수 => Missing (AIR-01)",
        "new": "        return 0  # MUTANT: Missing->0 (must be killed)",
        "tests": ["tests/test_airkorea.py"],
    },
    {
        "id": "freshness-sign-flip",
        "rel": os.path.join("host", "kdash", "models.py"),
        "old": "    return Validity.Valid if now_utc - observed_utc "
        "<= ttl_sec else Validity.Stale",
        "new": "    return Validity.Valid if now_utc - observed_utc "
        "> ttl_sec else Validity.Stale",
        "tests": ["tests/test_models.py"],
    },
]


def run_mutant(work_host, mutant):
    target = os.path.join(
        work_host,
        os.path.relpath(os.path.join(ROOT, mutant["rel"]), os.path.join(ROOT, "host")),
    )
    with open(target, encoding="utf-8") as fh:
        text = fh.read()
    if text.count(mutant["old"]) != 1:
        return "ERROR-anchor-count-%d" % text.count(mutant["old"])
    with open(target, "w", encoding="utf-8") as fh:
        fh.write(text.replace(mutant["old"], mutant["new"]))
    proc = subprocess.run(
        [sys.executable, "-m", "pytest"]
        + mutant["tests"]
        + ["-q", "--tb=no", "-p", "no:cacheprovider"],
        capture_output=True,
        text=True,
        cwd=work_host,
    )
    if proc.returncode == 0:
        return "SURVIVED"
    if proc.returncode == 1:
        return "KILLED"
    return "ERROR-exit-%d: %s" % (proc.returncode, proc.stderr[-500:])


def main():
    work = tempfile.mkdtemp(prefix="kdash-mutation-")
    results = {}
    try:
        work_host = os.path.join(work, "host")
        shutil.copytree(
            os.path.join(ROOT, "host"),
            work_host,
            ignore=shutil.ignore_patterns("__pycache__", ".pytest_cache"),
        )
        failed = False
        for mutant in MUTANTS:
            # Fresh copy per mutant so kills cannot leak into the next run.
            shutil.rmtree(work_host)
            shutil.copytree(
                os.path.join(ROOT, "host"),
                work_host,
                ignore=shutil.ignore_patterns("__pycache__", ".pytest_cache"),
            )
            verdict = run_mutant(work_host, mutant)
            results[mutant["id"]] = verdict
            print("mutation %s: %s" % (mutant["id"], verdict))
            if verdict != "KILLED":
                failed = True
    finally:
        shutil.rmtree(work, ignore_errors=True)
    if failed:
        print("MUTATION GATE FAILED: %s" % results)
        return 1
    print(
        "MUTATION GATE PASSED: %s"
        % ", ".join("%s=%s" % kv for kv in sorted(results.items()))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
