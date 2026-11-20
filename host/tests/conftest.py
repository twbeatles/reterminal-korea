import json
import os
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
FIX = os.path.join(ROOT, "test", "fixtures")
GOLDEN = os.path.join(ROOT, "test", "golden", "provider_cases.json")
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


def load(name):
    with open(os.path.join(FIX, name), encoding="utf-8") as f:
        return json.load(f)


def raw(name) -> str:
    """Fixture as the raw HTTP body the device adapter receives."""
    with open(os.path.join(FIX, name), encoding="utf-8") as f:
        return f.read()
