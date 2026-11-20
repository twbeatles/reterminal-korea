#!/usr/bin/env python3
"""Build + run tools/render_previews.cpp, convert PBM -> PNG (T04c evidence).

Outputs: .pio/preview/*.pbm (C++ render truth) and evidence/p1/host-preview/*.png.
"""

import argparse
import glob
import os
import subprocess
import sys

from run_native_tests import find_compilers

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXCLUDE = {"src/main.cpp", "src/net/HttpTransport.cpp"}


def p(path):
    return os.path.abspath(path).replace("\\", "/")


def find_lib(name, marker):
    hits = glob.glob(os.path.join(ROOT, ".pio", "libdeps", "*", name, marker))
    return os.path.dirname(hits[0]) if hits else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--output-dir", default=os.path.join(ROOT, "evidence", "p1", "host-preview")
    )
    args = ap.parse_args()
    unity = find_lib("Unity", os.path.join("src", "unity.c"))
    ajson = find_lib("ArduinoJson", os.path.join("src", "ArduinoJson.h"))
    cxx, _ = find_compilers()
    if not cxx or not unity or not ajson:
        print("BLOCKED_TOOLCHAIN: preview compiler/dependencies missing")
        return 2
    srcs = sorted(
        f
        for f in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)
        if os.path.relpath(f, ROOT).replace(os.sep, "/") not in EXCLUDE
    )
    inc = [
        "-I" + p(os.path.join(ROOT, "include")),
        "-I" + p(os.path.join(ROOT, "src")),
        "-I" + p(unity),
        "-I" + p(ajson),
    ]
    flags = ["-std=c++17", "-O1", "-DKDASH_HOST_TEST=1", "-fno-sanitize=all"]
    outdir = os.path.join(ROOT, ".pio", "preview")
    os.makedirs(outdir, exist_ok=True)
    exe = p(
        os.path.join(
            outdir, "render_previews.exe" if os.name == "nt" else "render_previews"
        )
    )
    cmd = (
        cxx
        + flags
        + inc
        + [p(os.path.join(ROOT, "tools", "render_previews.cpp"))]
        + [p(s) for s in srcs]
        + ["-o", exe]
    )
    r = subprocess.run(cmd, capture_output=True, text=True)
    err = [line for line in (r.stdout + r.stderr).splitlines() if "error" in line]
    if r.returncode != 0:
        print("\n".join(err[:20]))
        return 1
    r = subprocess.run([exe], capture_output=True, text=True, cwd=ROOT)
    print(r.stdout[-1500:])
    if r.returncode != 0:
        return r.returncode
    # PBM -> PNG for human review.
    try:
        from PIL import Image
    except ImportError:
        print("Pillow missing: PBM only.")
        return 0
    dest = args.output_dir
    os.makedirs(dest, exist_ok=True)
    for pbm in sorted(glob.glob(os.path.join(outdir, "*.pbm"))):
        pgm = pbm[:-4] + ".pgm"
        img = Image.open(pgm if os.path.exists(pgm) else pbm)
        img.save(os.path.join(dest, os.path.basename(pbm)[:-4] + ".png"))
        print("png", os.path.basename(pbm))
    return 0


if __name__ == "__main__":
    sys.exit(main())
