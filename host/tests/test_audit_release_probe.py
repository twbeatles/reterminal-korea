"""Audit release/probe regressions, using temporary artifacts and fake network."""

import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def module(name, rel):
    spec = importlib.util.spec_from_file_location(name, ROOT / rel)
    loaded = importlib.util.module_from_spec(spec)
    sys.modules[name] = loaded
    spec.loader.exec_module(loaded)
    return loaded


release = module("audit_package_release", "scripts/package_release.py")
probe = module("audit_api_probe", "tools/api_probe.py")


def test_binary_scan_distinguishes_sdk_pem_delimiter_from_key(tmp_path):
    bdir = checkout(tmp_path)
    fw = bdir / "firmware.bin"
    original = fw.read_bytes()
    # Only standalone C-string parser delimiters are inert, never a key body.
    prefix = b"-----BE" + b"GIN "
    delimiters = (
        b"\x00"
        + prefix
        + b"RSA PRIVATE KEY-----\x00"
        + prefix
        + b"EC PRIVATE KEY-----\x00"
        + prefix
        + b"PRIVATE KEY-----\x00"
    )
    fw.write_bytes(original + delimiters)
    assert release.inspect_images(tmp_path, "0.3.0", environ={})
    fw.write_bytes(
        original + b"\x00" + prefix + b"PRIVATE KEY-----\n"
        b"FAKE-KEY-MATERIAL-FOR-REJECTION\n-----END PRIVATE KEY-----\x00"
    )
    with pytest.raises(release.ReleaseError, match="secret-like"):
        release.inspect_images(tmp_path, "0.3.0", environ={})


def checkout(tmp_path):
    (tmp_path / "include").mkdir()
    (tmp_path / "platformio.ini").write_text(
        "[env:reterminal_e1001]\nboard = seeed_xiao_esp32s3\n"
        "board_build.arduino.memory_type = qio_opi\n"
        'build_flags =\n  -D RETERMINAL_E1001\n  -D KDASH_FW_VERSION=\\"0.3.0\\"\n',
        encoding="utf-8",
    )
    bdir = tmp_path / ".pio" / "build" / "reterminal_e1001"
    bdir.mkdir(parents=True)
    for name in release.ARTIFACTS:
        data = (
            b"\xe9\x00image"
            if name != "firmware.bin"
            else b"\xe9\x00KDASH_BUILD_ID|reterminal_e1001|0.3.0|END\x00"
        )
        (bdir / name).write_bytes(data)
    return bdir


def git_ok(args, root):
    output = "a" * 40 if args[1] == "rev-parse" else ""
    return subprocess.CompletedProcess(args, 0, output, "")


@pytest.mark.parametrize("version", ["../escaped", "1.2", "01.2.3", "1.2.3/../../x"])
def test_release_rejects_unsafe_version_before_creating_output(tmp_path, version):
    checkout(tmp_path)
    with pytest.raises(release.ReleaseError):
        release.package(tmp_path, version, build=False, runner=git_ok, environ={})
    assert not (tmp_path / "dist").exists()


@pytest.mark.parametrize("status,output", [(1, ""), (0, "?? unknown\n")])
def test_release_git_checks_fail_closed(tmp_path, status, output):
    checkout(tmp_path)

    def bad_git(args, root):
        return subprocess.CompletedProcess(args, status, output, "")

    with pytest.raises(release.ReleaseError):
        release.package(tmp_path, "0.3.0", build=False, runner=bad_git)
    assert not (tmp_path / "dist").exists()


def test_release_requires_committed_revision(tmp_path):
    checkout(tmp_path)

    def no_commit(args, root):
        return subprocess.CompletedProcess(args, 0, "", "")

    with pytest.raises(release.ReleaseError, match="committed"):
        release.package(tmp_path, "0.3.0", build=False, runner=no_commit)


@pytest.mark.parametrize(
    "fault",
    [
        "missing",
        "identity",
        "board",
        "config-version",
        "dev-credentials",
        "marker",
        "known-secret",
    ],
)
def test_release_rejects_incomplete_mismatched_or_sensitive_images(tmp_path, fault):
    bdir = checkout(tmp_path)
    env = {}
    if fault == "missing":
        (bdir / "partitions.bin").unlink()
    elif fault == "identity":
        (bdir / "firmware.bin").write_bytes(
            b"KDASH_BUILD_ID|reterminal_e1002|0.3.0|END"
        )
    elif fault in ("board", "config-version"):
        path = tmp_path / "platformio.ini"
        text = path.read_text()
        path.write_text(
            text.replace("seeed_xiao_esp32s3", "wrong")
            if fault == "board"
            else text.replace("0.3.0", "0.3.1")
        )
    elif fault == "dev-credentials":
        (tmp_path / "include" / "dev_credentials.h").write_text("// local only")
    elif fault == "marker":
        (bdir / "bootloader.bin").write_bytes(b"KDASH_TEST_SECRET_MARKER")
    else:
        env["KDASH_KMA_KEY"] = "FAKE-private-value"
        (bdir / "bootloader.bin").write_bytes(env["KDASH_KMA_KEY"].encode())
    with pytest.raises(release.ReleaseError):
        release.package(tmp_path, "0.3.0", build=False, runner=git_ok, environ=env)
    assert not (tmp_path / "dist").exists()


def test_release_build_and_all_images_hashes(tmp_path):
    bdir = checkout(tmp_path)
    calls = []

    def runner(args, root):
        calls.append(args)
        return git_ok(args, root)

    result = release.package(tmp_path, "0.3.0", runner=runner, environ={})
    meta = json.loads((result / "version.json").read_text())
    assert meta["version"] == "0.3.0" and meta["board"] == "reterminal_e1001"
    assert set(meta["sha256"]) == set(release.ARTIFACTS)
    assert ["pio", "run", "-e", "reterminal_e1001"] in calls
    for name in release.ARTIFACTS:
        assert (result / name).read_bytes() == (bdir / name).read_bytes()
    assert (result / "FLASHING.md").is_file()
    assert len((result / "SHA256SUMS").read_text().splitlines()) == 3


def test_probe_dry_run_and_no_keys_never_call_network(tmp_path, capsys):
    def forbidden(_):
        pytest.fail("network must not be reached")

    assert (
        probe.main(["--dry-run"], environ={}, root=tmp_path, transport=forbidden) == 0
    )
    assert probe.main([], environ={}, root=tmp_path, transport=forbidden) == 2
    assert not (tmp_path / "evidence").exists()
    assert "PLAN ONLY" in capsys.readouterr().out


def test_probe_quota_and_daily_repeated_run_cap(tmp_path):
    env = {"KDASH_EXIM_KEY": "FAKE-probe-env-key"}
    calls = []

    def fake(item):
        calls.append(item)
        return (
            200,
            "application/json",
            b'[{"result":1,"cur_unit":"USD","deal_bas_r":"1,384.50"}]',
        )

    args = ["--live", "--provider", "exim"]
    assert probe.main(args, environ=env, root=tmp_path, transport=fake) == 2
    assert calls == []
    args += ["--daily-quota", "exim=300"]
    assert probe.main(args, environ=env, root=tmp_path, transport=fake) == 0
    assert len(calls) == 3
    assert probe.main(args, environ=env, root=tmp_path, transport=fake) == 2
    assert len(calls) == 3


def test_probe_sanitizes_echoes_but_preserves_ignored_raw(tmp_path):
    env = {"KDASH_EXIM_KEY": "FAKE-env-echo-value"}
    raw = json.dumps(
        [
            {
                "result": 1,
                "cur_unit": "USD",
                "deal_bas_r": "1384.50",
                "unexpected_echo": env["KDASH_EXIM_KEY"],
            }
        ]
    ).encode()
    items = probe.plan(["exim"], env, 1793600000, "종로구", 60, 127, "005930")
    assert (
        probe.execute(
            items, tmp_path, env, 1793600000, lambda _: (200, "application/json", raw)
        )
        == 0
    )
    for path in (tmp_path / "test" / "fixtures").iterdir():
        assert env["KDASH_EXIM_KEY"] not in path.read_text(encoding="utf-8")
        assert "REDACTED" in path.read_text(encoding="utf-8")
    assert any(
        path.read_bytes() == raw
        for path in (tmp_path / "evidence" / "probe" / "raw").rglob("*.bin")
    )


def test_probe_http200_internal_error_is_failure_and_urls_not_logged(tmp_path, capsys):
    env = {"KDASH_EXIM_KEY": "FAKE-env-probe-auth"}
    args = ["--live", "--provider", "exim", "--daily-quota", "exim=300"]
    assert (
        probe.main(
            args,
            environ=env,
            root=tmp_path,
            transport=lambda _: (200, "application/json", b'[{"result":3}]'),
        )
        == 1
    )
    records = json.loads(
        next((tmp_path / "evidence" / "probe").glob("*.json")).read_text()
    )["records"]
    assert all(r["outcome"] == "AuthError" for r in records)
    output = capsys.readouterr().out
    assert env["KDASH_EXIM_KEY"] not in output and "https://" not in output


def test_probe_network_exception_text_is_not_written(tmp_path, capsys):
    env = {"KDASH_EXIM_KEY": "FAKE-env-probe-network"}

    def fail(item):
        raise OSError(item.url)

    args = ["--live", "--provider", "exim", "--daily-quota", "exim=300"]
    assert probe.main(args, environ=env, root=tmp_path, transport=fail) == 1
    report = next((tmp_path / "evidence" / "probe").glob("*.json")).read_text()
    assert env["KDASH_EXIM_KEY"] not in report
    assert "https://" not in report
    assert env["KDASH_EXIM_KEY"] not in capsys.readouterr().out


def test_probe_does_not_follow_redirects():
    assert (
        probe.NoRedirect().redirect_request(
            None, None, 302, "", {}, "https://example.com"
        )
        is None
    )


def test_probe_fetch_rejects_unapproved_url_before_open():
    with pytest.raises(ValueError, match="unapproved"):
        probe.fetch(probe.Probe("exim", "forbidden", "https://example.com"))


@pytest.mark.parametrize(
    "declared,chunks,expected",
    [
        ("1000", [b"{}"], "incomplete"),
        (None, [b"x" * 65536], None),
        (None, [b"x" * 65536, b"x"], "oversize"),
        ("65537", [], "oversize"),
        ("2", [b"{}"], None),
    ],
)
def test_probe_real_reader_framing_and_cap(monkeypatch, declared, chunks, expected):
    class Response:
        code = 200
        headers = {"Content-Type": "application/json"}

        def __init__(self):
            self.parts = list(chunks)
            if declared is not None:
                self.headers = dict(self.headers, **{"Content-Length": declared})

        def read1(self, n):
            return self.parts.pop(0) if self.parts else b""

        read = read1

        def __enter__(self):
            return self

        def __exit__(self, *_):
            pass

    class Opener:
        def open(self, req, timeout):
            assert timeout == 15
            return Response()

    def build(*handlers):
        contexts = [
            handler._context for handler in handlers if hasattr(handler, "_context")
        ]
        assert contexts and contexts[0].check_hostname
        assert contexts[0].verify_mode == probe.ssl.CERT_REQUIRED
        return Opener()

    monkeypatch.setattr(probe.urllib.request, "build_opener", build)
    item = probe.Probe("exim", "cap", "https://oapi.koreaexim.go.kr/")
    if expected:
        with pytest.raises(ValueError, match=expected):
            probe.fetch(item)
    else:
        assert probe.fetch(item)[2] == b"".join(chunks)


def test_probe_kis_token_stays_in_memory_or_ignored_raw(tmp_path):
    env = {
        "KDASH_KIS_APP_KEY": "FAKE-kis-probe-key",
        "KDASH_KIS_APP_SECRET": "FAKE-kis-probe-secret",
    }
    value = "FAKE-issued-access-value"
    calls = []

    def fake(item):
        calls.append(item.name)
        if item.name == "token":
            assert json.loads(item.body)["grant_type"] == "client_credentials"
            return (
                200,
                "application/json",
                json.dumps({"access_token": value, "expires_in": 86400}).encode(),
            )
        assert item.headers["authorization"] == "Bearer " + value
        return 200, "application/json", b'{"rt_cd":"0","output":{"stck_prpr":"70000"}}'

    items = probe.plan(["kis"], env, 1793600000, "종로구", 60, 127, "005930")
    assert probe.execute(items, tmp_path, env, 1793600000, fake) == 0
    assert calls == ["token", "price"]
    for path in (tmp_path / "test" / "fixtures").iterdir():
        text = path.read_text(encoding="utf-8")
        assert value not in text
        assert env["KDASH_KIS_APP_SECRET"] not in text


def test_probe_budget_reservation_is_exclusive(tmp_path):
    from concurrent.futures import ThreadPoolExecutor

    items = [probe.Probe("exim", "one", "")]

    def reserve():
        try:
            probe.reserve_budget(tmp_path, items, {"exim": 100}, 1793600000)
            return True
        except (ValueError, OSError):
            return False

    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda _: reserve(), range(2)))
    assert sorted(results) == [False, True]
