"""Private build configuration lives outside the repository; never echo values."""

import ast
import os
import re
from pathlib import Path
from urllib.parse import quote, unquote


def private_directory(project, environ=None, home=None):
    environ = os.environ if environ is None else environ
    home = Path.home() if home is None else Path(home)
    directory = Path(
        environ.get("KDASH_PRIVATE_CONFIG_DIR") or home / ".config" / "kdash-e1001"
    )
    directory = directory.expanduser().resolve()
    project = Path(project).resolve()
    if directory == project or project in directory.parents:
        raise ValueError("private configuration must be outside the repository")
    return directory


def configure(environment):
    try:
        directory = private_directory(environment.subst("$PROJECT_DIR"))
    except ValueError:
        raise RuntimeError(
            "private_config: choose a directory outside the repository"
        ) from None
    if not (directory / "kdash_private_credentials.h").is_file():
        raise RuntimeError(
            "private_config: private header missing; use the default environment for portal setup"
        )
    environment.Prepend(CPPPATH=[str(directory)])


def known_private_values(project, environ=None):
    header = private_directory(project, environ) / "kdash_private_credentials.h"
    if not header.is_file():
        return set()
    values = set()
    pattern = r'#\s*define\s+KDASH_DEV_([A-Z_]+)\s+("(?:[^"\\]|\\.)*")'
    for name, literal in re.findall(pattern, header.read_text(encoding="utf-8-sig")):
        if not (name.endswith(("KEY", "SECRET")) or name == "WIFI_SSID"):
            continue
        value = ast.literal_eval(literal)
        if not value:
            continue
        for variant in (value, unquote(value), quote(unquote(value), safe="")):
            values.add(variant.encode("utf-8"))
    return values


# PlatformIO supplies Import; ordinary Python tests can import this safely.
if "Import" in globals():
    Import("env")  # noqa: F821
    configure(env)  # noqa: F821
