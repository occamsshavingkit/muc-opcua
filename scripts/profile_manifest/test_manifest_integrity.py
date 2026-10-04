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
graph_deps = _load("graph_deps")


class ManifestIntegrityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        super().setUpClass()
        manifest_path = _REPO / "profiles" / "opcua-profile-manifest.yaml"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            cls.manifest = json.load(manifest_file)
        graph_path = _REPO / "profiles" / "opcua-profile-graph.json"
        with graph_path.open(encoding="utf-8") as graph_file:
            cls.graph = json.load(graph_file)

    def test_committed_manifest_is_strict_json(self) -> None:
        self.assertIsInstance(self.manifest["items"], list)

    def test_committed_manifest_has_no_recursive_satisfied_by_keys(self) -> None:
        # When every object and array in the manifest is traversed recursively.
        def count_satisfied_by_keys(value) -> int:
            if isinstance(value, dict):
                return int("satisfied_by" in value) + sum(
                    count_satisfied_by_keys(child) for child in value.values()
                )
            if isinstance(value, list):
                return sum(count_satisfied_by_keys(child) for child in value)
            return 0

        satisfied_by_count = count_satisfied_by_keys(self.manifest)

        # Then no satisfied_by middleman key remains at any depth.
        self.assertEqual(satisfied_by_count, 0)

    def test_committed_manifest_has_unique_canonical_cu_ids(self) -> None:
        # When canonical OPC CU identifiers are counted across CU items.
        cu_ids = [
            str(item["opc_reference"]["cu_id"])
            for item in self.manifest["items"]
            if item.get("kind") == "conformance_unit"
            and isinstance(item.get("opc_reference"), dict)
            and item["opc_reference"].get("cu_id") is not None
        ]
        duplicates = {
            cu_id: count for cu_id, count in Counter(cu_ids).items() if count > 1
        }

        # Then every canonical OPC CU identifier has one manifest owner.
        self.assertEqual(duplicates, {})

    def test_standard_mandatory_graph_cus_have_exactly_one_manifest_owner(self) -> None:
        # Given the canonical mandatory CU closure for Standard 2025 UA Server.
        mandatory_cu_names = graph_deps._mandatory_cu_names(self.graph, "2269")

        # When manifest owners are grouped by canonical graph CU name.
        owners_by_name = {
            cu_name: [
                item["id"]
                for item in self.manifest["items"]
                if item.get("kind") == "conformance_unit"
                and isinstance(item.get("opc_reference"), dict)
                and item["opc_reference"].get("cu_name") == cu_name
            ]
            for cu_name in mandatory_cu_names
        }
        invalid_owners = {
            cu_name: owners
            for cu_name, owners in owners_by_name.items()
            if len(owners) != 1
        }

        # Then all 52 mandatory CUs have one unambiguous manifest owner.
        self.assertEqual(len(mandatory_cu_names), 52)
        self.assertEqual(invalid_owners, {})

    def test_capacity_cus_have_explicit_standard_project_metadata(self) -> None:
        capacity_cus = [
            (
                "opc_monitor_items_500",
                "Monitor Items 500",
                5242,
            ),
            (
                "opc_monitor_minqueuesize_05",
                "Monitor MinQueueSize_05",
                5250,
            ),
            (
                "opc_cu_5248",
                "Subscription Minimum 05",
                5248,
            ),
            (
                "opc_cu_5249",
                "Subscription Publish Min 10",
                5249,
            ),
        ]
        items_by_id = {item["id"]: item for item in self.manifest["items"]}

        for item_id, display_name, cu_id in capacity_cus:
            with self.subTest(item_id=item_id):
                item = items_by_id[item_id]
                self.assertEqual(item["opc_reference"]["cu_name"], display_name)
                self.assertEqual(str(item["opc_reference"]["cu_id"]), str(cu_id))
                self.assertEqual(
                    item["semantic_depends_on"],
                    ["MUC_OPCUA_CU_SUBSCRIPTION_STANDARD"],
                )
                self.assertEqual(item["project_profile_defaults"], {"standard": True})
                self.assertEqual(
                    item["project_required_for_profile"], {"standard": True}
                )

    def test_view_and_discovery_have_only_dedicated_canonical_owners(self) -> None:
        items = self.manifest["items"]
        items_by_id = {item["id"]: item for item in items}

        self.assertNotIn("service_browse", items_by_id)
        self.assertNotIn("service_discovery", items_by_id)
        self.assertEqual(
            items_by_id["opc_cu_2317"]["kconfig_symbol"],
            "MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH",
        )
        self.assertEqual(
            items_by_id["opc_cu_3530"]["kconfig_symbol"],
            "MUC_OPCUA_CU_VIEW_BASIC_2",
        )
        self.assertEqual(
            items_by_id["opc_cu_2328"]["kconfig_symbol"],
            "MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS",
        )
        self.assertEqual(
            items_by_id["opc_cu_2352"]["kconfig_symbol"],
            "MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF",
        )

    def test_write_has_only_the_dedicated_canonical_owner(self) -> None:
        items_by_id = {item["id"]: item for item in self.manifest["items"]}
        self.assertNotIn("service_write", items_by_id)

        canonical_write = items_by_id["opc_cu_2389"]
        self.assertEqual(canonical_write["implementation_state"], "claimed")
        self.assertEqual(
            canonical_write["kconfig_symbol"],
            "MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES",
        )
        self.assertIn("test_write_service", canonical_write["backing_tests"])

    def test_authorization_service_configuration_server_has_canonical_owner(self) -> None:

        # When the Authorization Service Configuration Server entries are selected.
        items = self.manifest["items"]
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
        self.assertEqual(canonical_item["opc_reference"]["spec"], "OPC-10000-12")
        self.assertEqual(canonical_item["opc_reference"]["section"], "9.7.4")
        self.assertEqual(
            canonical_item["notes"],
            "AuthorizationServiceConfigurationType nodes are gated directly by this CU.",
        )

    def test_reverse_connect_server_has_canonical_owner(self) -> None:
        items = self.manifest["items"]
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
        self.assertEqual(
            canonical_item["profile_defaults"],
            {
                "custom": False,
                "embedded": False,
                "full": True,
                "micro": False,
                "nano": False,
                "standard": False,
            },
        )

    def test_reverse_connect_facet_matches_canonical_profile_when_manifest_loaded(
        self,
    ) -> None:
        # Given the committed real manifest.
        # When the canonical Reverse Connect Server Facet is selected.
        reverse_connect_facets = [
            item
            for item in self.manifest["items"]
            if item.get("id") == "opc_facet_1632"
        ]

        # Then its identity, symbol, profile source, and defaults are exact.
        self.assertEqual(len(reverse_connect_facets), 1)
        reverse_connect_facet = reverse_connect_facets[0]
        self.assertEqual(
            reverse_connect_facet["opc_display_name"],
            "Reverse Connect Server Facet",
        )
        self.assertEqual(
            reverse_connect_facet["kconfig_symbol"],
            "MUC_OPCUA_FACET_REVERSE_CONNECT_SERVER",
        )
        self.assertEqual(reverse_connect_facet["implementation_state"], "implemented")
        self.assertEqual(reverse_connect_facet["opc_reference"]["profile_id"], "1632")
        self.assertEqual(
            reverse_connect_facet["opc_reference"]["profile_uri"],
            "http://opcfoundation.org/UA-Profile/Server/ReverseConnect",
        )
        self.assertEqual(
            reverse_connect_facet["profile_defaults"],
            {
                "custom": False,
                "embedded": False,
                "full": True,
                "micro": False,
                "nano": False,
                "standard": False,
            },
        )
        self.assertEqual(
            reverse_connect_facet["source_metadata"],
            {
                "profile_group_id": 171,
                "source_endpoint": "profile/?pg=171&all=1",
            },
        )

    def test_reverse_connect_facet_contains_only_canonical_cu_when_manifest_loaded(
        self,
    ) -> None:
        # Given the committed real manifest.
        # When Reverse Connect Server Facet containment is resolved.
        containment = self.manifest["facet_containment"].get("opc_facet_1632")

        # Then CU 2867 is its single mandatory child.
        self.assertEqual(containment, ["opc_cu_2867"])

    def test_core_2022_facet_excludes_reverse_connect_when_manifest_loaded(self) -> None:
        # Given the committed real manifest.
        # When Core 2022 Server Facet containment is resolved.
        core_2022_cus = self.manifest["facet_containment"]["opc_facet_1322"]

        # Then CU 2867 is not assigned to that noncanonical owner.
        self.assertNotIn("opc_cu_2867", core_2022_cus)

    def test_find_servers_self_uses_cu_specific_tests_when_manifest_loaded(self) -> None:
        # Given the committed real manifest.
        # When CU 2352 evidence is selected.
        find_servers_self = next(
            item for item in self.manifest["items"] if item.get("id") == "opc_cu_2352"
        )

        # Then it names both executables containing CU-specific scenarios.
        self.assertEqual(
            find_servers_self["backing_tests"],
            ["test_discovery_services", "test_discovery_endpoint"],
        )


if __name__ == "__main__":
    unittest.main()
