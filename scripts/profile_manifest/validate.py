#!/usr/bin/env python3
"""Manifest validation CLI for the OPC-complete Kconfig manifest.

Usage:
    python3 scripts/profile_manifest/validate.py --manifest-only

    python3 scripts/profile_manifest/validate.py --check-generated

    python3 scripts/profile_manifest/validate.py --check-capacities

    python3 scripts/profile_manifest/validate.py --check-profiles

    python3 scripts/profile_manifest/validate.py --all

    python3 scripts/profile_manifest/validate.py \\
        --manifest profiles/opcua-profile-manifest.yaml --all

Exit codes:
    0 - validation passed
    1 - validation failed (errors printed to stdout)

Uses the Python standard library plus the in-repo vendored
``scripts/kconfig/kconfiglib.py`` for Kconfig-backed checks.
"""

from __future__ import annotations

import argparse
import os
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
_PKG_PARENT = os.path.dirname(_HERE)
_REPO_ROOT = os.path.dirname(_PKG_PARENT)
if _PKG_PARENT not in sys.path:
    sys.path.insert(0, _PKG_PARENT)

from profile_manifest.model import load_manifest, validate_manifest  # noqa: E402
from profile_manifest import graph_deps  # noqa: E402  # pylint: disable=wrong-import-position

_DEFAULT_PROFILES = ("nano", "micro", "embedded", "standard", "full", "custom")
_NAMED_PROFILES = ("nano", "micro", "embedded", "standard", "full")
_KCONFIG_DIR = os.path.join(_REPO_ROOT, "scripts", "kconfig")
_DEFAULT_MANIFEST = os.path.join(
    _REPO_ROOT, "profiles", "opcua-profile-manifest.yaml"
)
_UNSELECTABLE_STATES = ("unimplemented",)
_IN_SCOPE_071_CU_IDS = frozenset(
    (
        "opc_cu_2317",
        "opc_cu_2328",
        "opc_cu_2352",
        "opc_cu_2389",
        "opc_cu_2400",
        "opc_cu_2936",
        "opc_cu_3147",
        "opc_cu_3192",
        "opc_cu_3530",
        "opc_cu_3983",
    )
)


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate the OPC UA profile manifest.",
    )
    parser.add_argument(
        "--manifest",
        default=_DEFAULT_MANIFEST,
        help="Path to the manifest .yaml/.json file "
        "(default: profiles/opcua-profile-manifest.yaml).",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Run all checks: manifest validation, generated drift check, "
        "Kconfig parse check, unimplemented-item availability check, "
        "capacity compatibility check, named profile resolution check, "
        "profile override-to-custom check, claim/test map validation, "
        "naming convention check, and OPC reference-in-help check.",
    )
    parser.add_argument(
        "--manifest-only",
        action="store_true",
        help="Run only manifest-level validation (no generated-file or claim checks).",
    )
    parser.add_argument(
        "--check-generated",
        action="store_true",
        help="Check that committed Kconfig, defconfigs, and capacities.h match generator output.",
    )
    parser.add_argument(
        "--check-capacities",
        action="store_true",
        help="Check that Kconfig capacity int symbols resolve to manifest defaults per profile.",
    )
    parser.add_argument(
        "--check-profiles",
        action="store_true",
        help="Check that each named profile resolves to itself when loaded "
        "without facet/CU overrides (OPC-10000-7 §4.3), and that "
        "overriding a standard-profile Facet transitions the effective "
        "profile to custom (OPC-10000-7 §4.2 and §4.3).",
    )
    parser.add_argument(
        "--check-claims",
        action="store_true",
        help="Check that every claimed manifest item has backing tests and the "
        "generated claim map / roadmap / build docs match generator output.",
    )
    return parser.parse_args(argv)


def _check_generated(manifest: dict, manifest_path: str) -> list[str]:
    """Regenerate Kconfig, defconfigs, and capacities.h and compare bytes.

    Returns a list of error strings (empty means no drift).
    """
    from profile_manifest.generate import (
        generate_kconfig,
        generate_defconfig,
        generate_capacities_h,
    )

    errors: list[str] = []

    # Kconfig drift check.
    kconfig_path = os.path.join(_REPO_ROOT, "Kconfig")
    expected_kconfig = generate_kconfig(manifest)
    try:
        with open(kconfig_path, "r", encoding="utf-8") as fh:
            actual_kconfig = fh.read()
    except FileNotFoundError:
        errors.append("Kconfig: file is missing (run generate.py to create it)")
        actual_kconfig = None

    if actual_kconfig is not None and actual_kconfig != expected_kconfig:
        errors.append(
            "Kconfig: drift detected -- committed file does not match "
            "generator output (run generate.py --outputs kconfig to regenerate)"
        )

    # OPC UA Kconfig structure check (OPC-10000-7 §4.2, §4.3).
    # The generated Kconfig must contain internal cascade symbols, profile
    # drilldown sections, at least one Facet drilldown menu, at least one
    # Conformance Unit prompt, and a separate 'Project options' menu.
    if "MUC_OPCUA_INTERN_PROFILE_" not in expected_kconfig:
        errors.append(
            "Kconfig: no MUC_OPCUA_INTERN_PROFILE_ symbols found "
            "(expected internal cascade symbols per OPC-10000-7 §4.3; "
            "run generate.py --outputs kconfig to regenerate)"
        )
    if 'menu "Profile:' not in expected_kconfig:
        errors.append(
            "Kconfig: no 'Profile:' menu found in generated Kconfig "
            "(expected at least one menu \"Profile: ...\" per OPC-10000-7 §4.3; "
            "run generate.py --outputs kconfig to regenerate)"
        )
    # Every named profile must have a drilldown section so users can browse
    # any profile regardless of selection.  This catches the empty-section
    # regression where a profile with no unique canonical facets/CUs would
    # be silently omitted.
    _NAMED_PROFILE_KEYS = ("nano", "micro", "embedded", "standard", "full")
    manifest_profiles = manifest.get("profiles", {})
    for pk in _NAMED_PROFILE_KEYS:
        profile = manifest_profiles.get(pk, {})
        opc_display_name = profile.get("opc_display_name")
        if not isinstance(opc_display_name, str) or not opc_display_name:
            continue
        expected_menu = 'menu "Profile: ' + opc_display_name + '"'
        if expected_menu not in expected_kconfig:
            errors.append(
                "Kconfig: no 'Profile: " + opc_display_name
                + "' menu found for profile '" + pk
                + "' (every named profile must have a drilldown section "
                "even if empty; OPC-10000-7 §4.3; run generate.py "
                "--outputs kconfig to regenerate)"
            )
    if 'menu "Facet:' not in expected_kconfig and 'menuconfig MUC_OPCUA_FACET_' not in expected_kconfig:
        errors.append(
            "Kconfig: no Facet menu or menuconfig found in generated Kconfig "
            "(expected at least one menu \"Facet: ...\" or menuconfig MUC_OPCUA_FACET_* "
            "per OPC-10000-7 §4.2; run generate.py --outputs kconfig to regenerate)"
        )
    if '"CU:' not in expected_kconfig:
        errors.append(
            "Kconfig: no 'CU:' prompt found in generated Kconfig "
            "(expected at least one bool \"CU: ...\" per OPC-10000-7 §4.2; "
            "run generate.py --outputs kconfig to regenerate)"
        )
    if 'menu "Project options"' not in expected_kconfig:
        errors.append(
            "Kconfig: no 'Project options' menu found in generated Kconfig "
            "(expected menu \"Project options\" to separate non-OPC "
            "optimization items from the OPC Facet/CU tree per "
            "OPC-10000-7 §4.2; run generate.py --outputs kconfig to regenerate)"
        )

    # Old preset/facets-match symbols must not appear (graph-derived redo).
    for old_sym in ("MUC_OPCUA_PROFILE_PRESET_", "MUC_OPCUA_FACETS_MATCH_"):
        if old_sym in expected_kconfig:
            errors.append(
                "Kconfig: obsolete symbol prefix '" + old_sym
                + "' found in generated Kconfig (should use "
                "MUC_OPCUA_INTERN_PROFILE_ cascade symbols; "
                "run generate.py --outputs kconfig to regenerate)"
            )
            break

    # CUs inside ``if FACET``/``endif`` blocks depend on the facet.
    # The ``if`` block enforces facet-off → CU-off. No per-CU
    # ``depends on`` or ``select`` needed.

    # Defconfig drift check.
    for pk in _DEFAULT_PROFILES:
        defconfig_path = os.path.join(_REPO_ROOT, "configs", pk + ".defconfig")
        expected_defconfig = generate_defconfig(manifest, pk)
        try:
            with open(defconfig_path, "r", encoding="utf-8") as fh:
                actual_defconfig = fh.read()
        except FileNotFoundError:
            errors.append(
                "configs/" + pk + ".defconfig: file is missing "
                "(run generate.py --outputs defconfigs to create it)"
            )
            continue

        if actual_defconfig != expected_defconfig:
            errors.append(
                "configs/" + pk + ".defconfig: drift detected -- committed file "
                "does not match generator output"
            )

    # capacities.h drift check.
    cap_path = os.path.join(_REPO_ROOT, "include", "muc_opcua", "capacities.h")
    expected_cap = generate_capacities_h(manifest)
    try:
        with open(cap_path, "r", encoding="utf-8") as fh:
            actual_cap = fh.read()
    except FileNotFoundError:
        errors.append(
            "include/muc_opcua/capacities.h: file is missing "
            "(run generate.py --outputs capacities_h to create it)"
        )
        actual_cap = None

    if actual_cap is not None and actual_cap != expected_cap:
        errors.append(
            "include/muc_opcua/capacities.h: drift detected -- committed file "
            "does not match generator output (run generate.py --outputs capacities_h)"
        )

    # Claim map drift check.
    from profile_manifest.generate import generate_claim_map

    claim_path = os.path.join(
        _REPO_ROOT, "tests", "conformance", "claim_test_map.md"
    )
    expected_claim = generate_claim_map(manifest)
    try:
        with open(claim_path, "r", encoding="utf-8") as fh:
            actual_claim = fh.read()
    except FileNotFoundError:
        errors.append(
            "tests/conformance/claim_test_map.md: file is missing "
            "(run generate.py --outputs claim_map to create it)"
        )
        actual_claim = None

    if actual_claim is not None and actual_claim != expected_claim:
        errors.append(
            "tests/conformance/claim_test_map.md: drift detected -- committed "
            "file does not match generator output (run generate.py --outputs claim_map)"
        )

    # Roadmap drift check.
    from profile_manifest.generate import generate_roadmap

    roadmap_path = os.path.join(
        _REPO_ROOT, "docs", "conformance", "opc-profile-roadmap.md"
    )
    expected_roadmap = generate_roadmap(manifest)
    try:
        with open(roadmap_path, "r", encoding="utf-8") as fh:
            actual_roadmap = fh.read()
    except FileNotFoundError:
        errors.append(
            "docs/conformance/opc-profile-roadmap.md: file is missing "
            "(run generate.py --outputs roadmap to create it)"
        )
        actual_roadmap = None

    if actual_roadmap is not None and actual_roadmap != expected_roadmap:
        errors.append(
            "docs/conformance/opc-profile-roadmap.md: drift detected -- "
            "committed file does not match generator output "
            "(run generate.py --outputs roadmap)"
        )

    # completion.md drift check (spec 073 CU-based conformance completion).
    import json as _json

    from profile_manifest import completion as _completion

    completion_path = os.path.join(_REPO_ROOT, "docs", "conformance", "completion.md")
    snap_path = os.path.join(_REPO_ROOT, "profiles", "opcua-profile-normalized-snapshot.json")
    try:
        with open(snap_path, encoding="utf-8") as fh:
            _snapshot = _json.load(fh)
        _cat_path = os.path.join(_REPO_ROOT, "profiles", "opcua-server-conformance.json")
        _catalog = None
        if os.path.exists(_cat_path):
            with open(_cat_path, encoding="utf-8") as fh:
                _catalog = _json.load(fh)
        expected_completion = _completion.render_report(manifest, _snapshot, _catalog)
        with open(completion_path, "r", encoding="utf-8") as fh:
            actual_completion = fh.read()
        if actual_completion != expected_completion:
            errors.append(
                "docs/conformance/completion.md: drift detected -- committed "
                "file does not match generator output "
                "(run generate.py --outputs completion)"
            )
    except FileNotFoundError:
        errors.append(
            "docs/conformance/completion.md: file is missing "
            "(run generate.py --outputs completion to create it)"
        )

    # build-and-gating.md generated-section drift check.
    _check_build_docs_section(manifest, errors)

    # Latest-active canonical profile surface (OPC-10000-7 §4.3, §4.7):
    # committed Kconfig must contain 2025 profile choice symbols and the
    # canonical defconfigs must select them; superseded 2022 symbols must
    # not be selectable. Only run when the Kconfig file exists; the missing
    # file case is already reported above.
    if actual_kconfig is not None:
        _check_latest_active_profile_surface(errors)

    return errors


def _check_latest_active_profile_surface(errors: list[str]) -> None:
    kconfig_path = os.path.join(_REPO_ROOT, "Kconfig")
    config_dir = os.path.join(_REPO_ROOT, "configs")
    with open(kconfig_path, "r", encoding="utf-8") as fh:
        kconfig = fh.read()
    required = (
        "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER",
        "MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER",
        "MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER",
        "MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER",
    )
    for sym in required:
        if ("config " + sym) not in kconfig:
            errors.append("generated Kconfig missing active profile symbol " + sym)
    forbidden_choice = (
        "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2022_SERVER",
        "MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2022_SERVER",
        "MUC_OPCUA_PROFILE_EMBEDDED_2022_UA_SERVER",
        "MUC_OPCUA_PROFILE_STANDARD_2022_UA_SERVER",
    )
    for sym in forbidden_choice:
        if ("config " + sym) in kconfig:
            errors.append("superseded 2022 profile must not be selectable: " + sym)
    defconfigs = {
        "nano": required[0],
        "micro": required[1],
        "embedded": required[2],
        "standard": required[3],
    }
    for name, sym in defconfigs.items():
        path = os.path.join(config_dir, name + ".defconfig")
        with open(path, "r", encoding="utf-8") as fh:
            text = fh.read()
        if sym + "=y" not in text:
            errors.append(path + " does not select " + sym)


def _check_capacity_defaults(manifest: dict) -> list[str]:
    """Verify Kconfig capacity int symbols resolve to manifest defaults.

    Loads the generated Kconfig with kconfiglib, applies each profile defconfig,
    and checks that every capacity int symbol resolves to the value declared in
    the manifest for that profile.  Returns a list of error strings.
    """
    errors: list[str] = []

    kconfig_path = os.path.join(_REPO_ROOT, "Kconfig")
    if not os.path.isfile(kconfig_path):
        errors.append("capacity defaults: Kconfig not found at " + kconfig_path)
        return errors

    if _KCONFIG_DIR not in sys.path:
        sys.path.insert(0, _KCONFIG_DIR)

    # See _check_named_profile_resolution for why CONFIG_ must be empty:
    # generated symbols carry the MUC_OPCUA_ prefix and defconfig lines
    # must match verbatim.
    os.environ["CONFIG_"] = ""
    import kconfiglib  # noqa: E402

    capacities = manifest.get("capacities", [])
    cap_symbols = {}
    for cap in capacities:
        sym = cap.get("kconfig_symbol")
        if not sym:
            continue
        defaults = cap.get("defaults", {})
        cap_symbols[sym] = defaults

    for pk in _DEFAULT_PROFILES:
        defconfig_path = os.path.join(_REPO_ROOT, "configs", pk + ".defconfig")
        if not os.path.isfile(defconfig_path):
            errors.append(
                "capacity defaults: configs/" + pk + ".defconfig not found"
            )
            continue

        kconf = kconfiglib.Kconfig(kconfig_path, warn=False)
        kconf.load_config(defconfig_path)

        for sym_name, defaults in cap_symbols.items():
            expected = defaults.get(pk)
            if expected is None:
                continue
            sym = kconf.syms.get(sym_name)
            if sym is None:
                errors.append(
                    "capacity defaults: Kconfig symbol '" + sym_name
                    + "' not found in generated Kconfig"
                )
                continue
            actual = sym.str_value
            if str(actual) != str(expected):
                errors.append(
                    "capacity defaults: " + sym_name + " for profile '" + pk
                    + "': expected " + str(expected) + ", got " + str(actual)
                )

    return errors


def _check_named_profile_resolution(manifest: dict) -> list[str]:
    """Verify each named profile remains selected when loaded without overrides.

    Regenerates the expected Kconfig from the manifest, loads it with
    kconfiglib, applies each named profile's defconfig (which selects only
    that profile's choice symbol -- no facet or CU overrides), and confirms
    the corresponding ``MUC_OPCUA_PROFILE_<OPC_NAME>`` symbol resolves to
    ``y`` (OPC-10000-7 §4.3).

    This validates the profile-choice defaults introduced in US4: a named
    profile selected without overrides must not degenerate to custom.
    Custom is intentionally excluded here; T044 covers the
    override-to-custom transition.

    Returns a list of error strings (empty means no errors).
    """
    errors: list[str] = []

    if _KCONFIG_DIR not in sys.path:
        sys.path.insert(0, _KCONFIG_DIR)

    # Generated Kconfig symbols include the ``MUC_OPCUA_`` prefix in their
    # names (e.g. ``MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2017_SERVER``).
    # With ``CONFIG_=MUC_OPCUA_`` kconfiglib would strip that prefix from
    # defconfig lines, leaving ``PROFILE_NANO_...`` which does not match any
    # symbol.  Setting ``CONFIG_`` to the empty string makes kconfiglib use
    # defconfig entries verbatim so they match the prefixed symbol names.
    os.environ["CONFIG_"] = ""
    import kconfiglib  # noqa: E402

    from profile_manifest.generate import (  # noqa: E402
        generate_kconfig,
        generate_defconfig,
        _resolve_profile_symbol,
    )

    expected_kconfig = generate_kconfig(manifest)
    profiles = manifest.get("profiles", {})

    # Write the regenerated Kconfig to a temp file so the check operates
    # on expected content rather than the possibly-stale committed file.
    fd, kconfig_tmp_path = tempfile.mkstemp(suffix=".kconfig")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            fh.write(expected_kconfig)

        # Verify the expected Kconfig parses cleanly before per-profile
        # checks.  A parse failure (e.g. dependency loop) is reported as
        # a single clear error rather than one per profile.
        try:
            kconfiglib.Kconfig(kconfig_tmp_path, warn=False)
        except Exception as exc:
            errors.append(
                "named profile resolution: generated Kconfig failed to "
                "parse with kconfiglib: "
                + str(exc).strip().replace("\n", " ")
                + " (OPC-10000-7 §4.3)"
            )
            return errors

        for pk in _NAMED_PROFILES:
            expected_symbol = _resolve_profile_symbol(profiles, pk)

            # Regenerate the expected defconfig so the check operates on
            # expected content (including preset seed symbols) rather than
            # the possibly-stale committed file.
            expected_defconfig = generate_defconfig(manifest, pk)
            fd2, defconfig_tmp_path = tempfile.mkstemp(suffix=".defconfig")
            try:
                with os.fdopen(fd2, "w", encoding="utf-8") as fh:
                    fh.write(expected_defconfig)

                try:
                    kconf = kconfiglib.Kconfig(kconfig_tmp_path, warn=False)
                    kconf.load_config(defconfig_tmp_path)
                except Exception as exc:
                    errors.append(
                        "named profile resolution: failed to load defconfig "
                        "for profile '" + pk + "': "
                        + str(exc).strip().replace("\n", " ")
                    )
                    continue
            finally:
                try:
                    os.unlink(defconfig_tmp_path)
                except OSError:
                    pass

            sym = kconf.syms.get(expected_symbol)
            if sym is None:
                errors.append(
                    "named profile resolution: profile symbol '"
                    + expected_symbol
                    + "' not found in generated Kconfig (profile '" + pk + "')"
                )
                continue

            if sym.tri_value != 2:
                errors.append(
                    "named profile resolution: profile '" + pk + "' (symbol '"
                    + expected_symbol
                    + "') is not selected after loading its defconfig without "
                    "overrides: expected y, got " + sym.str_value
                    + " (OPC-10000-7 §4.3; the named profile should remain "
                    "selected when no facet/CU overrides are applied)"
                )
    finally:
        try:
            os.unlink(kconfig_tmp_path)
        except OSError:
            pass

    return errors


def _check_profile_override_to_custom(manifest: dict) -> list[str]:
    """Verify profile choice is preserved when facets are overridden.

    New semantics (OPC-10000-7 §4.3): profile selection seeds defaults
    only.  Overriding a facet away from the profile default changes the
    feature set but does NOT switch the profile to ``custom``.  The
    selected named-profile choice symbol must remain ``y``, and the
    overridden facet must reflect the user's override.

    This also checks cross-profile activation: selecting ``nano`` and
    enabling a higher-profile facet must turn that facet ON while the
    profile choice stays ``nano`` and unrelated higher-profile facets
    remain OFF.
    """
    errors: list[str] = []

    if _KCONFIG_DIR not in sys.path:
        sys.path.insert(0, _KCONFIG_DIR)

    os.environ["CONFIG_"] = ""
    import kconfiglib  # noqa: E402

    from profile_manifest.generate import (  # noqa: E402
        generate_kconfig,
        generate_defconfig,
        _resolve_profile_symbol,
        _selectable_facet_toggles,
    )

    expected_kconfig = generate_kconfig(manifest)
    profiles = manifest.get("profiles", {})
    standard_symbol = _resolve_profile_symbol(profiles, "standard")

    fd, kconfig_tmp_path = tempfile.mkstemp(suffix=".kconfig")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            fh.write(expected_kconfig)

        try:
            kconfiglib.Kconfig(kconfig_tmp_path, warn=False)
        except Exception as exc:
            errors.append(
                "profile override: generated Kconfig failed to "
                "parse with kconfiglib: "
                + str(exc).strip().replace("\n", " ")
                + " (OPC-10000-7 §4.2)"
            )
            return errors

        # -- Check 1: overriding a facet preserves the profile choice -------
        expected_defconfig = generate_defconfig(manifest, "standard")
        fd2, defconfig_tmp_path = tempfile.mkstemp(suffix=".defconfig")
        try:
            with os.fdopen(fd2, "w", encoding="utf-8") as fh:
                fh.write(expected_defconfig)

            try:
                kconf = kconfiglib.Kconfig(kconfig_tmp_path, warn=False)
                kconf.load_config(defconfig_tmp_path)
            except Exception as exc:
                errors.append(
                    "profile override: failed to load standard "
                    "defconfig: " + str(exc).strip().replace("\n", " ")
                    + " (OPC-10000-7 §4.3)"
                )
                return errors
        finally:
            try:
                os.unlink(defconfig_tmp_path)
            except OSError:
                pass

        # Select a standard-enabled Facet toggle symbol.
        candidate = "MUC_OPCUA_FACET_CORE_2017_SERVER"
        facet_symbol = None
        probe = kconf.syms.get(candidate)
        if probe is not None and probe.type:
            facet_symbol = candidate
        else:
            for sym, facet_item in _selectable_facet_toggles(manifest):
                pd = facet_item.get("profile_defaults")
                if isinstance(pd, dict) and pd.get("standard") is True:
                    if sym in kconf.syms and kconf.syms[sym].type:
                        facet_symbol = sym
                        break

        if facet_symbol is None:
            errors.append(
                "profile override: no standard-enabled Facet "
                "toggle symbol found in generated Kconfig to override "
                "(expected at least one MUC_OPCUA_FACET_* with "
                "profile_defaults.standard=true; OPC-10000-7 §4.2)"
            )
            return errors

        facet_sym = kconf.syms.get(facet_symbol)
        if facet_sym is None:
            errors.append(
                "profile override: Facet toggle symbol '"
                + facet_symbol + "' not found (OPC-10000-7 §4.2)"
            )
            return errors

        if facet_sym.tri_value != 2:
            errors.append(
                "profile override: Facet toggle '" + facet_symbol
                + "' is '" + facet_sym.str_value + "' under standard "
                "(expected y; OPC-10000-7 §4.2)"
            )
            return errors

        # Override the Facet away from standard default (y -> n).
        facet_sym.set_value(0)

        # The profile choice must STAY as standard (new semantics).
        std_sym = kconf.syms.get(standard_symbol)
        if std_sym is None:
            errors.append(
                "profile override: standard profile symbol '"
                + standard_symbol + "' not found (OPC-10000-7 §4.3)"
            )
        elif std_sym.tri_value != 2:
            errors.append(
                "profile override: after overriding Facet '"
                + facet_symbol + "' to n, standard profile symbol '"
                + standard_symbol + "' is '" + std_sym.str_value
                + "' (expected y; OPC-10000-7 §4.3 -- profile choice "
                "must be preserved when facets are overridden)"
            )

        # The facet itself must be OFF.
        if facet_sym.tri_value != 0:
            errors.append(
                "profile override: after setting Facet '" + facet_symbol
                + "' to n, it is '" + facet_sym.str_value
                + "' (expected n; the user override must take effect)"
            )

        # -- Check 2: cross-profile activation -----------------------------
        # Select nano and enable a higher-profile facet.
        nano_symbol = _resolve_profile_symbol(profiles, "nano")
        nano_defconfig = generate_defconfig(manifest, "nano")
        fd3, nano_defconfig_tmp = tempfile.mkstemp(suffix=".defconfig")
        try:
            with os.fdopen(fd3, "w", encoding="utf-8") as fh:
                fh.write(nano_defconfig)

            kconf2 = kconfiglib.Kconfig(kconfig_tmp_path, warn=False)
            kconf2.load_config(nano_defconfig_tmp)
        finally:
            try:
                os.unlink(nano_defconfig_tmp)
            except OSError:
                pass

        nano_sym = kconf2.syms.get(nano_symbol)
        if nano_sym is None or nano_sym.tri_value != 2:
            errors.append(
                "cross-profile activation: nano profile not selected "
                "after loading its defconfig (OPC-10000-7 §4.3)"
            )
        else:
            # Find a full-only facet to enable from nano.
            cross_facet = None
            for sym, facet_item in _selectable_facet_toggles(manifest):
                pd = facet_item.get("profile_defaults")
                if (
                    isinstance(pd, dict)
                    and pd.get("full") is True
                    and pd.get("standard") is not True
                ):
                    if sym in kconf2.syms and kconf2.syms[sym].type:
                        cross_facet = sym
                        break

            if cross_facet is not None:
                cross_sym = kconf2.syms[cross_facet]
                # Must be OFF under nano (not in nano's defaults).
                if cross_sym.tri_value != 0:
                    errors.append(
                        "cross-profile activation: facet '" + cross_facet
                        + "' is ON under nano before override (expected OFF)"
                    )
                else:
                    # Enable it.
                    cross_sym.set_value(2)
                    if cross_sym.tri_value != 2:
                        errors.append(
                            "cross-profile activation: facet '"
                            + cross_facet + "' could not be enabled from "
                            "nano (expected ON after user override)"
                        )
                    # Profile must still be nano.
                    if nano_sym.tri_value != 2:
                        errors.append(
                            "cross-profile activation: after enabling '"
                            + cross_facet + "' from nano, nano profile is '"
                            + nano_sym.str_value + "' (expected y)"
                        )
    finally:
        try:
            os.unlink(kconfig_tmp_path)
        except OSError:
            pass

    return errors


def _check_build_docs_section(manifest: dict, errors: list[str]) -> None:
    """Check that the generated section in docs/build-and-gating.md matches."""
    from profile_manifest.generate import (
        generate_build_docs_section,
        _BUILD_DOCS_BEGIN,
        _BUILD_DOCS_END,
    )

    path = os.path.join(_REPO_ROOT, "docs", "build-and-gating.md")
    try:
        with open(path, "r", encoding="utf-8") as fh:
            content = fh.read()
    except FileNotFoundError:
        errors.append(
            "docs/build-and-gating.md: file is missing "
            "(run generate.py --outputs build_docs to create it)"
        )
        return

    begin_idx = content.find(_BUILD_DOCS_BEGIN)
    end_idx = content.find(_BUILD_DOCS_END)

    if begin_idx == -1 or end_idx == -1 or end_idx <= begin_idx:
        errors.append(
            "docs/build-and-gating.md: generated-section markers not found "
            "(run generate.py --outputs build_docs to insert them)"
        )
        return

    expected_section = generate_build_docs_section(manifest)
    actual_section = content[begin_idx + len(_BUILD_DOCS_BEGIN):end_idx]

    if actual_section != "\n" + expected_section:
        errors.append(
            "docs/build-and-gating.md: generated section drift detected -- "
            "content between BEGIN/END markers does not match generator output "
            "(run generate.py --outputs build_docs)"
        )


def _check_claims(manifest: dict) -> list[str]:
    """Validate claim/test coverage from the manifest.

    Checks:
      - Every item with implementation_state ``claimed`` has at least one
        ``backing_tests`` entry.
      - Every claimed item that is enabled in a profile has backing tests
        listed for that profile's claim.
      - Items with state ``implemented`` that have backing tests appear in
        the generated claim map (informational; the drift check covers the
        exact byte-for-byte match).
    """
    errors: list[str] = []

    items = manifest.get("items", [])
    for item in items:
        if not isinstance(item, dict):
            continue

        errors.extend(_check_in_scope_071_implemented_cu_requirements(item))

        state = item.get("implementation_state")
        if state != "claimed":
            continue

        item_id = item.get("id", "<unknown>")
        backing = item.get("backing_tests") or []

        if not backing:
            errors.append(
                "claim: item '" + item_id + "' is claimed but has no "
                "backing_tests (every claimed item must list at least one test)"
            )
            continue

        for t in backing:
            if not isinstance(t, str) or not t:
                errors.append(
                    "claim: item '" + item_id + "' has an invalid backing_tests "
                    "entry (must be a non-empty string)"
                )

        profile_defaults = item.get("profile_defaults") or {}
        claimed_profiles = [
            pk for pk in _DEFAULT_PROFILES if profile_defaults.get(pk) is True
        ]
        if claimed_profiles:
            if not backing:
                errors.append(
                    "claim: item '" + item_id + "' is claimed for profiles "
                    + ", ".join(claimed_profiles)
                    + " but has no backing tests for those profiles"
                )

    return errors


def _check_in_scope_071_manifest_requirements(manifest: dict) -> list[str]:
    """Validate 071 in-scope implemented CU claim evidence in a manifest."""
    errors: list[str] = []
    items = manifest.get("items", [])
    if not isinstance(items, list):
        return errors
    for item in items:
        if isinstance(item, dict):
            errors.extend(_check_in_scope_071_implemented_cu_requirements(item))
    return errors


def _check_in_scope_071_implemented_cu_requirements(item: dict) -> list[str]:
    """Require claim evidence before a 071 CU is marked implemented.

    OPC-10000-7 §4.2 exposes Conformance Units as selectable profile/facet
    capabilities. For the 071 nano-service behaviour scope, an implemented CU
    claim must therefore have a selectable Kconfig symbol plus concrete backing
    tests before promotion (SCN-001, CASE-010, quickstart path 4).
    """
    item_id = item.get("id")
    if item_id not in _IN_SCOPE_071_CU_IDS:
        return []
    if item.get("kind") != "conformance_unit":
        return []
    if item.get("implementation_state") != "implemented":
        return []

    errors: list[str] = []
    symbol = item.get("kconfig_symbol")
    if not isinstance(symbol, str) or not symbol:
        errors.append(
            "claim: in-scope 071 CU '" + str(item_id) + "' is implemented but "
            "has no non-empty kconfig_symbol (required before promotion per "
            "OPC-10000-7 §4.2; evidence binding: SCN-001, CASE-010, "
            "quickstart path 4)"
        )

    backing = item.get("backing_tests") or []
    if not isinstance(backing, list) or not backing:
        errors.append(
            "claim: in-scope 071 CU '" + str(item_id) + "' is implemented but "
            "has no non-empty backing_tests (required before promotion per "
            "OPC-10000-7 §4.2; evidence binding: SCN-001, CASE-010, "
            "quickstart path 4)"
        )
    else:
        for test_path in backing:
            if not isinstance(test_path, str) or not test_path:
                errors.append(
                    "claim: in-scope 071 CU '" + str(item_id) + "' has an "
                    "invalid backing_tests entry (must be a non-empty string; "
                    "OPC-10000-7 §4.2; evidence binding: SCN-001, CASE-010, "
                    "quickstart path 4)"
                )
                break

    return errors


def _check_naming_conventions(manifest: dict) -> list[str]:
    """Verify generated OPC Kconfig symbols have no redundant kind suffix.

    The naming algorithm (research.md Decision 3, implemented in
    :func:`compute_kconfig_symbol`) strips one trailing kind word
    (e.g. ``Facet`` for facets, ``Profile`` for profiles) before applying
    the kind-specific ``MUC_OPCUA_*_`` prefix.  A generated Facet symbol
    ``MUC_OPCUA_FACET_<TOKEN>`` whose ``<TOKEN>`` ends with ``_FACET``
    (or a Profile symbol whose token ends with ``_PROFILE``) indicates the
    display name carries a redundant trailing kind word that was not
    stripped, producing a doubled suffix like
    ``MUC_OPCUA_FACET_CORE_2017_SERVER_FACET``.

    OPC reference: OPC-10000-7 §4.2 (Facets) and §4.3 (Profiles).

    Returns a list of error strings (empty means no errors).
    """
    from profile_manifest.generate import (  # noqa: E402
        compute_kconfig_symbol,
        _resolve_profile_symbol,
    )

    errors: list[str] = []

    facet_prefix = "MUC_OPCUA_FACET_"
    seen_facets: set[str] = set()
    for item in manifest.get("items", []):
        if not isinstance(item, dict) or item.get("kind") != "facet":
            continue
        opc_display_name = item.get("opc_display_name")
        if not isinstance(opc_display_name, str) or not opc_display_name:
            continue
        try:
            symbol = compute_kconfig_symbol(opc_display_name, "facet")
        except ValueError:
            continue
        if symbol in seen_facets:
            continue
        seen_facets.add(symbol)
        token = (
            symbol[len(facet_prefix):]
            if symbol.startswith(facet_prefix) else symbol
        )
        if token.endswith("_FACET"):
            errors.append(
                "naming convention: Facet symbol '" + symbol
                + "' (item '" + str(item.get("id", "<unknown>"))
                + "', opc_display_name '" + opc_display_name
                + "') has a redundant trailing '_FACET' in its token '"
                + token + "'; the trailing kind word should be stripped "
                + "from the display name (OPC-10000-7 §4.2)"
            )

    profile_prefix = "MUC_OPCUA_PROFILE_"
    profiles = manifest.get("profiles", {})
    for pk in profiles:
        symbol = _resolve_profile_symbol(profiles, pk)
        if not symbol.startswith(profile_prefix):
            continue
        token = symbol[len(profile_prefix):]
        if token.endswith("_PROFILE"):
            errors.append(
                "naming convention: Profile symbol '" + symbol
                + "' (profile '" + pk
                + "') has a redundant trailing '_PROFILE' in its token '"
                + token + "'; the trailing kind word should be stripped "
                + "from the display name (OPC-10000-7 §4.3)"
            )

    return errors


def _extract_config_help(kconfig_lines: list[str], symbol: str) -> str | None:
    """Extract the help text for a Kconfig ``config``/``menuconfig`` symbol.

    Scans *kconfig_lines* for ``config <symbol>`` or ``menuconfig <symbol>``,
    then looks for a ``\\thelp`` line within that config block and collects
    the subsequent help-body lines (those indented deeper than ``help``).

    Returns the concatenated help text, or ``None`` if the symbol has no
    config block or the block has no ``help`` section.
    """
    config_marker = "config " + symbol
    menuconfig_marker = "menuconfig " + symbol
    n = len(kconfig_lines)
    i = 0
    while i < n:
        if kconfig_lines[i] == config_marker or kconfig_lines[i] == menuconfig_marker:
            j = i + 1
            while j < n:
                line = kconfig_lines[j]
                if (
                    line
                    and not line.startswith("\t")
                    and not line.startswith("#")
                ):
                    return None
                if line.startswith("config ") or line.startswith("menuconfig "):
                    return None
                if line == "\thelp":
                    help_parts: list[str] = []
                    k = j + 1
                    while k < n:
                        hline = kconfig_lines[k]
                        if hline.startswith("\t  "):
                            help_parts.append(hline)
                            k += 1
                        else:
                            break
                    return "\n".join(help_parts)
                j += 1
            return None
        i += 1
    return None


def _check_opc_references_in_help(manifest: dict) -> list[str]:
    """Verify Profile/Facet/CU Kconfig help entries include OPC references.

    For every generated Profile, Facet toggle, and CU ``config`` symbol
    in the expected Kconfig output, finds its ``help`` block and verifies
    the help text includes the manifest OPC source reference
    (OPC-10000-7 §4.2 for Facets/CUs, §4.3 for Profiles):

    - Profile symbols: the help must contain the profile URI
      (``opc_profile_uri``) when the manifest declares one.
    - Facet/CU symbols: the help must contain ``opc_reference.spec`` and,
      when present, ``opc_reference.section``.

    Each error identifies the symbol and the manifest item ID whose help
    is missing the reference.  Symbols that do not appear as a
    ``config`` in the generated Kconfig (e.g. unselectable items emitted
    as ``comment`` directives) are skipped.
    """
    from profile_manifest.generate import (  # noqa: E402
        generate_kconfig,
        compute_kconfig_symbol,
        _resolve_profile_symbol,
        _facet_toggle_symbol,
        _SELECTABLE_STATES,
    )

    errors: list[str] = []
    expected_kconfig = generate_kconfig(manifest)
    kconfig_lines = expected_kconfig.splitlines()

    # -- Profile help entries (OPC-10000-7 §4.3) -------------------------
    profiles = manifest.get("profiles", {})
    for pk in _DEFAULT_PROFILES:
        profile = profiles.get(pk, {})
        if not isinstance(profile, dict):
            continue
        uri = profile.get("opc_profile_uri")
        if not uri:
            continue

        symbol = _resolve_profile_symbol(profiles, pk)
        config_line = "config " + symbol
        if not any(ln == config_line for ln in kconfig_lines):
            continue

        help_text = _extract_config_help(kconfig_lines, symbol)
        if help_text is None:
            errors.append(
                "OPC reference in help: profile symbol '" + symbol
                + "' (profile '" + pk + "') has no help entry "
                "(expected OPC UA Server Profile URI; OPC-10000-7 §4.3)"
            )
            continue

        if uri not in help_text:
            errors.append(
                "OPC reference in help: profile symbol '" + symbol
                + "' (profile '" + pk + "') help is missing the OPC UA "
                "Server Profile URI '" + uri + "' (OPC-10000-7 §4.3)"
            )

    # -- Facet/CU help entries (OPC-10000-7 §4.2) ------------------------
    for item in manifest.get("items", []):
        if not isinstance(item, dict):
            continue
        kind = item.get("kind")
        if kind not in ("facet", "conformance_unit"):
            continue
        state = item.get("implementation_state")
        if state not in _SELECTABLE_STATES:
            continue

        opc_ref = item.get("opc_reference")
        if not isinstance(opc_ref, dict):
            continue
        spec = opc_ref.get("spec")
        if not spec:
            continue
        section = opc_ref.get("section")

        item_id = str(item.get("id", "<unknown>"))

        if kind == "facet":
            symbol = _facet_toggle_symbol(item)
        else:
            sym_raw = item.get("kconfig_symbol")
            if isinstance(sym_raw, str) and sym_raw:
                symbol = sym_raw
            else:
                display = item.get("opc_display_name")
                if not isinstance(display, str) or not display:
                    symbol = None
                else:
                    try:
                        symbol = compute_kconfig_symbol(display, "conformance_unit")
                    except ValueError:
                        symbol = None

        if not symbol:
            continue

        config_line = "config " + symbol
        menuconfig_line = "menuconfig " + symbol
        if not any(ln == config_line or ln == menuconfig_line for ln in kconfig_lines):
            continue

        help_text = _extract_config_help(kconfig_lines, symbol)
        if help_text is None:
            expected_ref = spec + ((" §" + str(section)) if section else "")
            errors.append(
                "OPC reference in help: " + kind + " symbol '" + symbol
                + "' (item '" + item_id + "') has no help entry "
                + "for expected reference '" + expected_ref + "'"
            )
            continue
        expected_ref = spec + ((" §" + str(section)) if section else "")
        if expected_ref not in help_text:
            errors.append(
                "OPC reference in help: " + kind + " symbol '" + symbol
                + "' (item '" + item_id + "') help lacks expected reference '"
                + expected_ref + "'"
            )

    return errors


def _check_kconfig_parse(manifest: dict) -> list[str]:
    """Verify the generated Kconfig parses cleanly with kconfiglib.

    Loads the generated ``Kconfig`` and applies each profile defconfig,
    collecting any parse or load errors. Returns a list of error strings.
    """
    errors: list[str] = []

    kconfig_path = os.path.join(_REPO_ROOT, "Kconfig")
    if not os.path.isfile(kconfig_path):
        errors.append("kconfig parse: Kconfig not found at " + kconfig_path)
        return errors

    if _KCONFIG_DIR not in sys.path:
        sys.path.insert(0, _KCONFIG_DIR)

    # See _check_named_profile_resolution for why CONFIG_ must be empty:
    # generated symbols carry the MUC_OPCUA_ prefix and defconfig lines
    # must match verbatim.
    os.environ["CONFIG_"] = ""
    import kconfiglib  # noqa: E402

    for pk in _DEFAULT_PROFILES:
        defconfig_path = os.path.join(_REPO_ROOT, "configs", pk + ".defconfig")
        if not os.path.isfile(defconfig_path):
            errors.append(
                "kconfig parse: configs/" + pk + ".defconfig not found"
            )
            continue

        try:
            kconf = kconfiglib.Kconfig(kconfig_path, warn=False)
            kconf.load_config(defconfig_path)
        except Exception as exc:
            errors.append(
                "kconfig parse: failed to load profile '" + pk + "': " + str(exc)
            )

    return errors


def _check_unimplemented_availability(manifest: dict) -> list[str]:
    """Verify unimplemented items appear in Kconfig as comments, not symbols.

    Checks that (OPC-10000-7 §4.2 and each item's ``opc_reference``):
      - The manifest declares at least one unimplemented/deferred item.
      - Each such item's id appears in the generated Kconfig text.
      - None of them declare a ``kconfig_symbol`` (which would make them
        selectable).
      - Each unimplemented OPC item (has ``opc_reference`` or kind
        ``facet``/``conformance_unit``) is emitted as a Kconfig ``comment``
        containing its OPC display/facet name and ``(NOT IMPLEMENTED)``,
        not as a selectable ``config`` symbol.
      - The computed OPC Kconfig symbol for each such item is not present
        as a ``config`` entry in the generated Kconfig.
      - Via kconfiglib, none of them resolve as a settable symbol in the
        full profile's ``.config``.
    """
    errors: list[str] = []

    kconfig_path = os.path.join(_REPO_ROOT, "Kconfig")
    try:
        with open(kconfig_path, "r", encoding="utf-8") as fh:
            kconfig_text = fh.read()
    except FileNotFoundError:
        errors.append(
            "unimplemented availability: Kconfig not found at " + kconfig_path
        )
        return errors

    from profile_manifest.generate import (  # noqa: E402
        _facet_name,
        compute_kconfig_symbol,
    )

    items = manifest.get("items", [])
    unimplemented = [
        i for i in items
        if isinstance(i, dict) and i.get("implementation_state") in _UNSELECTABLE_STATES
    ]

    if not unimplemented:
        errors.append(
            "unimplemented availability: manifest has no unimplemented/deferred "
            "items; at least one is required to prove unavailable entries are "
            "visible but not selectable in Kconfig"
        )
        return errors

    # Symbols legitimately emitted as selectable ``config`` entries from
    # claimed/implemented items.  The OPC catalog legitimately contains
    # distinct Conformance Units that share a display name (e.g. CU 1673
    # "Attribute Read" from the 2017 facet and CU 3072 "Attribute Read" from
    # the 2022 facet).  When an unimplemented item's computed symbol matches a
    # symbol owned by a *different* claimed/implemented item, the
    # unimplemented duplicate is not itself selectable -- toggling that symbol
    # enables the claimed item's feature -- so it is not a selectability
    # violation (OPC-10000-7 §4.2).  This set lets the collision check below
    # distinguish a true violation from such name-sharing roadmap entries.
    _SELECTABLE_STATES = ("claimed", "implemented")
    legitimate_selectable_symbols: set[str] = set()
    for i in items:
        if not isinstance(i, dict):
            continue
        if i.get("implementation_state") not in _SELECTABLE_STATES:
            continue
        kind = i.get("kind")
        raw = i.get("kconfig_symbol")
        if isinstance(raw, str) and raw:
            legitimate_selectable_symbols.add(raw)
        display = i.get("opc_display_name")
        if isinstance(display, str) and display and kind in ("facet", "conformance_unit"):
            try:
                legitimate_selectable_symbols.add(
                    compute_kconfig_symbol(display, kind)
                )
            except ValueError:
                pass

    at_least_one_visible = False
    kconfig_lines = kconfig_text.splitlines()

    for item in unimplemented:
        item_id = item.get("id", "")
        state = item.get("implementation_state", "")

        sym = item.get("kconfig_symbol")
        if sym:
            errors.append(
                "unimplemented availability: item '" + item_id
                + "' has implementation_state '" + state
                + "' but declares kconfig_symbol '" + sym
                + "' (unimplemented items must not have a selectable symbol)"
            )

        if item_id and item_id in kconfig_text:
            at_least_one_visible = True
        else:
            errors.append(
                "unimplemented availability: item '" + item_id
                + "' does not appear in the generated Kconfig text"
            )

        # OPC items must be visible ``comment`` directives, not selectable
        # ``config`` symbols (OPC-10000-7 §4.2 and each item's opc_reference).
        kind = item.get("kind")
        opc_ref = item.get("opc_reference")
        is_opc_item = (
            isinstance(opc_ref, dict) or kind in ("facet", "conformance_unit")
        )
        if not is_opc_item:
            continue

        expected_name = _facet_name(item)
        if not expected_name:
            display = item.get("opc_display_name")
            if isinstance(display, str) and display:
                expected_name = display

        if expected_name:
            comment_marker = expected_name + " (NOT IMPLEMENTED)"
            has_comment = any(
                ln.startswith("comment ") and comment_marker in ln
                for ln in kconfig_lines
            )
            if has_comment:
                at_least_one_visible = True
            else:
                errors.append(
                    "unimplemented availability: OPC item '" + item_id
                    + "' is not emitted as a Kconfig comment (expected "
                    + "'comment \"" + expected_name
                    + " (NOT IMPLEMENTED) ...'; OPC-10000-7 §4.2)"
                )

        # The item must not be emitted as a selectable ``config`` symbol.
        forbidden_symbols = set()
        if sym:
            forbidden_symbols.add(sym)
        display_name = item.get("opc_display_name")
        if (
            isinstance(display_name, str)
            and display_name
            and kind in ("facet", "conformance_unit")
        ):
            try:
                forbidden_symbols.add(compute_kconfig_symbol(display_name, kind))
            except ValueError:
                pass

        for sym_name in forbidden_symbols:
            config_line = "config " + sym_name
            if not any(ln == config_line for ln in kconfig_lines):
                continue
            # A claimed/implemented item may legitimately own a symbol that a
            # distinct unimplemented OPC item also computes (the OPC catalog
            # has separate CUs sharing a display name, e.g. CU 1673 vs CU 3072
            # both "Attribute Read").  The unimplemented duplicate is not made
            # selectable by that shared config -- selecting it enables the
            # claimed feature -- so this is not a violation (OPC-10000-7 §4.2).
            if sym_name in legitimate_selectable_symbols:
                continue
            errors.append(
                "unimplemented availability: OPC item '" + item_id
                + "' is emitted as selectable config symbol '" + sym_name
                + "' (unimplemented items must be comments, not config; "
                "OPC-10000-7 §4.2)"
            )

    if not at_least_one_visible:
        errors.append(
            "unimplemented availability: none of the manifest's unimplemented "
            "items are visible in the generated Kconfig"
        )

    if _KCONFIG_DIR not in sys.path:
        sys.path.insert(0, _KCONFIG_DIR)

    # See _check_named_profile_resolution for why CONFIG_ must be empty:
    # generated symbols carry the MUC_OPCUA_ prefix and defconfig lines
    # must match verbatim.
    os.environ["CONFIG_"] = ""
    import kconfiglib  # noqa: E402

    full_defconfig = os.path.join(_REPO_ROOT, "configs", "full.defconfig")
    if os.path.isfile(full_defconfig):
        kconf = kconfiglib.Kconfig(kconfig_path, warn=False)
        kconf.load_config(full_defconfig)

        for item in unimplemented:
            item_id = item.get("id", "")
            sym_name = item.get("kconfig_symbol")
            if not sym_name:
                continue

            sym = kconf.syms.get(sym_name)
            if sym is not None and sym.tri_value != 0:
                errors.append(
                    "unimplemented availability: item '" + item_id
                    + "' symbol '" + sym_name
                    + "' is selectable and resolved to y/m in the full profile"
                )

    return errors


def _check_reconciliation_links(manifest: dict) -> list[str]:
    """Validate spec 073 FR-002 reconciliation links.

    `satisfied_by` must point to an existing implemented CU, and `not_applicable`
    must map known profile keys to non-empty reasons.
    """
    errors: list[str] = []
    items = manifest.get("items", [])
    by_id = {it.get("id"): it for it in items if it.get("kind") == "conformance_unit"}
    implemented = {"claimed", "implemented"}
    profile_keys = set(manifest.get("profiles", {}).keys())
    for it in items:
        if it.get("kind") != "conformance_unit":
            continue
        iid = it.get("id", "<unknown>")
        sat = it.get("satisfied_by")
        if sat is not None:
            target = by_id.get(sat)
            if target is None:
                errors.append("reconciliation: '" + str(iid) + "' satisfied_by unknown CU '" + str(sat) + "'")
            elif target.get("implementation_state") not in implemented:
                errors.append(
                    "reconciliation: '" + str(iid) + "' satisfied_by '" + str(sat)
                    + "' which is not implemented/claimed"
                )
        na = it.get("not_applicable")
        if na is not None:
            if not isinstance(na, dict) or not na:
                errors.append("reconciliation: '" + str(iid) + "' not_applicable must be a non-empty mapping")
            else:
                for pkey, reason in na.items():
                    if profile_keys and pkey not in profile_keys:
                        errors.append(
                            "reconciliation: '" + str(iid) + "' not_applicable references unknown profile '"
                            + str(pkey) + "'"
                        )
                    if not isinstance(reason, str) or not reason.strip():
                        errors.append(
                            "reconciliation: '" + str(iid) + "' not_applicable['" + str(pkey)
                            + "'] must have a grounded reason"
                        )
    return errors


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)

    try:
        manifest = load_manifest(args.manifest)
    except (FileNotFoundError, ValueError) as exc:
        print("manifest: FAIL")
        print("  load error: " + str(exc))
        return 1

    # Live join: validate the graph-resolved depends_on/profile_defaults --
    # the same values generate.py would emit -- not the manifest's raw
    # (possibly stale/absent) stored fields. In-memory only.
    graph_path = os.path.join(_REPO_ROOT, "profiles", "opcua-profile-graph.json")
    graph_deps.resolve_into(manifest, graph_deps.load_graph(graph_path))

    errors = validate_manifest(manifest)
    errors.extend(_check_in_scope_071_manifest_requirements(manifest))
    errors.extend(_check_reconciliation_links(manifest))
    if errors:
        print("manifest: FAIL")
        for err in errors:
            print("  - " + err)
        return 1

    if args.all:
        all_errors: list[str] = []
        all_errors.extend(_check_generated(manifest, args.manifest))
        all_errors.extend(_check_kconfig_parse(manifest))
        all_errors.extend(_check_unimplemented_availability(manifest))
        all_errors.extend(_check_capacity_defaults(manifest))
        all_errors.extend(_check_named_profile_resolution(manifest))
        all_errors.extend(_check_profile_override_to_custom(manifest))
        all_errors.extend(_check_claims(manifest))
        all_errors.extend(_check_naming_conventions(manifest))
        all_errors.extend(_check_opc_references_in_help(manifest))

        if all_errors:
            print("manifest: FAIL")
            for err in all_errors:
                print("  - " + err)
            return 1

        print("manifest: OK")
        return 0

    if (
        args.manifest_only
        and not args.check_generated
        and not args.check_capacities
        and not args.check_profiles
        and not args.check_claims
    ):
        print("manifest: OK")
        return 0

    has_checks = (
        args.check_generated
        or args.check_capacities
        or args.check_profiles
        or args.check_claims
    )
    if not has_checks:
        print("manifest: OK")
        return 0

    all_errors: list[str] = []

    if args.check_generated:
        gen_errors = _check_generated(manifest, args.manifest)
        if gen_errors:
            all_errors.extend(gen_errors)

    if args.check_capacities:
        cap_errors = _check_capacity_defaults(manifest)
        if cap_errors:
            all_errors.extend(cap_errors)

    if args.check_profiles:
        profile_errors = _check_named_profile_resolution(manifest)
        if profile_errors:
            all_errors.extend(profile_errors)
        override_errors = _check_profile_override_to_custom(manifest)
        if override_errors:
            all_errors.extend(override_errors)

    if args.check_claims:
        claim_errors = _check_claims(manifest)
        if claim_errors:
            all_errors.extend(claim_errors)

    if all_errors:
        print("validation: FAIL")
        for err in all_errors:
            print("  - " + err)
        return 1

    print("validation: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
