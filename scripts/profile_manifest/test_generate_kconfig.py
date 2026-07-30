#!/usr/bin/env python3
from __future__ import annotations

import os
import sys
import tempfile
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from generate import (  # noqa: E402  # pylint: disable=wrong-import-position
    generate_build_docs_section,
    generate_kconfig,
    update_build_docs,
)
from model import validate_manifest  # noqa: E402  # pylint: disable=wrong-import-position


_PROFILE_DEFAULTS = {
    "nano": False,
    "micro": False,
    "embedded": False,
    "standard": False,
    "full": False,
    "custom": False,
}


def _profiles() -> dict[str, dict]:
    profiles: dict[str, dict] = {}
    for key in ("nano", "micro", "embedded", "standard"):
        profiles[key] = {
            "id": "MUC_OPCUA_PROFILE_" + key.upper(),
            "display": key.title(),
            "selectable": True,
            "release_status_name": "active",
            "opc_display_name": key.title() + " 2025 UA Server Profile",
        }
    profiles["full"] = {
        "id": "MUC_OPCUA_PROFILE_FULL",
        "display": "Full",
        "selectable": True,
        "opc_display_name": "Full UA Server Profile",
    }
    profiles["custom"] = {
        "id": "MUC_OPCUA_PROFILE_CUSTOM",
        "display": "Custom",
        "selectable": True,
    }
    return profiles


class GenerateKconfigTest(unittest.TestCase):
    def test_documented_cu_is_visible_inside_selectable_facet_without_symbol(self) -> None:
        facet_defaults = dict(_PROFILE_DEFAULTS)
        facet_defaults["nano"] = True
        manifest = {
            "schema_version": 1,
            "profiles": _profiles(),
            "items": [
                {
                    "id": "test_facet",
                    "kind": "facet",
                    "implementation_state": "implemented",
                    "kconfig_symbol": "MUC_OPCUA_FACET_TEST_SERVER",
                    "opc_display_name": "Test Server Facet",
                    "opc_reference": {
                        "spec": "OPC-10000-7",
                        "section": "4.2",
                    },
                    "profile_defaults": facet_defaults,
                },
                {
                    "id": "documented_cu",
                    "kind": "conformance_unit",
                    "implementation_state": "documented",
                    "kconfig_symbol": "MUC_OPCUA_CU_DOCUMENTED_CAPABILITY",
                    "opc_display_name": "Documented Capability",
                    "opc_reference": {
                        "spec": "OPC-10000-7",
                        "section": "6.4",
                        "cu_id": "9999",
                        "cu_name": "Documented Capability",
                    },
                    "notes": "Satisfied by shipped documentation.",
                    "profile_defaults": dict(_PROFILE_DEFAULTS),
                },
            ],
            "capacities": [],
            "facet_containment": {"test_facet": ["documented_cu"]},
        }

        self.assertEqual(validate_manifest(manifest), [])
        kconfig = generate_kconfig(manifest)
        documented_comment = (
            'comment "Documented Capability (DOCUMENTED) '
            '[OPC-10000-7 §6.4]"'
        )

        self.assertIn(documented_comment, kconfig)
        self.assertIn(
            "#   Implementation state: documented -- visible but not selectable.",
            kconfig,
        )
        self.assertIn(
            "#   Detail: CU 9999:Documented Capability.",
            kconfig,
        )

        facet_start = kconfig.index("menuconfig MUC_OPCUA_FACET_TEST_SERVER")
        facet_end = kconfig.index("\nendif\n", facet_start)
        documented_position = kconfig.index(documented_comment)
        self.assertLess(facet_start, documented_position)
        self.assertLess(documented_position, facet_end)
        self.assertNotRegex(
            kconfig,
            r"(?m)^config MUC_OPCUA_CU_DOCUMENTED_CAPABILITY$",
        )

    def test_documented_cu_without_facet_is_visible_flat_without_symbol(self) -> None:
        manifest = {
            "schema_version": 1,
            "profiles": _profiles(),
            "items": [
                {
                    "id": "flat_documented_cu",
                    "kind": "conformance_unit",
                    "implementation_state": "documented",
                    "kconfig_symbol": "MUC_OPCUA_CU_FLAT_DOCUMENTED_CAPABILITY",
                    "opc_display_name": "Flat Documented Capability",
                    "opc_reference": {
                        "spec": "OPC-10000-7",
                        "section": "6.5",
                        "cu_id": "9998",
                        "cu_name": "Flat Documented Capability",
                    },
                    "notes": "Satisfied by shipped documentation.",
                    "profile_defaults": dict(_PROFILE_DEFAULTS),
                },
            ],
            "capacities": [],
        }

        self.assertEqual(validate_manifest(manifest), [])
        kconfig = generate_kconfig(manifest)
        flat_section = (
            'comment "Documented/unimplemented OPC items '
            '(visible but not selectable)"'
        )
        documented_comment = (
            'comment "Flat Documented Capability (DOCUMENTED) '
            '[OPC-10000-7 §6.5]"'
        )

        section_start = kconfig.index(flat_section)
        section_end = kconfig.index("\nendmenu\n", section_start)
        documented_position = kconfig.index(documented_comment)
        self.assertLess(section_start, documented_position)
        self.assertLess(documented_position, section_end)
        self.assertNotRegex(
            kconfig,
            r"(?m)^config MUC_OPCUA_CU_FLAT_DOCUMENTED_CAPABILITY$",
        )


class GenerateBuildDocsTest(unittest.TestCase):
    def test_unavailable_item_notes_escape_markdown_table_pipe(self) -> None:
        manifest = {
            "items": [
                {
                    "id": "documented_cu",
                    "implementation_state": "documented",
                    "opc_reference": {
                        "spec": "OPC-10000-7",
                        "section": "6.4",
                    },
                    "notes": "Documented capability | satisfied elsewhere.",
                },
            ],
            "capacities": [],
        }

        section = generate_build_docs_section(manifest)

        self.assertIn(
            "| documented_cu | OPC-10000-7 §6.4 | documented | "
            r"Documented capability \| satisfied elsewhere. |",
            section,
        )

    def test_update_build_docs_is_byte_idempotent(self) -> None:
        manifest = {"items": [], "capacities": []}

        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "build.md")
            with open(path, "w", encoding="utf-8") as fh:
                fh.write("# Build\n\n## Verifying gating behavior\n\nBody\n")

            update_build_docs(manifest, path)
            with open(path, "rb") as fh:
                first = fh.read()

            update_build_docs(manifest, path)
            with open(path, "rb") as fh:
                second = fh.read()

        self.assertEqual(first, second)
        self.assertIn(
            b"<!-- END GENERATED MANIFEST TABLES -->\n\n"
            b"## Verifying gating behavior",
            second,
        )


if __name__ == "__main__":
    unittest.main()
