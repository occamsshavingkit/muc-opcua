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
from collections import Counter

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


class ManifestJsonIntegrityTest(unittest.TestCase):
    def test_committed_manifest_is_strict_json(self) -> None:
        # Given the committed profile manifest.
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"

        # When it is parsed by the standard-library JSON parser.
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        # Then the canonical item collection is available without YAML fallback.
        self.assertIsInstance(manifest["items"], list)

    def test_committed_manifest_has_no_recursive_satisfied_by_keys(self) -> None:
        # Given the committed profile manifest parsed as strict JSON.
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        # When every object and array in the manifest is traversed recursively.
        def count_satisfied_by_keys(value: object) -> int:
            if isinstance(value, dict):
                return int("satisfied_by" in value) + sum(
                    count_satisfied_by_keys(child) for child in value.values()
                )
            if isinstance(value, list):
                return sum(count_satisfied_by_keys(child) for child in value)
            return 0

        satisfied_by_count = count_satisfied_by_keys(manifest)

        # Then no satisfied_by middleman key remains at any depth.
        self.assertEqual(satisfied_by_count, 0)

    def test_committed_manifest_has_unique_canonical_cu_ids(self) -> None:
        # Given the committed profile manifest parsed as strict JSON.
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        # When canonical OPC CU identifiers are counted across CU items.
        cu_ids = [
            str(item["opc_reference"]["cu_id"])
            for item in manifest["items"]
            if item.get("kind") == "conformance_unit"
            and isinstance(item.get("opc_reference"), dict)
            and item["opc_reference"].get("cu_id") is not None
        ]
        duplicates = {
            cu_id: count for cu_id, count in Counter(cu_ids).items() if count > 1
        }

        # Then every canonical OPC CU identifier has one manifest owner.
        self.assertEqual(duplicates, {})

    def test_authorization_service_configuration_server_has_canonical_owner(self) -> None:
        # Given the committed profile manifest parsed as strict JSON.
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        # When the Authorization Service Configuration Server entries are selected.
        items = manifest["items"]
        self.assertNotIn(
            "cu_authorization_service_server",
            [item.get("id") for item in items],
        )
        canonical_items = [item for item in items if item.get("id") == "opc_cu_3182"]

        # Then the canonical CU is the sole implemented owner with complete evidence.
        self.assertEqual(len(canonical_items), 1)
        canonical_item = canonical_items[0]
        self.assertEqual(
            canonical_item["kconfig_symbol"],
            "MUC_OPCUA_CU_AUTHORIZATION_SERVICE_CONFIGURATION_SERVER",
        )
        self.assertEqual(canonical_item["implementation_state"], "implemented")
        self.assertEqual(
            canonical_item["semantic_depends_on"],
            [
                "MUC_OPCUA_CU_USER_TOKEN_JWT",
                "MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION",
            ],
        )
        self.assertEqual(canonical_item["depends_on_op"], "and")
        self.assertIn("tests/unit/test_type_system.c", canonical_item["backing_tests"])
        self.assertEqual(
            canonical_item["notes"],
            "AuthorizationServiceConfigurationType nodes are gated directly by this CU.",
        )

    def test_reverse_connect_server_has_canonical_owner(self) -> None:
        # Given the committed profile manifest parsed as strict JSON.
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        # When the Reverse Connect Server entries are selected.
        items = manifest["items"]
        self.assertNotIn(
            "opc_cu_reverse_connect",
            [item.get("id") for item in items],
        )
        canonical_items = [item for item in items if item.get("id") == "opc_cu_2867"]

        # Then the canonical CU is the sole claimed owner with complete evidence.
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
