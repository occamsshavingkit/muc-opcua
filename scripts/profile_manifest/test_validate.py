#!/usr/bin/env python3
"""Focused tests for profile_manifest.validate helper checks."""

from __future__ import annotations

import copy
import importlib.util
import pathlib
import unittest

_VALIDATE_PATH = pathlib.Path(__file__).with_name("validate.py")
_SPEC = importlib.util.spec_from_file_location("profile_manifest_validate", _VALIDATE_PATH)
assert _SPEC is not None and _SPEC.loader is not None
validate = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(validate)


def _manifest_with_item(**overrides: object) -> dict:
    item = {
        "id": "opc_cu_2317",
        "kind": "conformance_unit",
        "implementation_state": "implemented",
        "kconfig_symbol": "MUC_OPCUA_CU_VIEW_BASIC_TRANSLATE_BROWSE_PATHS",
        "backing_tests": ["tests/unit/test_browse_service.c"],
        "profile_defaults": {
            "nano": True,
            "micro": True,
            "embedded": True,
            "standard": True,
            "full": True,
            "custom": False,
        },
    }
    item.update(overrides)
    return {"items": [item]}


class ValidateInScopeImplementedCuRequirementsTest(unittest.TestCase):
    def test_manifest_helper_ignores_malformed_items_without_raising(self) -> None:
        for malformed_items in (None, "not-a-list", {"id": "opc_cu_2317"}):
            with self.subTest(items=malformed_items):
                errors = validate._check_in_scope_071_manifest_requirements(
                    {"items": malformed_items}
                )

                self.assertEqual(errors, [])

    def test_manifest_helper_rejects_in_scope_implemented_cu_without_required_evidence(self) -> None:
        manifest = _manifest_with_item(kconfig_symbol="", backing_tests=[])

        errors = validate._check_in_scope_071_manifest_requirements(manifest)

        self.assertTrue(
            any("opc_cu_2317" in error and "kconfig_symbol" in error for error in errors),
            errors,
        )
        self.assertTrue(
            any("opc_cu_2317" in error and "backing_tests" in error for error in errors),
            errors,
        )

    def test_check_claims_rejects_in_scope_implemented_cu_without_backing_tests(self) -> None:
        manifest = _manifest_with_item(backing_tests=[])

        errors = validate._check_claims(manifest)

        self.assertTrue(
            any("opc_cu_2317" in error and "backing_tests" in error for error in errors),
            errors,
        )

    def test_check_claims_rejects_in_scope_implemented_cu_without_kconfig_symbol(self) -> None:
        manifest = _manifest_with_item(kconfig_symbol="")

        errors = validate._check_claims(manifest)

        self.assertTrue(
            any("opc_cu_2317" in error and "kconfig_symbol" in error for error in errors),
            errors,
        )

    def test_t007_helper_accepts_in_scope_implemented_cu_with_symbol_and_backing_tests(self) -> None:
        item = copy.deepcopy(_manifest_with_item()["items"][0])

        errors = validate._check_in_scope_071_implemented_cu_requirements(item)

        self.assertEqual(errors, [])

    def test_reconciliation_rejects_satisfied_by_unimplemented_target(self) -> None:
        manifest = {
            "profiles": {"nano": {}},
            "items": [
                {"id": "opc_cu_a", "kind": "conformance_unit", "satisfied_by": "opc_cu_b",
                 "implementation_state": "unimplemented"},
                {"id": "opc_cu_b", "kind": "conformance_unit", "implementation_state": "unimplemented"},
            ],
        }
        errors = validate._check_reconciliation_links(manifest)
        self.assertTrue(any("satisfied_by" in e and "not implemented" in e for e in errors), errors)

    def test_reconciliation_rejects_unknown_target_and_ungrounded_na(self) -> None:
        manifest = {
            "profiles": {"nano": {}},
            "items": [
                {"id": "opc_cu_a", "kind": "conformance_unit", "satisfied_by": "nope",
                 "implementation_state": "unimplemented"},
                {"id": "opc_cu_c", "kind": "conformance_unit", "implementation_state": "unimplemented",
                 "not_applicable": {"nano": ""}},
                {"id": "opc_cu_d", "kind": "conformance_unit", "implementation_state": "unimplemented",
                 "not_applicable": {"bogus": "reason"}},
            ],
        }
        errors = validate._check_reconciliation_links(manifest)
        self.assertTrue(any("unknown CU" in e for e in errors), errors)
        self.assertTrue(any("grounded reason" in e for e in errors), errors)
        self.assertTrue(any("unknown profile" in e for e in errors), errors)

    def test_reconciliation_accepts_valid_links(self) -> None:
        manifest = {
            "profiles": {"nano": {}},
            "items": [
                {"id": "opc_cu_a", "kind": "conformance_unit", "satisfied_by": "opc_cu_b",
                 "implementation_state": "unimplemented"},
                {"id": "opc_cu_b", "kind": "conformance_unit", "implementation_state": "claimed"},
                {"id": "opc_cu_c", "kind": "conformance_unit", "implementation_state": "unimplemented",
                 "not_applicable": {"nano": "grounded reason"}},
            ],
        }
        self.assertEqual(validate._check_reconciliation_links(manifest), [])


if __name__ == "__main__":
    unittest.main()
