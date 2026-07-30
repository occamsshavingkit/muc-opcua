#!/usr/bin/env python3
from __future__ import annotations

import os
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from generate import generate_kconfig, validate_manifest  # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
