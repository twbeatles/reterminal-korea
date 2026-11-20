#!/usr/bin/env python3
"""Build and package E1001 recovery artifacts, failing closed on invalid inputs.

Only a clean committed checkout without local dev_credentials.h may release.
The actual firmware identity (retained by the boot log), PlatformIO settings,
all recovery images, known environment credentials and secret patterns are
checked before dist is created. Scanning cannot prove arbitrary binary secrecy.
This command never flashes a device. OTA/32MB partition selection stays deferred.
"""

from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "host"))
sys.path.insert(0, str(ROOT / "scripts"))
from kdash.secret_scan import find_secrets  # noqa: E402
from private_config import known_private_values  # noqa: E402

ARTIFACTS = ("firmware.bin", "bootloader.bin", "partitions.bin")
IDENTITY = re.compile(rb"KDASH_BUILD_ID\|([a-z0-9_]+)\|([0-9]+\.[0-9]+\.[0-9]+)\|END")
VERSION = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)")
KEY_ENV = (
    "KDASH_KMA_KEY",
    "KDASH_AIR_KEY",
    "KDASH_EXIM_KEY",
    "KDASH_KIS_APP_KEY",
    "KDASH_KIS_APP_SECRET",
    "KDASH_WIFI_SECRET",
    "KDASH_HOLIDAY_KEY",
)
# mbedTLS embeds these exact NUL-terminated PEM delimiter constants so it can
# parse keys supplied at runtime. A full PEM string has a newline/body after
# BEGIN, not an immediate NUL, and must still be rejected below.
PEM_PARSER_CONSTANT = re.compile(
    rb"(^|\x00)-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----(?=\x00)"
)


class ReleaseError(ValueError):
    pass


def public_build_preflight(root, environ=None):
    env = os.environ if environ is None else environ
    for name in (
        "PLATFORMIO_BUILD_FLAGS",
        "PLATFORMIO_BUILD_SRC_FLAGS",
        "PLATFORMIO_EXTRA_SCRIPTS",
    ):
        if env.get(name):
            raise ReleaseError(
                "build overrides are forbidden in public release packaging"
            )
    cfg = configparser.ConfigParser(interpolation=None)
    if not cfg.read(Path(root) / "platformio.ini", encoding="utf-8"):
        raise ReleaseError("PlatformIO configuration missing")
    for name in ("env", "env:reterminal_e1001"):
        if name not in cfg:
            continue
        section = cfg[name]
        flags = section.get("build_flags", "") + section.get("build_src_flags", "")
        if (
            "KDASH_ALLOW_DEV_CREDENTIALS" in flags
            or "KDASH_DEV_" in flags
            or section.get("extra_scripts", "").strip()
            or section.get("extends", "").strip()
        ):
            raise ReleaseError(
                "public build configuration contains private/development hooks"
            )


def run(args, root):
    return subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=300)


def git_revision(root, runner=run):
    status = runner(["git", "status", "--porcelain"], root)
    if status.returncode:
        raise ReleaseError("git status failed")
    if status.stdout.strip():
        raise ReleaseError("dirty git tree")
    rev = runner(["git", "rev-parse", "--verify", "HEAD"], root)
    if rev.returncode or not re.fullmatch(r"[0-9a-fA-F]{40,64}", rev.stdout.strip()):
        raise ReleaseError("a committed source revision is required")
    return rev.stdout.strip()


def inspect_images(root, version, environ=None):
    public_build_preflight(root, environ)
    if not VERSION.fullmatch(version):
        raise ReleaseError("version must be X.Y.Z")
    if (root / "include" / "dev_credentials.h").exists():
        raise ReleaseError(
            "local development credentials must be absent for release builds"
        )
    cfg = configparser.ConfigParser(interpolation=None)
    if not cfg.read(root / "platformio.ini", encoding="utf-8"):
        raise ReleaseError("PlatformIO configuration missing")
    section = cfg["env:reterminal_e1001"]
    flags = section.get("build_flags", "")
    if (
        section.get("board") != "seeed_xiao_esp32s3"
        or section.get("board_build.arduino.memory_type") != "qio_opi"
        or not re.search(r"(?:^|\s)-D\s*RETERMINAL_E1001(?:\s|$)", flags)
    ):
        raise ReleaseError("board configuration is not E1001")
    # PlatformIO uses escaped quotes in build_flags.
    declared = re.search(r"KDASH_FW_VERSION\s*=\s*\\?\"([^\"\\]+)\\?\"", flags)
    if not declared or declared.group(1) != version:
        raise ReleaseError("requested version differs from build configuration")
    bdir = root / ".pio" / "build" / "reterminal_e1001"
    images = {}
    for name in ARTIFACTS:
        path = bdir / name
        if not path.is_file() or path.stat().st_size == 0:
            raise ReleaseError("required recovery artifact missing: " + name)
        images[name] = path.read_bytes()
    ids = set(IDENTITY.findall(images["firmware.bin"]))
    if ids != {(b"reterminal_e1001", version.encode("ascii"))}:
        raise ReleaseError("firmware identity is missing or differs from board/version")
    env = os.environ if environ is None else environ
    known = [
        str(env[k]).encode("utf-8") for k in KEY_ENV if len(str(env.get(k, ""))) >= 8
    ]
    known.extend(known_private_values(root, env))
    for name, blob in images.items():
        if (
            b"KDASH_TEST_SECRET_MARKER" in blob
            or b"KDASH_PRIVATE_BUILD|END" in blob
            or any(value in blob for value in known)
        ):
            raise ReleaseError("credential marker detected in " + name)
        # Extract printable runs rather than dumping image text to stdout/logs.
        scan_blob = PEM_PARSER_CONSTANT.sub(lambda match: match.group(1), blob)
        strings = b"\n".join(re.findall(rb"[\x20-\x7e]{8,}", scan_blob)).decode("ascii")
        if find_secrets(strings):
            raise ReleaseError("secret-like content detected in " + name)
    return images


def package(root, version, build=True, runner=run, environ=None):
    root = Path(root)
    if not VERSION.fullmatch(version):
        raise ReleaseError("version must be X.Y.Z")
    public_build_preflight(root, environ)
    sha = git_revision(root, runner)
    # Reject credential-bearing builds before starting a compiler.
    if (root / "include" / "dev_credentials.h").exists():
        raise ReleaseError(
            "local development credentials must be absent for release builds"
        )
    if build:
        result = runner(["pio", "run", "-e", "reterminal_e1001"], root)
        if result.returncode:
            raise ReleaseError("firmware build failed (compiler output withheld)")
        if git_revision(root, runner) != sha:
            raise ReleaseError("source revision changed during build")
    images = inspect_images(root, version, environ)
    target = root / "dist" / ("e1001-" + version)
    if target.exists():
        raise ReleaseError("release directory already exists")
    target.parent.mkdir(exist_ok=True)
    # Stage under the same parent; no partially populated release is visible.
    with tempfile.TemporaryDirectory(prefix=".release-", dir=target.parent) as staging:
        stage = Path(staging)
        hashes = {}
        for name, blob in images.items():
            (stage / name).write_bytes(blob)
            hashes[name] = hashlib.sha256(blob).hexdigest()
        meta = {
            "version": version,
            "board": "reterminal_e1001",
            "git_sha": sha,
            "sha256": hashes,
            "hardware_verified": False,
            "flash_layout": "board-default-32MB-verification-pending",
            "security_scan": "patterns-and-known-environment-values; not a proof of secrecy",
        }
        (stage / "version.json").write_text(
            json.dumps(meta, indent=2) + "\n", encoding="utf-8"
        )
        (stage / "SHA256SUMS").write_text(
            "".join(v + "  " + k + "\n" for k, v in hashes.items()), encoding="ascii"
        )
        (stage / "FLASHING.md").write_text(
            "# E1001 USB recovery artifacts\n\n"
            "This package is not hardware verified. Confirm the physical E1001 and flash size first.\n"
            "No erase or flash command was run by the packager. OTA is deferred.\n\n"
            "Flash offsets and any additional OTA bootstrap image must come from the verified "
            "PlatformIO upload recipe for this exact source revision. This package is an artifact "
            "bundle, not a standalone flash command. Use `pio run -e reterminal_e1001 -t upload` "
            "only after the user authorizes a device write; follow docs/RECOVERY.md. "
            "Do not guess offsets or treat the three files alone as a verified recovery procedure.\n",
            encoding="utf-8",
        )
        # TemporaryDirectory cleanup accepts the now-empty original path.
        stage.rename(target)
    return target


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--version", required=True)
    ap.add_argument(
        "--build",
        action="store_true",
        help="compatibility flag; build is already the default",
    )
    args = ap.parse_args(argv)
    try:
        target = package(ROOT, args.version)
    except ReleaseError as exc:
        # ReleaseError messages are fixed diagnostics defined in this module.
        print("REFUSED: " + str(exc), file=sys.stderr)
        return 1
    except (OSError, KeyError, configparser.Error, subprocess.SubprocessError):
        # Exception text from libraries can contain credentials or paths; never echo it.
        print(
            "REFUSED: release preflight/build/artifact checks failed", file=sys.stderr
        )
        return 1
    print("packaged " + target.name + " (hardware verification pending)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
