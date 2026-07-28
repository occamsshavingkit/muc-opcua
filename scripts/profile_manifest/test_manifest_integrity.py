#!/usr/bin/env python3
"""Integrity tests that run against the REAL committed manifest.

The other ``test_*.py`` modules here check the counting/validation helpers on
small synthetic fixtures.

These tests deliberately load the committed manifest rather than a fixture.
"""

from __future__ import annotations

import importlib.util
import json
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

class ManifestIntegrityTest(unittest.TestCase):
    def test_standard_profile_discovery_aggregate_remains_selectable(self) -> None:
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        service_discovery = next(
            item for item in manifest["items"] if item.get("id") == "service_discovery"
        )
        self.assertEqual(service_discovery["implementation_state"], "claimed")
        self.assertTrue(service_discovery["profile_defaults"]["standard"])
        self.assertIn("test_discovery_endpoint", service_discovery["backing_tests"])

    def test_full_profile_write_aggregate_remains_selectable(self) -> None:
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        service_write = next(
            item for item in manifest["items"] if item.get("id") == "service_write"
        )
        self.assertEqual(service_write["implementation_state"], "claimed")
        self.assertTrue(service_write["profile_defaults"]["full"])
        self.assertIn("test_write_service", service_write["backing_tests"])

    def test_reverse_connect_server_has_canonical_owner(self) -> None:
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        items = manifest["items"]
        self.assertNotIn(
            "opc_cu_reverse_connect",
            [item.get("id") for item in items],
        )
        canonical_items = [item for item in items if item.get("id") == "opc_cu_2867"]

        self.assertEqual(len(canonical_items), 1)
        canonical_item = canonical_items[0]
        self.assertEqual(
            canonical_item["kconfig_symbol"],
            "MUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER",
        )
        self.assertEqual(canonical_item["implementation_state"], "claimed")
        self.assertFalse(canonical_item["cu_optional"])
        self.assertIn("test_reverse_connect", canonical_item["backing_tests"])
        self.assertEqual(canonical_item["opc_reference"]["spec"], "OPC-10000-6")
        self.assertEqual(canonical_item["opc_reference"]["section"], "7.1.3")


if __name__ == "__main__":
    unittest.main()
