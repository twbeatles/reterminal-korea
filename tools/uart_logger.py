#!/usr/bin/env python3
"""Sanitized E1001 UART capture, bounded buffering and rotation, no DTR/RTS reset.

UART lines retain prefixes for soak_report. UTC capture events go to
<log>.events.jsonl. Numbered rotation chunks are preserved, never overwritten.
"""

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "host"))
from kdash.secret_scan import find_secrets, redact_text  # noqa: E402


def sanitize_line(raw):
    text = redact_text(raw.decode("utf-8", errors="replace").rstrip("\r\n"))
    return "capture: secret-like line suppressed" if find_secrets(text) else text


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("output", type=Path)
    parser.add_argument("--duration-sec", type=int, default=86400)
    parser.add_argument("--rotate-bytes", type=int, default=5 * 1024 * 1024)
    parser.add_argument("--stop-file", type=Path)
    parser.add_argument(
        "--reset-once",
        action="store_true",
        help="explicitly pulse RTS after first open to capture boot",
    )
    parser.add_argument(
        "--soak-min-duration-sec",
        type=int,
        default=0,
        help="on completion write <log>.report.json using continuous USB gate",
    )
    args = parser.parse_args(argv)
    if (
        args.duration_sec <= 0
        or args.rotate_bytes < 4096
        or args.soak_min_duration_sec < 0
    ):
        parser.error("positive duration and rotate-bytes >=4096 required")
    import serial  # development dependency only

    args.output.parent.mkdir(parents=True, exist_ok=True)
    events = args.output.with_suffix(args.output.suffix + ".events.jsonl")
    report_path = args.output.with_suffix(args.output.suffix + ".report.json")
    if args.output.exists() or events.exists() or report_path.exists():
        parser.error("capture destination exists; choose a new session name")
    deadline = time.monotonic() + args.duration_sec
    sequence = 0
    recorded = 0
    stream = args.output.open("x", encoding="utf-8", newline="\n")
    event_file = events.open("x", encoding="utf-8", newline="\n")

    def event(kind):
        event_file.write(
            json.dumps({"utc": datetime.now(timezone.utc).isoformat(), "event": kind})
            + "\n"
        )
        event_file.flush()

    event("start")
    reset_done = False
    try:
        while time.monotonic() < deadline:
            if args.stop_file and args.stop_file.exists():
                break
            port = serial.Serial(port=None, baudrate=115200, timeout=1)
            port.dtr = False
            port.rts = False
            port.port = args.port
            try:
                port.open()
                event("connected")
                if args.reset_once and not reset_done:
                    event("intentional_reset")
                    port.rts = True
                    time.sleep(0.1)
                    port.rts = False
                    reset_done = True
                pending = bytearray()
                discarding = False
                while time.monotonic() < deadline:
                    if args.stop_file and args.stop_file.exists():
                        break
                    chunk = port.read(min(max(port.in_waiting, 1), 4096))
                    if discarding:
                        if b"\n" not in chunk:
                            continue
                        _, _, chunk = chunk.partition(b"\n")
                        discarding = False
                    pending.extend(chunk)
                    while b"\n" in pending:
                        line, _, rest = pending.partition(b"\n")
                        pending = bytearray(rest)
                        if len(line) > 4096:
                            event("oversize_line_dropped")
                            continue
                        safe = sanitize_line(line)
                        if stream.tell() >= args.rotate_bytes:
                            stream.close()
                            sequence += 1
                            path = args.output.with_name(
                                args.output.name + f".{sequence:04d}"
                            )
                            stream = path.open("x", encoding="utf-8", newline="\n")
                            event("rotate")
                        stream.write(safe + "\n")
                        stream.flush()
                        recorded += 1
                    if len(pending) > 4096:
                        pending.clear()
                        discarding = True  # never persist an orphaned secret tail
                        event("oversize_unterminated_line_dropped")
            except serial.SerialException:
                event("serial_disconnected")
                time.sleep(min(2, max(0, deadline - time.monotonic())))
            finally:
                port.close()
    except KeyboardInterrupt:
        event("interrupted")
    finally:
        event("stop")
        stream.close()
        event_file.close()
    if args.soak_min_duration_sec:
        # Import as a sibling script without requiring tools to be installed.
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from soak_report import analyze_lines

        def captured_lines():
            paths = [args.output] + [
                args.output.with_name(args.output.name + f".{i:04d}")
                for i in range(1, sequence + 1)
            ]
            for path in paths:
                with path.open(encoding="utf-8") as source:
                    yield from source

        with events.open(encoding="utf-8") as metadata:
            report = analyze_lines(
                captured_lines(), args.soak_min_duration_sec, capture_events=metadata
            )
        with report_path.open("x", encoding="utf-8", newline="\n") as destination:
            json.dump(report, destination, ensure_ascii=False, indent=2)
        return 1 if report["failures"] else 0
    return 0 if recorded else 1  # completion is not a soak PASS


if __name__ == "__main__":
    raise SystemExit(main())
