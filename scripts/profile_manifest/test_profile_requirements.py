#!/usr/bin/env python3
from __future__ import annotations

import importlib
import os
import re
import sys
import tempfile
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
_KCONFIG_DIR = os.path.abspath(os.path.join(_HERE, "..", "kconfig"))
if _KCONFIG_DIR not in sys.path:
    sys.path.insert(0, _KCONFIG_DIR)

import graph_deps as deps  # noqa: E402  # pylint: disable=wrong-import-position
from generate import generate_kconfig  # noqa: E402  # pylint: disable=wrong-import-position

kconfiglib = importlib.import_module("kconfiglib")


class ProfileRequirementResolutionTest(unittest.TestCase):
    def test_graph_and_project_profile_requirements_remain_separate(self) -> None:
        # Given one graph-mandatory Standard CU and one project-required optional CU.
        graph = {"profiles": {
            "2269": {
                "name": "Standard Profile",
                "child_profiles": [],
                "child_cus": [
                    {"id": 9001, "name": "Graph Required", "isOptional": False},
                    {"id": 9002, "name": "Project Required", "isOptional": True},
                ],
            },
            "cu_master": {},
        }}
        manifest = {"items": [
            {
                "id": "graph_required",
                "kind": "conformance_unit",
                "kconfig_symbol": "MUC_OPCUA_CU_GRAPH_REQUIRED",
                "opc_reference": {"cu_id": "9001", "cu_name": "Graph Required"},
                "implementation_state": "implemented",
            },
            {
                "id": "project_required",
                "kind": "conformance_unit",
                "kconfig_symbol": "MUC_OPCUA_CU_PROJECT_REQUIRED",
                "opc_reference": {"cu_id": "9002", "cu_name": "Project Required"},
                "implementation_state": "implemented",
                "project_profile_defaults": {"standard": True},
                "project_required_for_profile": {"standard": True},
            },
        ]}

        # When graph-derived values are resolved.
        deps.resolve_into(manifest, graph)

        # Then graph requirements remain distinct from additive project requirements.
        graph_item, project_item = manifest["items"]
        self.assertEqual(graph_item.get("required_for_profile"), {"standard": True})
        self.assertEqual(project_item.get("required_for_profile"), {})
        self.assertEqual(
            project_item["project_required_for_profile"], {"standard": True}
        )
        self.assertTrue(project_item["profile_defaults"]["standard"])


class ProfileMarkerGenerationTest(unittest.TestCase):
    def test_standard_marker_ands_every_effective_mandatory_canonical_cu(self) -> None:
        # Given graph-required, project-required, and default-only Standard CUs.
        profile_defaults = {
            "nano": False,
            "micro": False,
            "embedded": False,
            "standard": True,
            "full": True,
            "custom": False,
        }
        manifest = {
            "profiles": {},
            "advertised_profile_markers": [{
                "id": "MUC_OPCUA_MARKER_STANDARD_PROFILE",
                "required_profile": "standard",
            }],
            "items": [
                {
                    "id": "z_graph_required",
                    "kind": "conformance_unit",
                    "implementation_state": "claimed",
                    "kconfig_symbol": "MUC_OPCUA_CU_GRAPH_REQUIRED",
                    "opc_display_name": "Graph Required",
                    "profile_defaults": dict(profile_defaults),
                    "required_for_profile": {"standard": True},
                },
                {
                    "id": "m_default_only",
                    "kind": "conformance_unit",
                    "implementation_state": "claimed",
                    "kconfig_symbol": "MUC_OPCUA_CU_DEFAULT_ONLY",
                    "opc_display_name": "Default Only",
                    "profile_defaults": dict(profile_defaults),
                },
                {
                    "id": "a_project_required",
                    "kind": "conformance_unit",
                    "implementation_state": "claimed",
                    "kconfig_symbol": "MUC_OPCUA_CU_PROJECT_REQUIRED",
                    "opc_display_name": "Project Required",
                    "profile_defaults": dict(profile_defaults),
                    "project_required_for_profile": {"standard": True},
                },
            ],
            "capacities": [],
        }

        # When Kconfig is generated from the requirement model.
        kconfig = generate_kconfig(manifest)

        # Then the hidden marker is the stable AND closure, not profile default intent.
        marker_start = kconfig.index("config MUC_OPCUA_MARKER_STANDARD_PROFILE")
        marker_end = kconfig.index("\n\n", marker_start)
        marker_block = kconfig[marker_start:marker_end]
        marker_directives = [line.strip() for line in marker_block.splitlines()]
        bool_directives = [
            directive
            for directive in marker_directives
            if directive == "bool" or directive.startswith("bool ")
        ]
        self.assertEqual(bool_directives, ["bool"])
        self.assertFalse(
            any(directive.startswith("prompt ") for directive in marker_directives)
        )
        default_conditions = [
            line.strip().removeprefix("default y if ")
            for line in marker_block.splitlines()
            if line.strip().startswith("default y if ")
        ]
        self.assertEqual(len(default_conditions), 1)
        condition = default_conditions[0]
        self.assertNotIn("||", condition)
        self.assertSetEqual(
            {operand.strip() for operand in re.split(r"\s*&&\s*", condition)},
            {
                "MUC_OPCUA_CU_GRAPH_REQUIRED",
                "MUC_OPCUA_CU_PROJECT_REQUIRED",
            },
        )

        # And Kconfig resolves the marker on only while every mandatory CU is on.
        with tempfile.TemporaryDirectory() as temp_dir:
            kconfig_path = os.path.join(temp_dir, "Kconfig")
            with open(kconfig_path, "w", encoding="utf-8") as kconfig_file:
                kconfig_file.write(kconfig)
            kconf = kconfiglib.Kconfig(kconfig_path, warn=False)
            graph_required = kconf.syms["MUC_OPCUA_CU_GRAPH_REQUIRED"]
            project_required = kconf.syms["MUC_OPCUA_CU_PROJECT_REQUIRED"]
            marker = kconf.syms["MUC_OPCUA_MARKER_STANDARD_PROFILE"]

            graph_required.set_value(2)
            project_required.set_value(2)
            self.assertEqual(marker.str_value, "y")

            project_required.set_value(0)
            self.assertEqual(marker.str_value, "n")

    def test_marker_id_is_emitted_without_legacy_rename(self) -> None:
        # Given a marker whose identifier matches the former compatibility alias.
        manifest = {
            "profiles": {},
            "advertised_profile_markers": [{
                "id": "STANDARD_PROFILE",
                "required_profile": "standard",
            }],
            "items": [{
                "id": "required_cu",
                "kind": "conformance_unit",
                "implementation_state": "claimed",
                "kconfig_symbol": "MUC_OPCUA_CU_REQUIRED",
                "opc_display_name": "Required",
                "profile_defaults": {},
                "required_for_profile": {"standard": True},
            }],
            "capacities": [],
        }

        # When Kconfig is generated.
        kconfig = generate_kconfig(manifest)

        # Then the manifest identifier is emitted verbatim without compatibility remapping.
        self.assertIn("config STANDARD_PROFILE\n", kconfig)
        self.assertNotIn("config MUC_OPCUA_MARKER_STANDARD_PROFILE\n", kconfig)


if __name__ == "__main__":
    unittest.main()
