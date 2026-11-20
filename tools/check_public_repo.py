"""Read-only Git source/history audit. Reports locations/kinds, never values."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
sys.path.insert(0, str(ROOT / "scripts"))
from kdash.secret_scan import find_secrets  # noqa: E402
from private_config import known_private_values  # noqa: E402

# Reviewed historical synthetic detector inputs only. A content change changes
# the hash and loses the exception. Exact matches to local private values are
# NEVER exempted, including in these objects.
SYNTHETIC_OBJECTS = {
    "182bd34fad1467a9686cf05e5be7d904b2b4fb7a": "host/kdash/secret_scan.py",
    "a8209c4a0d96432f7fe5c21693960e8a756eeeeb": "host/tests/test_secret_scan.py",
    # Re-reviewed after the ruff format commit: only fake detector inputs.
    "a0b3d53faae90c0d4288f2a077cb12760fc209e8": "host/tests/test_secret_scan.py",
}


def private_path(path):
    p = Path(path)
    return (
        p.name in {"dev_credentials.h", "kdash_private_credentials.h", ".env"}
        or p.name.startswith(("secrets.", ".env."))
        or (p.parts and p.parts[0] in {"private", "secrets"})
        or (p.parts and p.parts[0] == "evidence" and p.suffix != ".md")
    )


def scan_blob(data, known=()):
    # A raw value in compiled/binary content is also a leak; do not decode first.
    hits = [{"kind": "known_private_value"}] if any(v in data for v in known) else []
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        text = data.decode("utf-16", errors="replace")
    elif b"\0" not in data:
        text = data.decode("utf-8-sig", errors="replace")
    else:
        return hits
    if any(v.decode("utf-8", errors="replace") in text for v in known):
        if not hits:
            hits.append({"kind": "known_private_value"})
    hits += [{"line": no, "kind": kind} for no, kind in find_secrets(text)]
    return hits


def audit(root=ROOT, history=False, environ=None):
    root = Path(root)
    known = known_private_values(root, environ)

    def git(*args):
        return subprocess.check_output(
            ["git", *args], cwd=root, stderr=subprocess.DEVNULL
        )

    objects = {}
    blocked_paths = []
    for record in git("ls-files", "--stage", "-z").split(b"\0"):
        if not record:
            continue
        meta, path = record.split(b"\t", 1)
        mode, oid, stage = meta.decode().split()
        if stage != "0":
            raise ValueError("unmerged index cannot be published")
        name = path.decode("utf-8", errors="replace")
        if private_path(name):
            blocked_paths.append(name)
        objects.setdefault(oid, name)
    if history:
        for line in (
            git("rev-list", "--objects", "--all")
            .decode("utf-8", errors="replace")
            .splitlines()
        ):
            oid, _, path = line.partition(" ")
            objects.setdefault(oid, path)
    findings = []
    scanned = 0
    synthetic_reviewed = 0
    batch = subprocess.Popen(
        ["git", "cat-file", "--batch"],
        cwd=root,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    try:
        for oid, path in objects.items():
            batch.stdin.write((oid + "\n").encode())
            batch.stdin.flush()
            header = batch.stdout.readline().decode().split()
            if len(header) != 3:
                raise ValueError("Git object read failed")
            size = int(header[2])
            if size > 32 * 1024 * 1024:
                raise ValueError("oversize Git object requires separate review")
            data = batch.stdout.read(size)
            if len(data) != size or batch.stdout.read(1) != b"\n":
                raise ValueError("incomplete Git object")
            if header[1] != "blob":
                continue
            scanned += 1
            hits = scan_blob(data, known)
            if SYNTHETIC_OBJECTS.get(oid) == path:
                synthetic_reviewed += 1
                hits = [hit for hit in hits if hit["kind"] == "known_private_value"]
            if hits:
                findings.append({"object": oid, "path": path, "hits": hits[:50]})
    finally:
        batch.stdin.close()
        batch.stdout.close()
        batch.wait()
    author_count = None
    if history:
        emails = set(git("log", "--all", "--format=%ae%n%ce").decode().splitlines())
        author_count = sum(
            not email.endswith("@users.noreply.github.com") for email in emails
        )
    return {
        "scope": "index_and_reachable_history" if history else "index",
        "blobs_scanned": scanned,
        "known_private_values_checked": bool(known),
        "findings": findings,
        "blocked_index_paths": blocked_paths,
        "reviewed_synthetic_objects": synthetic_reviewed,
        "source_scan_pass": not findings and not blocked_paths,
        "non_noreply_author_email_count": author_count,
        "author_metadata_requires_review": bool(author_count),
        "limitations": "Pattern/exact known-value scan; images and unknown secrets need human review. History is not rewritten.",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = audit(history=args.history)
    except (OSError, ValueError, SyntaxError, subprocess.SubprocessError):
        print(
            "public audit failed; details withheld to avoid private-value disclosure",
            file=sys.stderr,
        )
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return (
        1
        if not result["source_scan_pass"] or result["author_metadata_requires_review"]
        else 0
    )


if __name__ == "__main__":
    raise SystemExit(main())
