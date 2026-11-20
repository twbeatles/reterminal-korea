"""Soak-report gate (L5 fixture level): parser accepts a healthy sanitized
log and rejects panic / vacuous logs. Real device logs plug in unmodified."""

import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = None  # resolved via test file location
SCRIPT = "tools/soak_report.py"
OK_LOG = "test/fixtures/soak_ok.log"
PANIC_LOG = "test/fixtures/soak_panic.log"


def _run(log_rel):
    import os

    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    proc = subprocess.run(
        [sys.executable, os.path.join(root, SCRIPT), os.path.join(root, log_rel)],
        capture_output=True,
        text=True,
        cwd=root,
    )
    return proc


def test_soak_ok_log_passes_with_summary():
    proc = _run(OK_LOG)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    report = json.loads(proc.stdout)
    assert report["verdict"] == "PASS"
    assert report["fetches"] == 8
    assert report["outcomes"]["OK"] == 6
    assert report["heap_trend"]["samples"] == 2
    assert report["failures"] == []


def test_soak_panic_log_fails():
    proc = _run(PANIC_LOG)
    assert proc.returncode == 1, proc.stdout + proc.stderr
    report = json.loads(proc.stdout)
    assert report["verdict"] == "FAIL"
    assert len(report["panic_hits"]) == 1


def test_soak_vacuous_log_fails():
    import os
    import tempfile

    with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as fh:
        fh.write("kdash boot fw=0.3.0 board=e1001 reset=1 heap=1 psram=2\n")
        name = fh.name
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    proc = subprocess.run(
        [sys.executable, os.path.join(root, SCRIPT), name],
        capture_output=True,
        text=True,
        cwd=root,
    )
    os.unlink(name)
    assert proc.returncode == 1
    assert json.loads(proc.stdout)["verdict"] == "FAIL"


def _analyze(lines, duration=120, gap=120):
    spec = importlib.util.spec_from_file_location(
        "soak_report", Path(__file__).resolve().parents[2] / SCRIPT
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.analyze_lines(lines, duration, gap)


def _health(ms, usb=1):
    # Synthetic parser input; does not constitute real device soak evidence.
    return (
        f"health: uptime_ms={ms} heap=180000 min_heap=170000 "
        f"largest=100000 psram=7000000 usb={usb} wifi=1"
    )


FETCH = "fetch: provider=1 outcome=OK http=200 requests=1"
BOOT = "kdash boot fw=test board=e1001 reset=1 heap=190000 psram=7000000"


def test_continuous_gate_and_separate_memory_scopes():
    report = _analyze([BOOT, FETCH, _health(60000), _health(120000), _health(180000)])
    assert report["verdict"] == "PASS"
    assert report["scope"] == "continuous_usb_observation"
    assert report["health"]["duration_sec"] == 120
    assert report["health"]["samples"] == 3
    assert report["heap_trend"]["samples"] == 1
    assert report["health"]["internal_heap_last"] == 180000


@pytest.mark.parametrize(
    "samples, expected",
    [
        ([], "at least two"),
        ([_health(0)], "at least two"),
        ([_health(0), _health(60000)], "insufficient"),
        ([_health(0), _health(180000)], "gap exceeds"),
        ([_health(0), _health(120000, usb=0)], "non-USB"),
        ([_health(120000), _health(0)], "uptime reset"),
        ([_health(0), _health(0), _health(120000)], "duplicate"),
        ([_health(0), BOOT, _health(120000)], "reboot"),
        ([_health(0), "health: bad", _health(120000)], "malformed"),
    ],
)
def test_continuous_gate_rejects_missing_or_broken_evidence(samples, expected):
    report = _analyze([BOOT, FETCH, *samples])
    assert report["verdict"] == "FAIL"
    assert any(expected in failure for failure in report["failures"])


def test_health_uptime_past_32bit_millis_wrap():
    report = _analyze(
        [FETCH, _health(4294960000), _health(4295020000), _health(4295080000)]
    )
    assert report["verdict"] == "PASS"
    assert report["health"]["duration_sec"] == 120


def test_legacy_pass_is_explicitly_only_log_sanity():
    report = _analyze([BOOT, FETCH], duration=0)
    assert report["verdict"] == "PASS"
    assert report["scope"] == "log_sanity_only"
    assert report["health"] is None


def test_strict_cli_rejects_short_legacy_fixture():
    root = Path(__file__).resolve().parents[2]
    proc = subprocess.run(
        [sys.executable, SCRIPT, OK_LOG, "--min-duration-sec", "86400"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    assert proc.returncode == 1
    assert json.loads(proc.stdout)["scope"] == "continuous_usb_observation"


@pytest.mark.parametrize(
    "fault",
    [
        "checkpoint: persistence FAILED",
        "watchdog: subscription FAILED",
        "display: allocation FAILED",
        "display: page=0 crc=123 elapsed_ms=30000 ok=0",
        "charge: enabled=1 state=5 mv=4000 estimated_pct=91 readback=0 cfg=00 timer=00 status=00",
        "charge: enabled=1 state=2 mv=4000 estimated_pct=91 readback=0 cfg=00 timer=00 status=00",
    ],
)
def test_strict_soak_rejects_device_faults_despite_healthy_heap(fault):
    report = _analyze([BOOT, FETCH, _health(0), fault, _health(120000)])
    assert report["verdict"] == "FAIL"
    assert report["device_fault_count"] == 1


def test_fault_report_is_bounded_but_counts_all_occurrences():
    report = _analyze(
        [
            FETCH,
            _health(0),
            *(["checkpoint: persistence FAILED"] * 120),
            _health(120000),
        ]
    )
    assert report["device_fault_count"] == 120
    assert len(report["device_faults"]) == 50


@pytest.mark.parametrize(
    "events, verdict",
    [
        (["start", "connected", "rotate", "stop"], "PASS"),
        (["start", "connected", "intentional_reset", "stop"], "PASS"),
        (["start", "connected", "serial_disconnected", "connected", "stop"], "FAIL"),
        (["start", "connected", "oversize_line_dropped", "stop"], "FAIL"),
        (["start", "connected", "interrupted", "stop"], "FAIL"),
        (["start", "connected"], "FAIL"),
        (["stop", "connected", "start"], "FAIL"),
        (["start", "connected", "unknown_event", "stop"], "FAIL"),
    ],
)
def test_capture_integrity_includes_short_disconnects_and_discarded_data(
    events, verdict
):
    spec = importlib.util.spec_from_file_location(
        "soak_report", Path(__file__).resolve().parents[2] / SCRIPT
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    report = module.analyze_lines(
        [FETCH, _health(0), _health(120000)],
        120,
        capture_events=({"event": event} for event in events),
    )
    assert report["verdict"] == verdict
    assert report["capture_metadata_checked"] is True


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-q"]))
