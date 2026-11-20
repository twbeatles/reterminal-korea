#!/usr/bin/env python3
"""Build + run test_native/test_core.cpp against the portable src/ on the host.

Same sources and flags as `pio test -e host_native`, usable where PlatformIO's
native platform has no compiler (e.g. Windows without MinGW).
Compiler order: $CXX, g++, clang++, `python -m ziglang c++` (pip ziglang).
Unity + ArduinoJson are taken from .pio/libdeps/*/ (fetched by PlatformIO);
pass --unity/--arduinojson to override.

Exit code: test binary's exit code (0 == all passed); 2 == toolchain/deps
missing (reported as BLOCKED_TOOLCHAIN, never as a pass).
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXCLUDE = {
    os.path.join("src", "main.cpp"),
    os.path.join("src", "net", "HttpTransport.cpp"),
}


def find_compilers():
    cxx = os.environ.get("CXX")
    if cxx:
        cc = os.environ.get("CC", cxx)
        return [cxx], [cc]
    for cxx, cc in (("g++", "gcc"), ("clang++", "clang")):
        if shutil.which(cxx) and shutil.which(cc):
            return [cxx], [cc]
    try:
        subprocess.run(
            [sys.executable, "-m", "ziglang", "version"],
            check=True,
            capture_output=True,
        )
        return (
            [sys.executable, "-m", "ziglang", "c++"],
            [sys.executable, "-m", "ziglang", "cc"],
        )
    except Exception:
        return None, None


def find_lib(name, marker):
    hits = glob.glob(os.path.join(ROOT, ".pio", "libdeps", "*", name, marker))
    return os.path.dirname(hits[0]) if hits else None


def p(path):  # forward slashes: zig on Windows rejects some MSYS paths
    return os.path.abspath(path).replace("\\", "/")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--unity")
    ap.add_argument("--arduinojson")
    a = ap.parse_args()
    cxx, cc = find_compilers()
    unity = a.unity or find_lib("Unity", os.path.join("src", "unity.c"))
    ajson = a.arduinojson or find_lib(
        "ArduinoJson", os.path.join("src", "ArduinoJson.h")
    )
    if not cxx or not unity or not ajson:
        print(
            "BLOCKED_TOOLCHAIN: compiler=%s unity=%s arduinojson=%s"
            % (bool(cxx), bool(unity), bool(ajson))
        )
        print("hint: pip install ziglang; pio pkg install -e host_native")
        return 2
    out = os.path.join(ROOT, ".pio", "native-test")
    os.makedirs(out, exist_ok=True)
    srcs = sorted(
        f
        for f in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)
        if os.path.relpath(f, ROOT) not in EXCLUDE
    )
    inc = [
        "-I" + p(os.path.join(ROOT, "include")),
        "-I" + p(os.path.join(ROOT, "src")),
        "-I" + p(unity),
        "-I" + p(ajson),
    ]
    # zig enables UBSan by default but ships no runtime for it on Windows.
    nosan = ["-fno-sanitize=all"]
    flags = ["-std=c++17", "-Wall", "-Wextra", "-O1", "-DKDASH_HOST_TEST=1"] + nosan
    unity_o = p(os.path.join(out, "unity.o"))
    exe = p(os.path.join(out, "test_core.exe" if os.name == "nt" else "test_core"))
    steps = [
        cc
        + nosan
        + [
            "-O1",
            "-c",
            p(os.path.join(unity, "unity.c")),
            "-I" + p(unity),
            "-o",
            unity_o,
        ],
        cxx
        + flags
        + inc
        + [p(os.path.join(ROOT, "test_native", "test_core.cpp"))]
        + [p(s) for s in srcs]
        + [unity_o, "-o", exe],
    ]
    for cmd in steps:
        r = subprocess.run(cmd, capture_output=True, text=True)
        diag = [
            line
            for line in (r.stdout + r.stderr).splitlines()
            if ("error" in line or "warning:" in line)
            and "ziglang" not in line
            and "unused during compilation" not in line
        ]
        if diag:
            print("\n".join(diag[:60]))
        if r.returncode != 0:
            print("BUILD FAILED (%d)" % r.returncode)
            return 1
    env = dict(os.environ, KDASH_PROJECT_DIR=ROOT)
    return subprocess.run([exe], cwd=ROOT, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
