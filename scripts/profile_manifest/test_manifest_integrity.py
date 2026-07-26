#!/usr/bin/env python3
"""Integrity tests that run against the REAL committed manifest.

The other ``test_*.py`` modules here check the counting/validation helpers on
small synthetic fixtures.

These tests deliberately load the committed manifest rather than a fixture.
"""

from __future__ import annotations

import importlib.util
import pathlib
import unittest

_HERE = pathlib.Path(__file__).resolve().parent
_REPO = _HERE.parents[1]


def _load(name: str):
    path = _HERE / (name + ".py")
    spec = importlib.util.spec_from_file_location("profile_manifest_" + name, path)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


completion = _load("completion")

if __name__ == "__main__":
    unittest.main()
