#!/usr/bin/env python3
"""Regression coverage for the maintained Kconfig/CMake baseline checker."""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.kconfig import check_baseline


class CheckBaselineTest(unittest.TestCase):
    def test_normalize_kconfig_symbol_preserves_canonical_names(self):
        self.assertEqual(
            getattr(check_baseline, "normalize_kconfig_symbol")("READ_CACHE"),
            "MUC_OPCUA_READ_CACHE",
        )
        self.assertEqual(
            getattr(check_baseline, "normalize_kconfig_symbol")(
                "MUC_OPCUA_CU_ATTRIBUTE_READ"
            ),
            "MUC_OPCUA_CU_ATTRIBUTE_READ",
        )

    def test_compiler_gate_inventory_uses_direct_canonical_kconfig_selectors(self):
        cmake_selectors = {
            "MUC_OPCUA_CU_ATTRIBUTE_READ",
            "MUC_OPCUA_FACET_GROUP_ONLY",
            "MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES",
            "MUC_OPCUA_MARKER_STANDARD_PROFILE",
            "MUC_OPCUA_PROFILE_NANO",
        }
        kconfig_bools = {
            "MUC_OPCUA_CU_ATTRIBUTE_READ",
            "MUC_OPCUA_FACET_GROUP_ONLY",
            "MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES",
            "MUC_OPCUA_MARKER_STANDARD_PROFILE",
            "MUC_OPCUA_PROFILE_NANO",
        }

        inventory = getattr(check_baseline, "compiler_gate_inventory")(
            cmake_selectors,
            kconfig_bools,
        )

        self.assertEqual(
            inventory,
            {
                "MUC_OPCUA_CU_ATTRIBUTE_READ",
                "MUC_OPCUA_FACET_GROUP_ONLY",
                "MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES",
                "MUC_OPCUA_MARKER_STANDARD_PROFILE",
            },
        )

    def test_compare_gate_projection_reports_missing_and_unexpected_gates(self):
        projection = getattr(check_baseline, "compare_gate_projection")(
            {"MUC_OPCUA_CU_ATTRIBUTE_READ", "MUC_OPCUA_CU_VIEW_BASIC_2"},
            {"MUC_OPCUA_CU_ATTRIBUTE_READ", "MUC_OPCUA_CU_BROWSE"},
        )

        self.assertEqual(
            projection,
            {
                "missing": {"MUC_OPCUA_CU_VIEW_BASIC_2"},
                "unexpected": {"MUC_OPCUA_CU_BROWSE"},
            },
        )

    def test_compile_definition_zero_is_drift_and_one_is_enabled(self):
        expected = {"MUC_OPCUA_CU_ATTRIBUTE_READ"}
        with self.assertRaises(ValueError):
            getattr(check_baseline, "enabled_compiler_gates")(
                getattr(check_baseline, "parse_compile_definitions")(
                    "-DMUC_OPCUA_CU_ATTRIBUTE_READ=0"
                ),
                expected,
            )
        enabled = getattr(check_baseline, "enabled_compiler_gates")(
            getattr(check_baseline, "parse_compile_definitions")(
                "-DMUC_OPCUA_CU_ATTRIBUTE_READ=1"
            ),
            expected,
        )

        self.assertEqual(enabled, expected)

    def test_compiler_gate_names_keeps_public_kconfig_bools_in_domain(self):
        kconfig_bools = {
            "MUC_OPCUA_CU_ATTRIBUTE_READ",
            "MUC_OPCUA_PROFILE_NANO",
            "MUC_OPCUA_INTERN_STORAGE",
        }

        gate_names = getattr(check_baseline, "compiler_gate_names")(
            {"MUC_OPCUA_CU_ATTRIBUTE_READ"},
            kconfig_bools,
        )

        self.assertEqual(gate_names, {"MUC_OPCUA_CU_ATTRIBUTE_READ"})

    def test_compiler_gate_names_does_not_depend_on_source_declarations(self):
        gate = "MUC_OPCUA_ALLOW_HEAP"
        gate_names = getattr(check_baseline, "compiler_gate_names")(
            {gate},
            {gate},
        )

        self.assertEqual(gate_names, {gate})

    def test_enabled_compiler_gates_accepts_bare_and_one_values(self):
        gate = "MUC_OPCUA_CU_ATTRIBUTE_READ"
        gate_domain = {gate}

        self.assertEqual(
            getattr(check_baseline, "enabled_compiler_gates")({gate: None}, gate_domain),
            {gate},
        )
        self.assertEqual(
            getattr(check_baseline, "enabled_compiler_gates")({gate: "1"}, gate_domain),
            {gate},
        )

    def test_enabled_compiler_gates_rejects_noncanonical_values(self):
        gate = "MUC_OPCUA_CU_ATTRIBUTE_READ"
        for value in ("0", "2", "OFF"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                getattr(check_baseline, "enabled_compiler_gates")(
                    {gate: value},
                    {gate},
                )

    def test_sentinel_compile_definitions_parses_arguments_array(self):
        with tempfile.TemporaryDirectory() as builddir:
            (Path(builddir) / "compile_commands.json").write_text(
                json.dumps([{"file": "/tmp/src/core/server/init.c", "arguments": [
                    "cc", "-DMUC_OPCUA_CU_ATTRIBUTE_READ=1"]}]),
                encoding="utf-8",
            )

            self.assertEqual(
                getattr(check_baseline, "_sentinel_compile_definitions")(builddir),
                {"MUC_OPCUA_CU_ATTRIBUTE_READ": "1"},
            )

    def test_sentinel_compile_definitions_parses_command_string(self):
        with tempfile.TemporaryDirectory() as builddir:
            (Path(builddir) / "compile_commands.json").write_text(
                json.dumps([{"file": "/tmp/src/core/server/init.c",
                             "command": "cc -DMUC_OPCUA_CU_ATTRIBUTE_READ=1"}]),
                encoding="utf-8",
            )

            self.assertEqual(
                getattr(check_baseline, "_sentinel_compile_definitions")(builddir),
                {"MUC_OPCUA_CU_ATTRIBUTE_READ": "1"},
            )

    def test_sentinel_compile_definitions_requires_sentinel(self):
        with tempfile.TemporaryDirectory() as builddir:
            (Path(builddir) / "compile_commands.json").write_text(
                json.dumps([{"file": "/tmp/src/core/server/other.c", "command": "cc"}]),
                encoding="utf-8",
            )

            with self.assertRaises(ValueError):
                getattr(check_baseline, "_sentinel_compile_definitions")(builddir)

    def test_sentinel_compile_definitions_requires_recognized_definitions(self):
        with tempfile.TemporaryDirectory() as builddir:
            (Path(builddir) / "compile_commands.json").write_text(
                json.dumps([{"file": "/tmp/src/core/server/init.c",
                             "command": "cc -DOTHER_SYMBOL=1"}]),
                encoding="utf-8",
            )

            with self.assertRaises(ValueError):
                getattr(check_baseline, "_sentinel_compile_definitions")(builddir)

    def test_empty_global_inventory_fails_closed_but_custom_empty_projection_is_valid(self):
        with self.assertRaises(ValueError):
            getattr(check_baseline, "validate_gate_inventory")(set())

        getattr(check_baseline, "validate_gate_inventory")(
            {"MUC_OPCUA_CU_ATTRIBUTE_READ"}
        )
        self.assertEqual(
            getattr(check_baseline, "compare_gate_projection")(set(), set()),
            {"missing": set(), "unexpected": set()},
        )

    def test_kconfig_flags_use_canonical_prefixed_symbols(self):
        previous_config = os.environ.get("CONFIG_")
        previous_pythonpath = os.environ.get("PYTHONPATH")
        try:
            os.environ["CONFIG_"] = "AMBIENT_"
            os.environ["PYTHONPATH"] = str(ROOT / "scripts" / "kconfig")

            symbols = check_baseline.kconfig_flags(str(ROOT), "python3", "nano")

            canonical = "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER"
            self.assertIn(canonical, symbols)
            self.assertFalse(any(symbol.startswith("MUC_OPCUA_MUC_OPCUA_") for symbol in symbols))
        finally:
            if previous_config is None:
                os.environ.pop("CONFIG_", None)
            else:
                os.environ["CONFIG_"] = previous_config
            if previous_pythonpath is None:
                os.environ.pop("PYTHONPATH", None)
            else:
                os.environ["PYTHONPATH"] = previous_pythonpath

    def test_kconfig_flags_normalize_bare_symbols(self):
        previous_config = os.environ.get("CONFIG_")
        previous_pythonpath = os.environ.get("PYTHONPATH")
        try:
            os.environ["CONFIG_"] = "AMBIENT_"
            os.environ["PYTHONPATH"] = str(ROOT / "scripts" / "kconfig")

            symbols = check_baseline.kconfig_flags(str(ROOT), "python3", "micro")

            self.assertIn("MUC_OPCUA_SECURE_CHANNEL_CRYPTO", symbols)
            self.assertNotIn("SECURE_CHANNEL_CRYPTO", symbols)
        finally:
            if previous_config is None:
                os.environ.pop("CONFIG_", None)
            else:
                os.environ["CONFIG_"] = previous_config
            if previous_pythonpath is None:
                os.environ.pop("PYTHONPATH", None)
            else:
                os.environ["PYTHONPATH"] = previous_pythonpath

    def test_kconfig_flags_include_full_profile_mdns(self):
        previous_config = os.environ.get("CONFIG_")
        previous_pythonpath = os.environ.get("PYTHONPATH")
        try:
            os.environ["CONFIG_"] = "AMBIENT_"
            os.environ["PYTHONPATH"] = str(ROOT / "scripts" / "kconfig")

            symbols = check_baseline.kconfig_flags(str(ROOT), "python3", "full")

            self.assertIn("MUC_OPCUA_MDNS_DISCOVERY", symbols)
        finally:
            if previous_config is None:
                os.environ.pop("CONFIG_", None)
            else:
                os.environ["CONFIG_"] = previous_config
            if previous_pythonpath is None:
                os.environ.pop("PYTHONPATH", None)
            else:
                os.environ["PYTHONPATH"] = previous_pythonpath

    def test_kconfig_flags_resolves_vendored_kconfiglib_without_pythonpath(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("PYTHONPATH", None)

            symbols = check_baseline.kconfig_flags(
                str(ROOT),
                sys.executable,
                "nano",
            )

        self.assertIn(
            "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER",
            symbols,
        )


if __name__ == "__main__":
    unittest.main()
