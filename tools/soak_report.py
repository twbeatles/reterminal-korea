#!/usr/bin/env python3
"""Soak report (T11/L5): sanitized UART log -> heap/outcome/cycle summary.

Reads the firmware's own log lines (no secrets; see AGENTS.md):
  kdash boot fw=<ver> board=e1001 reset=<n> heap=<bytes> psram=<bytes> ...
  fetch: provider=<n> outcome=<STR> http=<n> requests=<n>

Pass criteria (fail-closed for vacuous input):
  - at least one fetch: line parsed, otherwise the log proves nothing;
  - no panic / task-watchdog / Guru Meditation tokens;
  - default PASS means log sanity only, not a completed hardware soak;
  - --min-duration-sec requires monotonic health uptime, USB power, no
    reboots, and no health gaps over --max-gap-sec (default 120);
  - boot heap and periodic INTERNAL heap/PSRAM are reported separately for
    human review, not gated: leak thresholds need on-device calibration.

Exit code: 0 == pass, 1 == fail, 2 == usage/file error.
"""

import argparse
import json
import re
import sys

FETCH_RE = re.compile(
    r"^fetch: provider=(\d+) outcome=([A-Z_]+) http=(-?\d+) requests=(\d+)\s*$"
)
HEAP_RE = re.compile(r"\bheap=(\d+)\b")
HEALTH_RE = re.compile(
    r"^health: uptime_ms=(\d+) heap=(\d+) min_heap=(\d+) largest=(\d+) "
    r"psram=(\d+) usb=([01]) wifi=([01])$"
)
PANIC_RES = [
    re.compile(r"Guru Meditation", re.IGNORECASE),
    re.compile(r"Task watchdog", re.IGNORECASE),
    re.compile(r"stack smashing", re.IGNORECASE),
    re.compile(r"heap_caps_.+failed", re.IGNORECASE),
    re.compile(r"LoadProhibited", re.IGNORECASE),
]
DEVICE_FAULT_RE = re.compile(
    r"^(?:checkpoint: persistence FAILED|watchdog: subscription FAILED|"
    r"display: allocation FAILED|display: page=.*\bok=0(?:\s|$)|"
    r"charge: enabled=1 .*\b(?:state=[25]|readback=0)\b)"
)
CAPTURE_LOSS_EVENTS = {
    "serial_disconnected",
    "oversize_line_dropped",
    "oversize_unterminated_line_dropped",
    "interrupted",
}
CAPTURE_EVENTS = CAPTURE_LOSS_EVENTS | {
    "start",
    "stop",
    "connected",
    "rotate",
    "intentional_reset",
}


def capture_summary(events):
    counts = {}
    malformed = False
    first = last = None
    for raw in events:
        try:
            record = json.loads(raw) if isinstance(raw, str) else raw
            kind = record["event"]
            if not isinstance(kind, str) or kind not in CAPTURE_EVENTS:
                raise ValueError()
        except (ValueError, TypeError, KeyError):
            malformed = True
            continue
        counts[kind] = counts.get(kind, 0) + 1
        if first is None:
            first = kind
        last = kind
    failures = []
    if malformed:
        failures.append("malformed capture metadata")
    if (
        counts.get("start") != 1
        or counts.get("stop") != 1
        or counts.get("connected") != 1
        or first != "start"
        or last != "stop"
    ):
        failures.append("capture metadata does not describe one completed session")
    if counts.get("intentional_reset", 0) > 1:
        failures.append("multiple intentional capture resets")
    if any(counts.get(kind) for kind in CAPTURE_LOSS_EVENTS):
        failures.append("capture interruption or discarded data")
    return counts, failures


def analyze_lines(lines, min_duration_sec=0, max_gap_sec=120, capture_events=None):
    fetches = 0
    outcomes = {}
    requests = 0
    heaps = []
    panics = []
    health = []
    boots = 0
    health_boots = []
    malformed_health = 0
    device_fault_count = 0
    device_faults = []
    panic_count = 0
    for no, raw in enumerate(lines, 1):
        line = raw.strip()
        if DEVICE_FAULT_RE.search(line):
            device_fault_count += 1
            if len(device_faults) < 50:
                device_faults.append({"line": no, "text": line[:160]})
        if line.startswith("kdash boot "):
            boots += 1
        hm = HEALTH_RE.fullmatch(line)
        if hm:
            health.append(
                dict(
                    zip(
                        (
                            "uptime_ms",
                            "heap",
                            "min_heap",
                            "largest",
                            "psram",
                            "usb",
                            "wifi",
                        ),
                        map(int, hm.groups()),
                    )
                )
            )
            health_boots.append(boots)
        elif line.startswith("health:"):
            malformed_health += 1
        m = FETCH_RE.match(line)
        if m:
            fetches += 1
            outcomes[m.group(2)] = outcomes.get(m.group(2), 0) + 1
            requests += int(m.group(4))
            continue
        hm = HEAP_RE.search(line) if line.startswith("kdash boot ") else None
        if hm:
            heaps.append(int(hm.group(1)))
        for pr in PANIC_RES:
            if pr.search(line):
                panic_count += 1
                if len(panics) < 50:
                    panics.append({"line": no, "text": line[:160]})
                break
    heap_trend = None
    if heaps:
        heap_trend = {
            "first": heaps[0],
            "last": heaps[-1],
            "min": min(heaps),
            "samples": len(heaps),
        }
    failures = []
    if fetches == 0:
        failures.append("no fetch: lines parsed (vacuous log)")
    if panic_count:
        failures.append("%d panic/watchdog token(s)" % panic_count)
    capture_counts = None
    if capture_events is not None:
        capture_counts, capture_failures = capture_summary(capture_events)
        failures.extend(capture_failures)
    # This gate proves continuous USB observation only. Memory trends and
    # functional recovery still require review; boot heap is a separate scope.
    continuous = None
    if health:
        gaps = [b["uptime_ms"] - a["uptime_ms"] for a, b in zip(health, health[1:])]
        continuous = {
            "samples": len(health),
            "duration_sec": (health[-1]["uptime_ms"] - health[0]["uptime_ms"]) / 1000,
            "max_gap_sec": max(gaps, default=0) / 1000,
            "internal_heap_first": health[0]["heap"],
            "internal_heap_last": health[-1]["heap"],
            "internal_heap_min": min(h["heap"] for h in health),
            "internal_min_heap": min(h["min_heap"] for h in health),
            "internal_largest_min": min(h["largest"] for h in health),
            "psram_first": health[0]["psram"],
            "psram_last": health[-1]["psram"],
            "psram_min": min(h["psram"] for h in health),
        }
    if min_duration_sec:
        if device_fault_count:
            failures.append(
                "%d device fault(s) during USB observation" % device_fault_count
            )
        if malformed_health:
            failures.append("malformed health sample(s)")
        if len(health) < 2:
            failures.append("continuous USB gate needs at least two health samples")
        else:
            if any(
                b["uptime_ms"] <= a["uptime_ms"] for a, b in zip(health, health[1:])
            ):
                failures.append("health uptime reset or duplicate")
            if boots > 1 or len(set(health_boots)) > 1:
                failures.append("reboot in continuous USB observation")
            if any(not h["usb"] for h in health):
                failures.append("non-USB health sample")
            if continuous["duration_sec"] < min_duration_sec:
                failures.append("insufficient continuous observation duration")
            if continuous["max_gap_sec"] > max_gap_sec:
                failures.append("health observation gap exceeds limit")
    return {
        "fetches": fetches,
        "outcomes": outcomes,
        "requests_total": requests,
        "heap_trend": heap_trend,
        "boot_count": boots,
        "health": continuous,
        "scope": "continuous_usb_observation"
        if min_duration_sec
        else "log_sanity_only",
        "capture_metadata_checked": capture_events is not None,
        "capture_event_counts": capture_counts,
        "device_fault_count": device_fault_count,
        "device_faults": device_faults,
        "panic_count": panic_count,
        "panic_hits": panics,
        "failures": failures,
        "verdict": "FAIL" if failures else "PASS",
    }


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", help="sanitized UART log")
    parser.add_argument(
        "--min-duration-sec",
        type=int,
        default=0,
        help="require continuous USB health coverage (e.g. 86400)",
    )
    parser.add_argument("--max-gap-sec", type=int, default=120)
    parser.add_argument(
        "--events", help="capture .events.jsonl; rejects interruption/data loss"
    )
    args = parser.parse_args(argv[1:])
    if args.min_duration_sec < 0 or args.max_gap_sec <= 0:
        parser.error("duration must be nonnegative and gap must be positive")
    try:
        with open(args.log, encoding="utf-8") as fh:
            if args.events:
                with open(args.events, encoding="utf-8") as metadata:
                    report = analyze_lines(
                        fh, args.min_duration_sec, args.max_gap_sec, metadata
                    )
            else:
                report = analyze_lines(fh, args.min_duration_sec, args.max_gap_sec)
    except OSError as exc:
        print("soak_report: cannot read %s: %s" % (args.log, exc))
        return 2
    print(json.dumps(report, indent=2, sort_keys=True))
    return 1 if report["failures"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
