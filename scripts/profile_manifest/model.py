"""Manifest loader and validator for the OPC-complete Kconfig manifest.

Standard-library only. The manifest is stored as JSON content with a .yaml
extension (see profiles/opcua-profile-manifest.yaml format_note) so json.load
parses it without requiring PyYAML.
"""

from __future__ import annotations

import json
import os
from typing import Any


_ALLOWED_IMPLEMENTATION_STATES = ("unimplemented", "deferred", "implemented", "claimed")
_ALLOWED_KINDS = ("profile", "facet", "conformance_unit", "optimization")
_ALLOWED_INTERNAL_CLASSIFICATIONS = ("keep_internal", "retire_internal")
_ALLOWED_CAPACITY_KINDS = ("profile_varying", "invariant", "derived")
_DEPENDS_ON_OPS = ("and", "or")
_DEFAULT_PROFILES = ("nano", "micro", "embedded", "standard", "full", "custom")

def load_manifest(path: str) -> dict:
    """Load the manifest at *path* and return it as a dict.

    The manifest is JSON-formatted YAML, so json.load handles it. Raises
    FileNotFoundError if missing and ValueError on a malformed document.
    """
    if not os.path.isfile(path):
        raise FileNotFoundError(f"manifest not found: {path}")
    with open(path, "r", encoding="utf-8") as fh:
        data = fh.read()
    try:
        manifest = json.loads(data)
    except json.JSONDecodeError as exc:
        raise ValueError(f"manifest {path} is not valid JSON/YAML: {exc}") from exc
    if not isinstance(manifest, dict):
        raise ValueError(f"manifest {path} must decode to a JSON object at top level")
    return manifest


def _err(errors: list[str], msg: str) -> None:
    errors.append(msg)


def _require_keys(
    errors: list[str],
    obj: dict,
    required: tuple[str, ...],
    context: str,
) -> None:
    for key in required:
        if key not in obj:
            _err(errors, f"{context}: missing required key '{key}'")


def _is_bool(value: Any) -> bool:
    return isinstance(value, bool)


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def validate_manifest(manifest: dict) -> list[str]:
    """Return a list of human-readable validation errors.

    An empty list means the manifest is valid. Covers:
      - schema_version presence
      - profiles section: ids and required fields
      - items: required keys, valid implementation_state / kind,
        kconfig_symbol required for build-gated claimed items,
        backing_tests required for claimed items,
        depends_on referencing known symbols,
        profile_defaults completeness
      - capacities: required keys, internal classification, defaults
        for every profile
      - facet_containment (when present): keys reference facet items
        and values reference conformance_unit items; each CU must appear
        in exactly one facet list
    """
    errors: list[str] = []

    if not isinstance(manifest, dict):
        return ["manifest root must be a dict/object"]

    _require_keys(errors, manifest, ("schema_version", "profiles", "items", "capacities"), "manifest")

    profiles = manifest.get("profiles")
    if not isinstance(profiles, dict):
        _err(errors, "manifest.profiles must be an object")
        profiles = {}

    known_profile_keys = set(profiles.keys())
    expected_profiles = set(_DEFAULT_PROFILES)
    missing_profiles = expected_profiles - known_profile_keys
    if missing_profiles:
        _err(
            errors,
            "manifest.profiles missing required profile ids: "
            + ", ".join(sorted(missing_profiles)),
        )

    profile_ids: dict[str, str] = {}
    for profile_key, profile in profiles.items():
        if not isinstance(profile, dict):
            _err(errors, f"profile '{profile_key}' must be an object")
            continue
        _require_keys(errors, profile, ("id", "display", "selectable"), f"profile '{profile_key}'")
        pid = profile.get("id")
        if not isinstance(pid, str) or not pid:
            _err(errors, f"profile '{profile_key}': id must be a non-empty string")
        else:
            if pid in profile_ids:
                _err(
                    errors,
                    f"profile '{profile_key}': duplicate profile id '{pid}' "
                    f"(also declared by '{profile_ids[pid]}')",
                )
            profile_ids[pid] = profile_key

        release_status_name = profile.get("release_status_name")
        if release_status_name is not None and release_status_name not in ("active", "superseded", "unknown"):
            _err(
                errors,
                f"profile '{profile_key}': release_status_name '{release_status_name}' is invalid",
            )
        if profile_key in ("nano", "micro", "embedded", "standard"):
            if profile.get("release_status_name") != "active":
                _err(errors, f"profile '{profile_key}': canonical named profile must be active")
            display = profile.get("opc_display_name")
            if not isinstance(display, str) or "2025" not in display:
                _err(errors, f"profile '{profile_key}': canonical named profile must use 2025 display name")

    items = manifest.get("items")
    if not isinstance(items, list):
        _err(errors, "manifest.items must be a list")
        items = []

    seen_item_ids: set[str] = set()
    item_kinds: dict[str, str] = {}
    seen_item_kconfig: dict[str, str] = {}

    # Pre-pass: collect every kconfig_symbol declared by any item so that
    # depends_on entries can be validated against the full set regardless of
    # ordering. Entries are kept as Kconfig symbol names (e.g. "BASE_NODES")
    # because that is the shape the seed manifest uses.
    known_kconfig_symbols: set[str] = set()
    for item in items:
        if isinstance(item, dict):
            sym = item.get("kconfig_symbol")
            if isinstance(sym, str) and sym:
                known_kconfig_symbols.add(sym)

    for idx, item in enumerate(items):
        ctx = f"items[{idx}]"
        if not isinstance(item, dict):
            _err(errors, f"{ctx}: must be an object")
            continue

        _require_keys(
            errors,
            item,
            ("id", "kind", "implementation_state", "profile_defaults"),
            ctx,
        )

        item_id = item.get("id")
        if not isinstance(item_id, str) or not item_id:
            _err(errors, f"{ctx}: id must be a non-empty string")
            item_id = f"<{idx}>"
        elif item_id in seen_item_ids:
            _err(errors, f"{ctx}: duplicate item id '{item_id}'")
        else:
            seen_item_ids.add(item_id)

        kind = item.get("kind")
        if kind not in _ALLOWED_KINDS:
            _err(
                errors,
                f"item '{item_id}': kind '{kind}' is not one of "
                f"{list(_ALLOWED_KINDS)}",
            )

        if item_id in seen_item_ids and item_id not in item_kinds:
            item_kinds[item_id] = str(kind)

        state = item.get("implementation_state")
        if state not in _ALLOWED_IMPLEMENTATION_STATES:
            _err(
                errors,
                f"item '{item_id}': implementation_state '{state}' is not one of "
                f"{list(_ALLOWED_IMPLEMENTATION_STATES)}",
            )

        kconfig_symbol = item.get("kconfig_symbol")
        if state in ("claimed", "implemented"):
            if kconfig_symbol is None or not isinstance(kconfig_symbol, str) or not kconfig_symbol:
                _err(
                    errors,
                    f"item '{item_id}': build-gated {state} item must declare a "
                    f"non-empty kconfig_symbol",
                )

        if kconfig_symbol is not None:
            if not isinstance(kconfig_symbol, str) or not kconfig_symbol:
                _err(errors, f"item '{item_id}': kconfig_symbol must be a non-empty string when present")
            elif kconfig_symbol in seen_item_kconfig:
                _err(
                    errors,
                    f"item '{item_id}': kconfig_symbol '{kconfig_symbol}' already "
                    f"used by item '{seen_item_kconfig[kconfig_symbol]}'",
                )
            else:
                seen_item_kconfig[kconfig_symbol] = item_id

        backing_tests = item.get("backing_tests", [])
        if backing_tests is None:
            backing_tests = []
        if not isinstance(backing_tests, list):
            _err(errors, f"item '{item_id}': backing_tests must be a list when present")
            backing_tests = []
        else:
            for t in backing_tests:
                if not isinstance(t, str) or not t:
                    _err(errors, f"item '{item_id}': backing_tests entries must be non-empty strings")

        if state == "claimed" and not backing_tests:
            _err(
                errors,
                f"item '{item_id}': claimed item must list at least one backing_tests entry",
            )

        depends_on = item.get("depends_on", [])
        if depends_on is None:
            depends_on = []
        if not isinstance(depends_on, list):
            _err(errors, f"item '{item_id}': depends_on must be a list when present")
            depends_on = []
        else:
            for dep in depends_on:
                if not isinstance(dep, str) or not dep:
                    _err(
                        errors,
                        f"item '{item_id}': depends_on entries must be non-empty strings",
                    )
                elif dep not in known_kconfig_symbols:
                    _err(
                        errors,
                        f"item '{item_id}': depends_on references unknown "
                        f"kconfig_symbol '{dep}' (no item declares it)",
                    )
        op = item.get("depends_on_op")
        if op is not None and op not in _DEPENDS_ON_OPS:
            _err(
                errors,
                f"item '{item_id}': depends_on_op '{op}' is not one of {list(_DEPENDS_ON_OPS)}",
            )

        profile_defaults = item.get("profile_defaults")
        if profile_defaults is None:
            _err(errors, f"item '{item_id}': profile_defaults is required")
            profile_defaults = {}
        if not isinstance(profile_defaults, dict):
            _err(errors, f"item '{item_id}': profile_defaults must be an object")
            profile_defaults = {}
        for profile_key in expected_profiles:
            if profile_key not in profile_defaults:
                _err(
                    errors,
                    f"item '{item_id}': profile_defaults missing profile '{profile_key}'",
                )
            elif not _is_bool(profile_defaults[profile_key]):
                _err(
                    errors,
                    f"item '{item_id}': profile_defaults['{profile_key}'] must be a boolean",
                )

        opc_reference = item.get("opc_reference")
        if opc_reference is not None and not isinstance(opc_reference, dict):
            _err(errors, f"item '{item_id}': opc_reference must be an object when present")

        opc_display_name = item.get("opc_display_name")
        if kind in ("facet", "conformance_unit"):
            if not isinstance(opc_display_name, str) or not opc_display_name:
                _err(
                    errors,
                    f"item '{item_id}': OPC {kind} must declare a non-empty opc_display_name",
                )

        # Source-metadata check: every OPC item (facet, conformance_unit,
        # profile) must carry at least one non-null grounding field so that
        # imported OPC facts are always traceable to an OPC Foundation or
        # pinned repository source. Optimization items are exempt.
        if kind in ("facet", "conformance_unit", "profile"):
            has_opc_source = False
            if isinstance(opc_reference, dict):
                for src_key in ("spec", "section", "facet", "source_doc", "source_url"):
                    val = opc_reference.get(src_key)
                    if val is not None:
                        has_opc_source = True
                        break
            source_metadata = item.get("source_metadata")
            if not has_opc_source and isinstance(source_metadata, dict):
                for src_key in ("source_doc", "source_url", "imported_from", "snapshot_id"):
                    val = source_metadata.get(src_key)
                    if val is not None:
                        has_opc_source = True
                        break
            if not has_opc_source:
                _err(
                    errors,
                    f"item '{item_id}': OPC {kind} must have source metadata "
                    f"(non-null opc_reference.spec/section/facet/source_doc/source_url "
                    f"or source_metadata with a grounding field)",
                )

    facet_containment = manifest.get("facet_containment")
    if facet_containment is not None:
        if not isinstance(facet_containment, dict):
            _err(errors, "manifest.facet_containment must be an object")
        else:
            cu_to_facets: dict[str, set[str]] = {}
            for facet_key, cu_list in facet_containment.items():
                if facet_key not in item_kinds:
                    _err(
                        errors,
                        f"facet_containment key '{facet_key}' does not match any item id",
                    )
                elif item_kinds[facet_key] != "facet":
                    _err(
                        errors,
                        f"facet_containment key '{facet_key}' references item of kind "
                        f"'{item_kinds[facet_key]}' (must be 'facet')",
                    )
                if not isinstance(cu_list, list):
                    _err(
                        errors,
                        f"facet_containment['{facet_key}'] must be a list",
                    )
                    continue
                for cu_id in cu_list:
                    if not isinstance(cu_id, str) or not cu_id:
                        _err(
                            errors,
                            f"facet_containment['{facet_key}']: entries must be non-empty strings",
                        )
                        continue
                    if cu_id not in item_kinds:
                        _err(
                            errors,
                            f"facet_containment['{facet_key}'] references unknown "
                            f"conformance_unit '{cu_id}' (no item declares it)",
                        )
                        continue
                    if item_kinds[cu_id] != "conformance_unit":
                        _err(
                            errors,
                            f"facet_containment['{facet_key}'] references item '{cu_id}' "
                            f"of kind '{item_kinds[cu_id]}' (must be 'conformance_unit')",
                        )
                        continue
                    cu_to_facets.setdefault(cu_id, set()).add(facet_key)
            for cu_id, facets in cu_to_facets.items():
                if len(facets) > 1:
                    _err(
                        errors,
                        f"conformance_unit '{cu_id}' is contained by multiple facets: "
                        + ", ".join(sorted(facets)),
                    )
            for cu_id in sorted(item_kinds):
                if item_kinds[cu_id] == "conformance_unit" and cu_id not in cu_to_facets:
                    _err(
                        errors,
                        f"conformance_unit '{cu_id}' is not contained by any facet",
                    )

    capacities = manifest.get("capacities")
    if not isinstance(capacities, list):
        _err(errors, "manifest.capacities must be a list")
        capacities = []

    seen_capacity_ids: set[str] = set()
    seen_capacity_overrides: dict[str, str] = {}
    seen_capacity_macros: dict[str, str] = {}
    seen_capacity_kconfig: dict[str, str] = {}

    for idx, cap in enumerate(capacities):
        ctx = f"capacities[{idx}]"
        if not isinstance(cap, dict):
            _err(errors, f"{ctx}: must be an object")
            continue

        _require_keys(
            errors,
            cap,
            (
                "id",
                "public_override",
                "internal_macro",
                "kconfig_symbol",
                "kind",
                "internal_classification",
                "defaults",
                "baseline_default",
            ),
            ctx,
        )

        cap_id = cap.get("id")
        if not isinstance(cap_id, str) or not cap_id:
            _err(errors, f"{ctx}: id must be a non-empty string")
            cap_id = f"<{idx}>"
        elif cap_id in seen_capacity_ids:
            _err(errors, f"{ctx}: duplicate capacity id '{cap_id}'")
        else:
            seen_capacity_ids.add(cap_id)

        public_override = cap.get("public_override")
        if not isinstance(public_override, str) or not public_override:
            _err(errors, f"capacity '{cap_id}': public_override must be a non-empty string")
        elif public_override in seen_capacity_overrides:
            _err(
                errors,
                f"capacity '{cap_id}': public_override '{public_override}' already "
                f"used by capacity '{seen_capacity_overrides[public_override]}'",
            )
        else:
            seen_capacity_overrides[public_override] = cap_id

        internal_macro = cap.get("internal_macro")
        if not isinstance(internal_macro, str) or not internal_macro:
            _err(errors, f"capacity '{cap_id}': internal_macro must be a non-empty string")
        elif internal_macro in seen_capacity_macros:
            _err(
                errors,
                f"capacity '{cap_id}': internal_macro '{internal_macro}' already "
                f"used by capacity '{seen_capacity_macros[internal_macro]}'",
            )
        else:
            seen_capacity_macros[internal_macro] = cap_id

        kconfig_symbol = cap.get("kconfig_symbol")
        if kconfig_symbol is None or not isinstance(kconfig_symbol, str) or not kconfig_symbol:
            _err(errors, f"capacity '{cap_id}': kconfig_symbol must be a non-empty string")
        elif kconfig_symbol in seen_capacity_kconfig:
            _err(
                errors,
                f"capacity '{cap_id}': kconfig_symbol '{kconfig_symbol}' already "
                f"used by capacity '{seen_capacity_kconfig[kconfig_symbol]}'",
            )
        else:
            seen_capacity_kconfig[kconfig_symbol] = cap_id

        kind = cap.get("kind")
        if kind not in _ALLOWED_CAPACITY_KINDS:
            _err(
                errors,
                f"capacity '{cap_id}': kind '{kind}' is not one of "
                f"{list(_ALLOWED_CAPACITY_KINDS)}",
            )

        classification = cap.get("internal_classification")
        if classification not in _ALLOWED_INTERNAL_CLASSIFICATIONS:
            _err(
                errors,
                f"capacity '{cap_id}': internal_classification '{classification}' "
                f"is not one of {list(_ALLOWED_INTERNAL_CLASSIFICATIONS)}",
            )

        defaults = cap.get("defaults")
        if not isinstance(defaults, dict):
            _err(errors, f"capacity '{cap_id}': defaults must be an object")
            defaults = {}
        for profile_key in expected_profiles:
            if profile_key not in defaults:
                _err(
                    errors,
                    f"capacity '{cap_id}': defaults missing profile '{profile_key}'",
                )
            elif not _is_int(defaults[profile_key]):
                _err(
                    errors,
                    f"capacity '{cap_id}': defaults['{profile_key}'] must be an integer",
                )

        baseline = cap.get("baseline_default")
        if not _is_int(baseline):
            _err(errors, f"capacity '{cap_id}': baseline_default must be an integer")

        if kind == "derived":
            derives_from = cap.get("derives_from")
            if not isinstance(derives_from, str) or not derives_from:
                _err(
                    errors,
                    f"capacity '{cap_id}': derived capacity must declare derives_from",
                )
            else:
                derives_cap = next((c for c in capacities if isinstance(c, dict) and c.get("id") == derives_from), None)
                if derives_cap is None:
                    _err(
                        errors,
                        f"capacity '{cap_id}': derives_from '{derives_from}' "
                        f"does not match any capacity id",
                    )

        opc_reference = cap.get("opc_reference")
        if opc_reference is not None and not isinstance(opc_reference, dict):
            _err(errors, f"capacity '{cap_id}': opc_reference must be an object when present")

    return errors


__all__ = [
    "load_manifest",
    "validate_manifest",
]
