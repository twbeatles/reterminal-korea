"""Publication guards use synthetic credentials and isolated Git fixtures."""

import importlib.util
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def test_public_noreply_is_distinct_from_personal_email():
    from kdash.secret_scan import find_secrets

    assert find_secrets("synthetic@users.noreply.github.com") == []
    assert find_secrets("person" + "@private.invalid") == [(1, "email")]


def load(relative, name):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_private_configuration_is_outside_and_missing_dev_file_fails(
    tmp_path, monkeypatch
):
    private = load("scripts/private_config.py", "private_test")
    project = tmp_path / "repo"
    project.mkdir()
    with pytest.raises(ValueError):
        private.private_directory(
            project, {"KDASH_PRIVATE_CONFIG_DIR": str(project / "private")}
        )
    directory = tmp_path / "personal"
    directory.mkdir()
    monkeypatch.setenv("KDASH_PRIVATE_CONFIG_DIR", str(directory))

    class Environment:
        added = None

        def subst(self, _):
            return str(project)

        def Prepend(self, **values):
            self.added = values

    env = Environment()
    with pytest.raises(RuntimeError):
        private.configure(env)
    (directory / "kdash_private_credentials.h").write_text(
        "// synthetic header", encoding="utf-8"
    )
    private.configure(env)
    assert env.added == {"CPPPATH": [str(directory.resolve())]}


def test_known_value_scanner_handles_binary_utf16_and_encoded_keys():
    audit = load("tools/check_public_repo.py", "public_scanner_test")
    value = ("synthetic_" + "private_123456789").encode()
    assert audit.scan_blob(b"\0binary\0" + value, {value})
    assert audit.scan_blob(value.decode().encode("utf-16"), {value})
    assert audit.scan_blob(b"ordinary source file", {value}) == []


def test_reachable_history_retains_removed_secret_and_metadata_is_private(tmp_path):
    audit = load("tools/check_public_repo.py", "public_history_test")
    repo = tmp_path / "repo"
    private = tmp_path / "private"
    repo.mkdir()
    private.mkdir()
    value = "SYNTHETIC" + "_KEY_0123456789"
    (private / "kdash_private_credentials.h").write_text(
        f'#define KDASH_DEV_KMA_KEY "{value}"\n', encoding="utf-8"
    )

    def git(*args):
        return subprocess.run(["git", *args], cwd=repo, capture_output=True, check=True)

    git("init", "-b", "main")
    git("config", "user.name", "Synthetic Test")
    git("config", "user.email", "synthetic@users.noreply.github.com")
    (repo / "file.txt").write_text(value, encoding="utf-8")
    git("add", "file.txt")
    git("commit", "-m", "synthetic history fixture")
    (repo / "file.txt").write_text("clean", encoding="utf-8")
    git("add", "file.txt")
    git("commit", "-m", "remove fixture value")
    env = {"KDASH_PRIVATE_CONFIG_DIR": str(private)}
    assert audit.audit(repo, environ=env)["source_scan_pass"] is True
    report = audit.audit(repo, history=True, environ=env)
    assert report["source_scan_pass"] is False
    assert report["author_metadata_requires_review"] is False
    assert any(
        hit["kind"] == "known_private_value"
        for row in report["findings"]
        for hit in row["hits"]
    )
    assert value not in str(report)


@pytest.mark.parametrize(
    "override",
    [
        "PLATFORMIO_BUILD_FLAGS",
        "PLATFORMIO_BUILD_SRC_FLAGS",
        "PLATFORMIO_EXTRA_SCRIPTS",
    ],
)
def test_public_package_rejects_build_override(tmp_path, override):
    release = load("scripts/package_release.py", "package_private_test")
    (tmp_path / "platformio.ini").write_text(
        (ROOT / "platformio.ini").read_text(encoding="utf-8"), encoding="utf-8"
    )
    with pytest.raises(release.ReleaseError):
        release.public_build_preflight(tmp_path, {override: "synthetic override"})


@pytest.mark.parametrize("payload", ["marker", "known_value"])
def test_release_images_reject_private_build_marker_or_known_external_key(
    tmp_path, payload
):
    release = load("scripts/package_release.py", "package_private_image_test")
    repo = tmp_path / "repo"
    private = tmp_path / "private"
    repo.mkdir()
    private.mkdir()
    (repo / "platformio.ini").write_text(
        (ROOT / "platformio.ini").read_text(encoding="utf-8"), encoding="utf-8"
    )
    value = "SYNTHETIC" + "_KEY_9876543210"
    (private / "kdash_private_credentials.h").write_text(
        f'#define KDASH_DEV_KIS_APP_KEY "{value}"\n', encoding="utf-8"
    )
    build = repo / ".pio" / "build" / "reterminal_e1001"
    build.mkdir(parents=True)
    for name in release.ARTIFACTS:
        data = b"KDASH_BUILD_ID|reterminal_e1001|1.0.0|END\0"
        data += b"KDASH_PRIVATE_BUILD|END" if payload == "marker" else value.encode()
        (build / name).write_bytes(data)
    with pytest.raises(release.ReleaseError):
        release.inspect_images(
            repo, "1.0.0", {"KDASH_PRIVATE_CONFIG_DIR": str(private)}
        )
