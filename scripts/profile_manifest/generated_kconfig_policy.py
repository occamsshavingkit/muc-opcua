"""Structural policy checks for generated profile-manifest Kconfig output."""

from __future__ import annotations

import re


_KCONFIG_VISIBLE_STATES = frozenset(("claimed", "implemented"))
_NAMED_PROFILE_KEYS = ("nano", "micro", "embedded", "standard", "full")
_KCONFIG_DECLARATION_RE = re.compile(r"^(?:config|menuconfig) ([A-Za-z0-9_]+)\s*$")
_KCONFIG_PROMPT_RE = re.compile(
    r'^\s*(?:bool|tristate|int|string|hex|prompt) "((?:[^"\\]|\\.)*)"\s*$'
)


def _kconfig_declarations(generated_kconfig: str) -> tuple[set[str], set[str]]:
    """Return exact declared symbols and prompt texts from Kconfig blocks."""
    symbols: set[str] = set()
    prompts: set[str] = set()
    in_block = False
    for line in generated_kconfig.splitlines():
        declaration = _KCONFIG_DECLARATION_RE.match(line)
        if declaration:
            symbols.add(declaration.group(1))
            in_block = True
            continue
        if in_block and line and not line[0].isspace():
            in_block = False
        if in_block:
            prompt = _KCONFIG_PROMPT_RE.match(line)
            if prompt:
                prompts.add(prompt.group(1))
    return symbols, prompts


def _display_prompt_variants(item: dict) -> set[str]:
    display_name = item.get("opc_display_name")
    if not isinstance(display_name, str) or not display_name:
        return set()
    variants = {display_name}
    if item.get("kind") == "conformance_unit":
        variants.add("CU: " + display_name)
    return variants


def _check_incomplete_item_visibility(
    manifest: dict,
    generated_kconfig: str,
) -> list[str]:
    errors: list[str] = []
    generated_lines = generated_kconfig.splitlines()
    items = manifest.get("items", [])
    if not isinstance(items, list):
        return errors

    declared_symbols, declared_prompts = _kconfig_declarations(generated_kconfig)
    selectable_prompts: set[str] = set()
    for item in items:
        if not isinstance(item, dict) or item.get("implementation_state") not in _KCONFIG_VISIBLE_STATES:
            continue
        selectable_prompts.update(
            _display_prompt_variants(item) & declared_prompts
        )

    for item in items:
        if not isinstance(item, dict):
            continue
        state = item.get("implementation_state")
        if state in _KCONFIG_VISIBLE_STATES:
            continue

        item_id = str(item.get("id", "<unknown>"))
        display_name = item.get("opc_display_name")
        if (
            state == "unimplemented"
            and item.get("kind") != "optimization"
            and isinstance(display_name, str)
            and display_name
        ):
            comment_display_name = re.sub(r"\s+", " ", display_name).strip()
            expected_comment = (
                'comment "' + comment_display_name + " (NOT IMPLEMENTED)"
            )
            comment_line = next(
                (line for line in generated_lines if expected_comment in line),
                None,
            )
            if comment_line is None:
                errors.append(
                    "Kconfig: unimplemented manifest item '" + item_id
                    + "' is missing its NOT IMPLEMENTED comment"
                )
            else:
                opc_ref = item.get("opc_reference")
                required_source_parts: list[str] = []
                if isinstance(opc_ref, dict):
                    spec = opc_ref.get("spec")
                    section = opc_ref.get("section")
                    cu_id = opc_ref.get("cu_id")
                    facet = opc_ref.get("facet")
                    if spec:
                        required_source_parts.append(str(spec))
                    if section:
                        required_source_parts.append("§" + str(section))
                    if cu_id:
                        required_source_parts.append("CU " + str(cu_id))
                    if facet:
                        required_source_parts.append(str(facet))
                missing_source_parts = [
                    part for part in required_source_parts
                    if part not in comment_line
                ]
                if required_source_parts and (
                    "[" not in comment_line or missing_source_parts
                ):
                    errors.append(
                        "Kconfig: unimplemented manifest item '" + item_id
                        + "' comment is missing OPC source text: "
                        + ", ".join(missing_source_parts)
                    )

        symbol = item.get("kconfig_symbol")
        if isinstance(symbol, str) and symbol and symbol in declared_symbols:
            errors.append(
                "Kconfig: incomplete manifest item '" + item_id
                + "' with implementation_state '" + str(state)
                + "' exposes Kconfig symbol '" + symbol + "'"
            )

        leaked_prompts = _display_prompt_variants(item) & declared_prompts
        leaked_prompts.difference_update(selectable_prompts)
        for display_name in sorted(leaked_prompts):
            errors.append(
                "Kconfig: incomplete manifest item '" + item_id
                + "' with implementation_state '" + str(state)
                + "' exposes OPC display prompt '" + display_name + "'"
            )

    return errors


def check_generated_kconfig_structure(
    manifest: dict,
    generated_kconfig: str,
) -> list[str]:
    errors: list[str] = []

    if "MUC_OPCUA_INTERN_PROFILE_" not in generated_kconfig:
        errors.append(
            "Kconfig: no MUC_OPCUA_INTERN_PROFILE_ symbols found "
            "(expected internal cascade symbols per OPC-10000-7 §4.3; "
            "run generate.py --outputs kconfig to regenerate)"
        )
    if 'menu "Profile:' not in generated_kconfig:
        errors.append(
            "Kconfig: no 'Profile:' menu found in generated Kconfig "
            "(expected at least one menu \"Profile: ...\" per OPC-10000-7 §4.3; "
            "run generate.py --outputs kconfig to regenerate)"
        )

    manifest_profiles = manifest.get("profiles", {})
    for profile_key in _NAMED_PROFILE_KEYS:
        profile = manifest_profiles.get(profile_key, {})
        opc_display_name = profile.get("opc_display_name")
        if not isinstance(opc_display_name, str) or not opc_display_name:
            continue
        expected_menu = 'menu "Profile: ' + opc_display_name + '"'
        if expected_menu not in generated_kconfig:
            errors.append(
                "Kconfig: no 'Profile: " + opc_display_name
                + "' menu found for profile '" + profile_key
                + "' (every named profile must have a drilldown section "
                "even if empty; OPC-10000-7 §4.3; run generate.py "
                "--outputs kconfig to regenerate)"
            )

    if (
        'menu "Facet:' not in generated_kconfig
        and "menuconfig MUC_OPCUA_FACET_" not in generated_kconfig
    ):
        errors.append(
            "Kconfig: no Facet menu or menuconfig found in generated Kconfig "
            "(expected at least one menu \"Facet: ...\" or menuconfig "
            "MUC_OPCUA_FACET_* per OPC-10000-7 §4.2; run generate.py "
            "--outputs kconfig to regenerate)"
        )
    if '"CU:' not in generated_kconfig:
        errors.append(
            "Kconfig: no 'CU:' prompt found in generated Kconfig "
            "(expected at least one bool \"CU: ...\" per OPC-10000-7 §4.2; "
            "run generate.py --outputs kconfig to regenerate)"
        )
    if 'menu "Project options"' not in generated_kconfig:
        errors.append(
            "Kconfig: no 'Project options' menu found in generated Kconfig "
            "(expected menu \"Project options\" to separate non-OPC "
            "optimization items from the OPC Facet/CU tree per "
            "OPC-10000-7 §4.2; run generate.py --outputs kconfig to regenerate)"
        )

    for obsolete_symbol in (
        "MUC_OPCUA_PROFILE_PRESET_",
        "MUC_OPCUA_FACETS_MATCH_",
    ):
        if obsolete_symbol in generated_kconfig:
            errors.append(
                "Kconfig: obsolete symbol prefix '" + obsolete_symbol
                + "' found in generated Kconfig (should use "
                "MUC_OPCUA_INTERN_PROFILE_ cascade symbols; "
                "run generate.py --outputs kconfig to regenerate)"
            )
            break

    errors.extend(_check_incomplete_item_visibility(manifest, generated_kconfig))
    return errors
