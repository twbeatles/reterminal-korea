"""UART capture tests use a synthetic serial port; no hardware access."""

import importlib.util
import json
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


def module():
    spec = importlib.util.spec_from_file_location(
        "uart_logger", Path(__file__).resolve().parents[2] / "tools/uart_logger.py"
    )
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def test_uart_sanitizes_before_storage():
    mod = module()
    raw = (
        b"Author"
        + b"ization: "
        + b"Bear"
        + b"er "
        + b"synthetic_private_value_1234567890\n"
    )
    assert "synthetic_private_value" not in mod.sanitize_line(raw)
    assert mod.sanitize_line(b"health: uptime_ms=60000 heap=200000\r\n").startswith(
        "health:"
    )
    assert "\ufffd" in mod.sanitize_line(b"broken: \xff")


@pytest.mark.parametrize("auto_report", [False, True])
def test_uart_capture_reconnects_rotates_and_does_not_reset(
    tmp_path, monkeypatch, auto_report
):
    mod = module()
    ports = []
    ticks = [0]

    def monotonic():
        ticks[0] += 1
        return ticks[0]

    class Disconnected(Exception):
        pass

    class Serial:
        def __init__(self, **kwargs):
            self.dtr = self.rts = None
            self.in_waiting = 3000
            self.reads = 0
            ports.append(self)

        def open(self):
            assert self.dtr is False and self.rts is False
            if len(ports) == 1:
                raise Disconnected()

        def read(self, count):
            self.reads += 1
            if self.reads <= 2:
                return b"x" * 4096
            if self.reads == 3:
                return b"orphaned_tail\nhealth: recovered\n"
            return ("health: " + "x" * 2800 + "\n").encode()

        def close(self):
            pass

    monkeypatch.setitem(
        sys.modules,
        "serial",
        SimpleNamespace(Serial=Serial, SerialException=Disconnected),
    )
    mod.time = SimpleNamespace(monotonic=monotonic, sleep=lambda _: None)
    output = tmp_path / "session.log"
    options = ["FAKE", str(output), "--duration-sec", "20", "--rotate-bytes", "4096"]
    if auto_report:
        options += ["--soak-min-duration-sec", "1"]
    assert mod.main(options) == (1 if auto_report else 0)
    if auto_report:
        report = json.loads(output.with_suffix(".log.report.json").read_text())
        assert report["verdict"] == "FAIL"  # capture completion cannot prove a soak
        assert report["scope"] == "continuous_usb_observation"
    assert output.exists()
    assert list(tmp_path.glob("session.log.0*"))
    events = [
        json.loads(line)["event"]
        for line in output.with_suffix(".log.events.jsonl").read_text().splitlines()
    ]
    assert "serial_disconnected" in events
    assert "oversize_unterminated_line_dropped" in events
    assert all(
        "orphaned_tail" not in path.read_text()
        for path in tmp_path.glob("session.log*")
    )
    assert "intentional_reset" not in events
    assert events[-1] == "stop"
    assert all(port.rts is False for port in ports)
    with pytest.raises(SystemExit):
        mod.main(["FAKE", str(output)])  # never overwrite existing evidence
