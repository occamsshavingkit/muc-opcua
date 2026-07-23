#!/usr/bin/env python3
"""Integrity tests that run against the REAL committed manifest.

The other ``test_*.py`` modules here check the counting/validation helpers on
small synthetic fixtures. That proves the algorithms are right but says nothing
about ``profiles/opcua-profile-manifest.yaml`` itself, so a malformed link in
the real data can sit undetected -- ``satisfied_by`` pointing at a Kconfig
symbol instead of a manifest item id is the failure mode this module was added
for.

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
validate = _load("validate")

_MANIFEST = completion._load_yaml(_REPO / "profiles" / "opcua-profile-manifest.yaml")


def _cu_items() -> list[dict]:
    return [
        it
        for it in _MANIFEST.get("items", [])
        if isinstance(it, dict) and it.get("kind") == "conformance_unit"
    ]


class RealManifestReconciliationTest(unittest.TestCase):
    """Spec 073 FR-002 reconciliation links must resolve in the real manifest."""

    def test_no_dangling_or_unimplemented_satisfied_by_links(self) -> None:
        errors = validate._check_reconciliation_links(_MANIFEST)
        self.assertEqual(
            errors,
            [],
            "reconciliation links in the committed manifest must resolve to an "
            "implemented/claimed CU item id:\n  " + "\n  ".join(errors),
        )

    def test_satisfied_by_targets_are_item_ids_not_kconfig_symbols(self) -> None:
        """A Kconfig symbol is never a valid ``satisfied_by`` target.

        ``satisfied_by`` takes a manifest item id (``opc_cu_*``/``service_*``).
        Pointing it at the target's ``kconfig_symbol`` resolves to nothing and
        silently drops the reconciliation, so reject the shape outright -- it
        gives a far clearer message than "unknown CU".
        """
        symbols = {
            it.get("kconfig_symbol")
            for it in _MANIFEST.get("items", [])
            if isinstance(it, dict) and it.get("kconfig_symbol")
        }
        offenders = [
            (it.get("id"), it.get("satisfied_by"))
            for it in _cu_items()
            if it.get("satisfied_by") in symbols
        ]
        self.assertEqual(
            offenders,
            [],
            "satisfied_by must name a manifest item id, not a Kconfig symbol: "
            + repr(offenders),
        )


if __name__ == "__main__":
    unittest.main()
