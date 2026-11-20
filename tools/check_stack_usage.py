#!/usr/bin/env python3
"""Check compiled Xtensa task frames; never equate this gate to hardware proof.

pio run -e reterminal_e1001
python tools/check_stack_usage.py

Uses the actual release ELF. It rejects oversized application frames and
known boot/persistence call chains exceeding the configured loopTask budget.
Indirect calls and dynamic stack use still require hardware high-water marks.
"""

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def find_objdump():
    override = os.environ.get("XTENSA_OBJDUMP")
    if override:
        return override
    exe = shutil.which("xtensa-esp32s3-elf-objdump")
    if exe:
        return exe
    home = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
    found = sorted(
        glob.glob(
            str(
                home
                / "packages"
                / "toolchain-xtensa*"
                / "bin"
                / "xtensa-esp32s3-elf-objdump*"
            )
        )
    )
    return found[0] if found else None


def frames(disassembly):
    result = {}
    name = None
    for line in disassembly.splitlines():
        symbol = re.search(r"^[0-9a-fA-F]+ <(.+)>:$", line.strip())
        if symbol:
            name = symbol.group(1)
        elif name:
            entry = re.search(r"\bentry\s+a1,\s*(0x[0-9a-fA-F]+|[0-9]+)", line)
            if entry:
                result[name] = int(entry.group(1), 0)
                name = None
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--elf", type=Path, default=ROOT / ".pio/build/reterminal_e1001/firmware.elf"
    )
    ap.add_argument("--max-frame", type=int, default=3072)
    ap.add_argument("--stack-bytes", type=int, default=8192)
    ap.add_argument(
        "--reserve", type=int, default=2048, help="reserve for SDK/indirect callees"
    )
    ap.add_argument("--json", type=Path, help="optional evidence report")
    args = ap.parse_args()
    objdump = find_objdump()
    if not objdump or not args.elf.exists():
        print("BLOCKED_TOOLCHAIN: missing Xtensa objdump or firmware ELF")
        return 2
    p = subprocess.run(
        [objdump, "-d", "-C", str(args.elf)], capture_output=True, text=True
    )
    if p.returncode:
        print("objdump failed")
        return 2
    all_frames = frames(p.stdout)
    app = {
        name: size
        for name, size in all_frames.items()
        if name.startswith("kdash::")
        or name.startswith("(anonymous namespace)::")
        or name in ("setup()", "loop()")
    }
    failures = [
        "%s frame=%d > %d" % (name, size, args.max_frame)
        for name, size in app.items()
        if size > args.max_frame
    ]
    if not app or "setup()" not in app:
        failures.append(
            "application/setup stack frames not found; cannot verify this ELF"
        )
    # Named lower-bound chains cover the audit's regression. Members absent
    # after refactoring/inlining contribute through their caller's frame.
    chains = {
        "boot_settings_read": [
            "setup()",
            "::LoadSettings(",
            "::LoadSettingsFromNvs(",
            "kdash::slot::Read(",
            "::ReadSlot(",
            "kdash::NvsStorage::read(",
        ],
        "runtime_restore": [
            "setup()",
            "::RestoreRuntime(",
            "kdash::RuntimeStore::Restore(",
            "kdash::slot::Read(",
            "::ReadSlot(",
            "kdash::NvsStorage::read(",
        ],
        "runtime_checkpoint": [
            "setup()",
            "::RunCycle(",
            "EspCyclePlatform::Checkpoint(",
            "kdash::RuntimeStore::Checkpoint(",
            "kdash::slot::Write(",
            "::ReadSlot(",
            "kdash::NvsStorage::read(",
        ],
        "render": [
            "setup()",
            "::RunCycle(",
            "EspCyclePlatform::Render(",
            "::RenderSnapshot(",
            "kdash::vm::BuildLife(",
            "kdash::ui::RenderLife(",
        ],
        "portal_headers": [
            "setup()",
            "::RunCycle(",
            "::OpenPortal(",
            "kdash::portal::RunPortal(",
            "::BoundedServer::Poll(",
        ],
    }
    totals = {}
    for label, pieces in chains.items():
        names = {
            name for piece in pieces for name in app if name == piece or piece in name
        }
        total = sum(app[name] for name in names)
        totals[label] = {
            "frames": total,
            "with_reserve": total + args.reserve,
            "symbols": sorted(names),
        }
        if total + args.reserve > args.stack_bytes:
            failures.append(
                "%s frames+reserve=%d > %d"
                % (label, total + args.reserve, args.stack_bytes)
            )
    report = {
        "elf": str(args.elf),
        "application_frame_count": len(app),
        "max_application_frame": max(app.values(), default=0),
        "stack_bytes": args.stack_bytes,
        "reserve": args.reserve,
        "chains": totals,
        "failures": failures,
        "limitations": "Known static chains only; indirect calls/SDK/dynamic use need hardware measurement.",
    }
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
