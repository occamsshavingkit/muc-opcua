#!/usr/bin/env python3
"""RED regression coverage for baseline-checker boundary behavior."""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Final
from unittest import mock

ROOT: Final = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.kconfig import check_baseline


NANO_PROFILE_SYMBOL: Final = "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER"


class CheckBaselineBoundaryTest(unittest.TestCase):
    def test_kconfig_domain_keeps_gate_omitted_from_source_definitions(self) -> None:
        # Given: a public Kconfig gate has been accidentally omitted from CMake.
        omitted_gate = "MUC_OPCUA_CU_ATTRIBUTE_READ"
        cmake_source = f"if({omitted_gate})\nendif()"

        # When: the expected compiler-gate domain is derived.
        selectors = check_baseline.parse_cmake_gate_selectors(cmake_source)
        gate_names = check_baseline.compiler_gate_names(selectors, {omitted_gate})
        declaration_projection = check_baseline.compare_gate_projection(
            gate_names,
            set(check_baseline.parse_compile_definitions(cmake_source)),
        )

        # Then: the omission remains visible for the projection comparison.
        self.assertEqual(declaration_projection["missing"], {omitted_gate})

    def test_compile_database_preserves_explicit_empty_gate_values(self) -> None:
        # Given: both compile database forms include an empty gate plus a recognized value.
        gate = "MUC_OPCUA_CU_ATTRIBUTE_READ"
        recognized = "MUC_OPCUA_ALLOW_HEAP"
        compiler_forms = (
            (
                "arguments",
                {
                    "arguments": [
                        "cc",
                        f"-D{gate}=",
                        f"-D{recognized}=0",
                        "-c",
                        "/tmp/src/core/server/init.c",
                    ]
                },
            ),
            (
                "command",
                {
                    "command": (
                        f"cc -D{gate}= -D{recognized}=0 "
                        "-c /tmp/src/core/server/init.c"
                    )
                },
            ),
        )

        for form_name, compiler_form in compiler_forms:
            with self.subTest(form=form_name), tempfile.TemporaryDirectory() as builddir:
                compile_database = [
                    {"file": "/tmp/src/core/server/init.c", **compiler_form}
                ]
                (Path(builddir) / "compile_commands.json").write_text(
                    json.dumps(compile_database),
                    encoding="utf-8",
                )

                # When: the sentinel command's definitions are parsed.
                definitions = check_baseline._sentinel_compile_definitions(builddir)

                # Then: empty remains distinct from bare, and is rejected as noncanonical.
                self.assertEqual(definitions.get(gate), "")
                with self.assertRaises(ValueError):
                    check_baseline.enabled_compiler_gates(definitions, {gate})

    def test_generated_compiler_tokens_preserve_unresolved_gate_values(self) -> None:
        # Given: a generated compiler token with a literal gate and unresolved value.
        gate = "MUC_OPCUA_CU_ATTRIBUTE_READ"
        generator_value = "$<BOOL:1>"
        token = f"-D{gate}={generator_value}"

        # When: the resolved compiler command is parsed.
        definitions = check_baseline._parse_compiler_tokens([token])

        # Then: the raw value remains visible and strict gate validation rejects it.
        self.assertEqual(definitions, {gate: generator_value})
        with self.assertRaises(ValueError):
            check_baseline.enabled_compiler_gates(definitions, {gate})

    def test_kconfig_flags_isolates_child_from_caller_plugins(self) -> None:
        # Given: the caller advertises an external Kconfig function module that raises.
        with tempfile.TemporaryDirectory() as caller_directory:
            Path(caller_directory, "maliciousfunctions.py").write_text(
                "raise RuntimeError('caller Kconfig function imported')\n",
                encoding="utf-8",
            )
            caller_environment = {
                "PYTHONPATH": caller_directory,
                "KCONFIG_FUNCTIONS": "maliciousfunctions",
            }

            # When: the baseline checker evaluates the vendored nano Kconfig.
            with mock.patch.dict(os.environ, caller_environment, clear=False):
                symbols = check_baseline.kconfig_flags(
                    str(ROOT),
                    sys.executable,
                    "nano",
                )

        # Then: caller plugins cannot affect the child and the nano profile resolves.
        self.assertIn(NANO_PROFILE_SYMBOL, symbols)


if __name__ == "__main__":
    unittest.main()
