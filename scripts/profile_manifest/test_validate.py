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


class ValidateGeneratedKconfigVisibilityPolicyTest(unittest.TestCase):
    def test_generated_kconfig_policy_rejects_obsolete_helper_prefixes(self) -> None:
        for obsolete_prefix in (
            "MUC_OPCUA_PROFILE_PRESET_",
            "MUC_OPCUA_FACETS_MATCH_",
        ):
            with self.subTest(obsolete_prefix=obsolete_prefix):
                generated_kconfig = (
                    "config " + obsolete_prefix + "STANDARD\n"
                    '\tbool "Obsolete generated helper"\n'
                )

                errors = validate._check_generated_kconfig_structure(
                    _manifest_with_item(),
                    generated_kconfig,
                )

                self.assertTrue(
                    any(obsolete_prefix in error for error in errors),
                    errors,
                )

    def test_generated_kconfig_policy_requires_unimplemented_comment(self) -> None:
        item_id = "opc_cu_missing_comment"
        display_name = "Missing Comment Conformance Unit"
        manifest = _manifest_with_item(
            id=item_id,
            implementation_state="unimplemented",
            opc_display_name=display_name,
            kconfig_symbol="MUC_OPCUA_CU_MISSING_COMMENT",
        )

        errors = validate._check_generated_kconfig_structure(manifest, "")

        self.assertTrue(
            any(
                item_id in error and "NOT IMPLEMENTED" in error
                for error in errors
            ),
            errors,
        )

    def test_generated_kconfig_policy_requires_unimplemented_comment_source(self) -> None:
        item_id = "opc_cu_missing_comment_source"
        display_name = "Missing Comment Source Conformance Unit"
        manifest = _manifest_with_item(
            id=item_id,
            implementation_state="unimplemented",
            opc_display_name=display_name,
            kconfig_symbol="MUC_OPCUA_CU_MISSING_COMMENT_SOURCE",
            opc_reference={
                "cu_id": "9999",
                "cu_name": display_name,
                "section": "7.1.3",
                "spec": "OPC-10000-6",
            },
        )
        generated_kconfig = (
            'comment "' + display_name + ' (NOT IMPLEMENTED)"\n'
        )

        errors = validate._check_generated_kconfig_structure(
            manifest,
            generated_kconfig,
        )

        self.assertTrue(
            any(item_id in error and "OPC source" in error for error in errors),
            errors,
        )

    def test_generated_kconfig_policy_rejects_incomplete_cu_identity(self) -> None:
        for state in ("unimplemented", "deferred"):
            with self.subTest(state=state):
                # Given an incomplete OPC CU whose symbol and prompt leaked into Kconfig.
                item_id = "opc_cu_hidden_" + state
                display_name = "Hidden " + state.title() + " Conformance Unit"
                symbol = "MUC_OPCUA_CU_HIDDEN_" + state.upper()
                manifest = _manifest_with_item(
                    id=item_id,
                    implementation_state=state,
                    opc_display_name=display_name,
                    kconfig_symbol=symbol,
                )
                generated_kconfig = (
                    "config " + symbol + "\n"
                    "\tbool \"CU: " + display_name + "\"\n"
                )

                # When the generated-Kconfig structural policy is checked.
                errors = validate._check_generated_kconfig_structure(
                    manifest,
                    generated_kconfig,
                )

                # Then both leaked forms identify the actual incomplete manifest item.
                self.assertTrue(
                    any(item_id in error and symbol in error for error in errors),
                    errors,
                )
                self.assertTrue(
                    any(item_id in error and display_name in error for error in errors),
                    errors,
                )

    def test_generated_kconfig_policy_ignores_incomplete_substring_prompt(self) -> None:
        # Given an incomplete item whose display name is only part of a selectable prompt.
        item_id = "opc_cu_3188"
        display_name = "Base Info Base Types"
        manifest = _manifest_with_item(
            id=item_id,
            implementation_state="unimplemented",
            opc_display_name=display_name,
            kconfig_symbol="MUC_OPCUA_CU_BASE_INFO_BASE_TYPES",
        )
        generated_kconfig = (
            'config MUC_OPCUA_CU_BASE_INFO_BASE_TYPES_CORE_TYPES_FOLDERS\n'
            '\tbool "CU: Base Info Base Types + Core Types Folders"\n'
            'comment "Base Info Base Types (NOT IMPLEMENTED)"\n'
        )

        # When the generated-Kconfig structural policy is checked.
        errors = validate._check_generated_kconfig_structure(manifest, generated_kconfig)

        # Then the selectable item's longer prompt is not attributed to the incomplete item.
        self.assertFalse(any(item_id in error for error in errors), errors)

    def test_generated_kconfig_policy_ignores_exact_prompt_owned_by_selectable_item(self) -> None:
        # Given an incomplete item sharing a display name with a claimed selectable item.
        incomplete_id = "opc_cu_3072"
        claimed_item = _manifest_with_item(
            id="service_read",
            implementation_state="claimed",
            opc_display_name="Attribute Read",
            kconfig_symbol="MUC_OPCUA_CU_ATTRIBUTE_READ",
        )["items"][0]
        incomplete_item = copy.deepcopy(claimed_item)
        incomplete_item.update(
            {
                "id": incomplete_id,
                "implementation_state": "unimplemented",
                "kconfig_symbol": "MUC_OPCUA_CU_ATTRIBUTE_READ_INCOMPLETE",
            }
        )
        manifest = {"items": [incomplete_item, claimed_item]}
        generated_kconfig = (
            'config MUC_OPCUA_CU_ATTRIBUTE_READ\n'
            '\tbool "CU: Attribute Read"\n'
            'comment "Attribute Read (NOT IMPLEMENTED)"\n'
        )

        # When the generated-Kconfig structural policy is checked.
        errors = validate._check_generated_kconfig_structure(manifest, generated_kconfig)

        # Then the exact prompt is owned by the claimed item, not the incomplete item.
        self.assertFalse(any(incomplete_id in error for error in errors), errors)




if __name__ == "__main__":
    unittest.main()
