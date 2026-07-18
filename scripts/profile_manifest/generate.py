#!/usr/bin/env python3
"""Generate Kconfig and profile defconfigs from the OPC UA profile manifest.

Usage:
    python3 scripts/profile_manifest/generate.py \
        --manifest profiles/opcua-profile-manifest.yaml \
        --outputs kconfig,defconfigs

Standard-library only. The generated files are deterministic: running the
generator twice produces byte-identical output, which ``validate.py
--check-generated`` relies on for drift detection.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_PKG_PARENT = os.path.dirname(_HERE)
_REPO_ROOT = os.path.dirname(_PKG_PARENT)
if _PKG_PARENT not in sys.path:
    sys.path.insert(0, _PKG_PARENT)

from profile_manifest.model import load_manifest, validate_manifest  # noqa: E402
from profile_manifest import completion as _completion  # noqa: E402
from profile_manifest import graph_deps  # noqa: E402  # pylint: disable=wrong-import-position

_DEFAULT_PROFILES = ("nano", "micro", "embedded", "standard", "full", "custom")
_SELECTABLE_STATES = ("claimed", "implemented", "deferred")
_UNSELECTABLE_STATES = ("unimplemented",)

_MARKER_ID_RENAMES: dict[str, str] = {
    "STANDARD_PROFILE": "MUC_OPCUA_MARKER_STANDARD_PROFILE",
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _profile_symbol(profile_key: str) -> str:
    return "PROFILE_" + profile_key.upper()


def _resolve_profile_symbol(profiles: dict, profile_key: str) -> str:
    """Resolve the Kconfig profile symbol for *profile_key* from manifest data.

    Mirrors the symbol resolution used by the profile ``choice`` entries and
    ``generate_defconfig()``: ``custom`` always maps to
    ``MUC_OPCUA_PROFILE_CUSTOM``; named profiles with an ``opc_display_name``
    map to their computed ``MUC_OPCUA_PROFILE_<NAME>`` symbol (OPC-10000-7
    §4.3); otherwise the manifest profile ``id`` (or legacy
    ``PROFILE_<KEY>`` fallback) is returned.
    """
    if profile_key == "custom":
        return "MUC_OPCUA_PROFILE_CUSTOM"
    profile = profiles.get(profile_key, {})
    opc_display_name = profile.get("opc_display_name")
    if isinstance(opc_display_name, str) and opc_display_name:
        return compute_kconfig_symbol(opc_display_name, "profile")
    return profile.get("id", _profile_symbol(profile_key))


def _internal_profile_symbol(profile_symbol: str) -> str:
    """Compute the invisible ``MUC_OPCUA_INTERN_PROFILE_<TOKEN>`` cascade symbol.

    The internal cascade symbols encode the profile inclusion hierarchy
    (standard includes embedded includes micro includes nano; full includes
    standard).  Profile selection seeds defaults only: defconfigs set the
    choice symbol, the internal cascade activates lower-profile symbols, and
    Facet / CU / capacity ``default`` expressions reference the lowest
    internal profile symbol that reaches them (OPC-10000-7 §4.3).

    ``custom`` maps to itself (it has no cascade symbol and uses the choice
    symbol directly in default expressions).
    """
    prefix = "MUC_OPCUA_PROFILE_"
    if profile_symbol.startswith(prefix):
        token = profile_symbol[len(prefix):]
    else:
        token = profile_symbol
    if profile_symbol == "MUC_OPCUA_PROFILE_CUSTOM":
        return profile_symbol
    return "MUC_OPCUA_INTERN_PROFILE_" + token


def _default_condition_symbol(
    profile_key: str, profile_symbols: dict[str, str],
) -> str:
    """Return the Kconfig symbol to reference in ``default ... if`` conditions.

    Named profiles use their invisible internal cascade symbol
    (:func:`_internal_profile_symbol`).  ``custom`` uses the effective
    choice symbol ``MUC_OPCUA_PROFILE_CUSTOM`` directly (it has no cascade
    symbol and is the hand-selected baseline).
    """
    if profile_key == "custom":
        return profile_symbols[profile_key]
    return _internal_profile_symbol(profile_symbols[profile_key])


# Cascade order from lowest to highest (OPC-10000-7 §4.3).  Selecting a
# higher profile auto-activates all lower internal profile symbols.
_CASCADE_ORDER = ("nano", "micro", "embedded", "standard", "full")


def _lowest_default_profile(item: dict) -> str | None:
    """Return the lowest named profile key where *item* defaults true.

    Cascade order: nano < micro < embedded < standard < full.
    Returns ``None`` when the item is not true in any named profile.
    """
    pd = item.get("profile_defaults") or {}
    for pk in _CASCADE_ORDER:
        if pd.get(pk) is True:
            return pk
    return None


def _item_prompt(item: dict) -> str:
    """Return a human-readable menu prompt for *item*.

    Uses the item's ``opc_display_name`` when present (non-empty); otherwise
    falls back to deriving a prompt from the item id.
    """
    display_name = item.get("opc_display_name")
    if isinstance(display_name, str) and display_name:
        return display_name
    item_id = item.get("id", "")
    return item_id.replace("_", " ").capitalize()


def _cu_symbol(item: dict) -> str | None:
    """Return the selectable Kconfig symbol for a Conformance Unit item."""
    sym = item.get("kconfig_symbol")
    if isinstance(sym, str) and sym:
        return sym
    opc_display_name = item.get("opc_display_name")
    if isinstance(opc_display_name, str) and opc_display_name:
        return compute_kconfig_symbol(opc_display_name, "conformance_unit")
    return None


def _facet_name(item: dict) -> str:
    """Return a human-readable facet/CU name for *item*.

    Falls back to the item-id-derived prompt when no OPC facet name is present.
    """
    opc_ref = item.get("opc_reference")
    if isinstance(opc_ref, dict):
        facet = opc_ref.get("facet")
        if isinstance(facet, str) and facet:
            return facet
    return _item_prompt(item)


def _opc_source_string(opc_ref: dict | None) -> str | None:
    """Build a compact ``spec §section`` string from an opc_reference."""
    if not isinstance(opc_ref, dict):
        return None
    spec = opc_ref.get("spec")
    section = opc_ref.get("section")
    parts: list[str] = []
    if spec:
        parts.append(spec)
    if section:
        parts.append("§" + str(section))
    return " ".join(parts) if parts else None


def _opc_detail_string(opc_ref: dict | None) -> str | None:
    """Build a facet / CU detail string from an opc_reference."""
    if not isinstance(opc_ref, dict):
        return None
    facet = opc_ref.get("facet")
    cu_id = opc_ref.get("cu_id")
    cu_ids = opc_ref.get("cu_ids")
    cu_name = opc_ref.get("cu_name")
    parts: list[str] = []
    if facet:
        parts.append(str(facet))
    if cu_id:
        parts.append("CU " + str(cu_id) + ((":" + cu_name) if cu_name else ""))
    elif cu_ids and isinstance(cu_ids, list):
        parts.append("CUs " + ", ".join(str(c) for c in cu_ids) + ((":" + cu_name) if cu_name else ""))
    elif cu_name:
        parts.append(str(cu_name))
    return " -- ".join(parts) if parts else None


def _sanitize_kconfig_text(value: str | None) -> str:
    """Render free-form manifest text safe for a single Kconfig line.

    Manifest notes (and other imported prose) may contain embedded newlines
    and inline HTML such as ``<br>`` or ``<a href="...">`` imported from the
    OPC profile snapshot.  Kconfig ``#`` comment lines and ``help`` bodies
    are parsed line-by-line: a bare continuation line without the leading
    ``#`` / ``\\t  `` prefix is tokenized as Kconfig syntax, and a fragment
    like ``href="..."`` is then misread as an assignment, which aborts
    parsing.  Stripping HTML tags and collapsing all whitespace (including
    newlines) to single spaces keeps each emitted comment/help line on one
    physical line so kconfiglib parses the generated file cleanly.
    """
    if not isinstance(value, str) or not value:
        return ""
    stripped = re.sub(r"<[^>]*>", "", value)
    return re.sub(r"\s+", " ", stripped).strip()


def _depends_expr(depends_on: list[str], op: str | None) -> str | None:
    """Render a ``depends on`` expression (conjunction by default)."""
    if not depends_on:
        return None
    if op == "or":
        return " || ".join(depends_on)
    if op == "and":
        return " && ".join(depends_on)
    if len(depends_on) == 1:
        return depends_on[0]
    return " && ".join(depends_on)


_KCONFIG_KIND_PREFIXES: dict[str, str] = {
    "profile": "MUC_OPCUA_PROFILE_",
    "facet": "MUC_OPCUA_FACET_",
    "conformance_unit": "MUC_OPCUA_CU_",
    "optimization": "MUC_OPCUA_OPT_",
}

_KCONFIG_TRAILING_SUFFIXES: dict[str, tuple[str, ...]] = {
    "profile": ("profile",),
    "facet": ("facet",),
    "optimization": ("optimization",),
    "conformance_unit": ("conformance unit", "cu"),
}


def compute_kconfig_symbol(opc_display_name: str, kind: str) -> str:
    """Compute the Kconfig symbol for an OPC display name and item kind.

    Implements the naming algorithm from research.md Decision 3: strip a
    redundant trailing kind word (e.g. ``Profile`` for profiles), uppercase,
    replace runs of non-alphanumeric characters with ``_``, collapse
    underscores, trim, and prefix with the kind-specific ``MUC_OPCUA_*_``
    prefix.

    Raises :class:`ValueError` when *kind* is not one of ``profile``,
    ``facet``, ``conformance_unit``, or ``optimization``, or when the
    resulting token is empty.
    """
    prefixes = _KCONFIG_KIND_PREFIXES
    if kind not in prefixes:
        raise ValueError(
            "Unsupported kind '" + str(kind) + "'; expected one of: "
            + ", ".join(sorted(prefixes))
        )

    name = opc_display_name.strip()
    lower = name.lower()
    for suffix in _KCONFIG_TRAILING_SUFFIXES[kind]:
        trailing = " " + suffix
        if lower.endswith(trailing):
            name = name[: -len(trailing)]
            break

    token = re.sub(r"[^0-9A-Z]+", "_", name.upper()).strip("_")
    if not token:
        raise ValueError(
            "Computed Kconfig symbol token is empty for opc_display_name='"
            + str(opc_display_name) + "' (kind='" + str(kind) + "')"
        )

    return prefixes[kind] + token


def _contained_cu_ids(manifest: dict) -> set[str]:
    """Return the set of CU item IDs listed in ``facet_containment``.

    These Conformance Units are emitted under their canonical Facet menu
    (OPC-10000-7 §4.2) and must be skipped in the flat emission to avoid
    duplicate Kconfig ``config`` entries.
    """
    facet_containment = manifest.get("facet_containment")
    if not isinstance(facet_containment, dict):
        return set()
    ids: set[str] = set()
    for cu_list in facet_containment.values():
        if isinstance(cu_list, list):
            for cu_id in cu_list:
                if isinstance(cu_id, str):
                    ids.add(cu_id)
    return ids


# ---------------------------------------------------------------------------
# Profile facets-match helpers (T036, OPC-10000-7 §4.2 and §4.3)
# ---------------------------------------------------------------------------

def _selectable_facet_toggles(manifest: dict) -> list[tuple[str, dict]]:
    """Return ``(facet_symbol, facet_item)`` pairs for emitted facet toggles.

    Mirrors the emission and de-duplication logic of
    :func:`_emit_facet_menus` so the facets-match comparison only
    references Facet group toggle symbols that are actually emitted as
    ``config MUC_OPCUA_FACET_<NAME>`` entries (OPC-10000-7 §4.2).
    Unimplemented / deferred Facets have no toggle symbol and are excluded.

    Iteration order follows ``facet_containment`` manifest order for
    deterministic output.
    """
    facet_containment = manifest.get("facet_containment")
    if not isinstance(facet_containment, dict):
        return []
    items = manifest.get("items", [])
    items_by_id = {
        i.get("id"): i for i in items if isinstance(i, dict) and i.get("id")
    }
    result: list[tuple[str, dict]] = []
    seen: set[str] = set()
    for facet_id in facet_containment:
        facet_item = items_by_id.get(facet_id)
        if not isinstance(facet_item, dict):
            continue
        if facet_item.get("implementation_state") not in _SELECTABLE_STATES:
            continue
        symbol = _facet_toggle_symbol(facet_item)
        if not symbol or symbol in seen:
            continue
        seen.add(symbol)
        result.append((symbol, facet_item))
    return result


def _selectable_contained_cus(manifest: dict) -> list[tuple[str, dict]]:
    """Return ``(cu_symbol, cu_item)`` pairs for emitted contained CU configs.

    Mirrors the containment iteration of :func:`_emit_facet_menus` so the
    facets-match comparison only references CU symbols that are actually
    emitted as ``config MUC_OPCUA_CU_<NAME>`` entries under their Facet
    menu (OPC-10000-7 §4.2 and each CU item's ``opc_reference``).
    Unimplemented / deferred CUs have no config symbol and are excluded.

    Iteration order follows ``facet_containment`` manifest order, then the
    contained CU list order within each facet, for deterministic output.
    Duplicate CU symbols (from a CU listed under multiple facets) are
    suppressed.
    """
    facet_containment = manifest.get("facet_containment")
    if not isinstance(facet_containment, dict):
        return []
    items = manifest.get("items", [])
    items_by_id = {
        i.get("id"): i for i in items if isinstance(i, dict) and i.get("id")
    }
    result: list[tuple[str, dict]] = []
    seen: set[str] = set()
    for facet_id in facet_containment:
        cu_ids = facet_containment[facet_id]
        if not isinstance(cu_ids, list):
            continue
        for cu_id in cu_ids:
            if not isinstance(cu_id, str):
                continue
            cu_item = items_by_id.get(cu_id)
            if not isinstance(cu_item, dict):
                continue
            if cu_item.get("implementation_state") not in _SELECTABLE_STATES:
                continue
            symbol = _cu_symbol(cu_item)
            if not symbol or symbol in seen:
                continue
            seen.add(symbol)
            result.append((symbol, cu_item))
    return result


def _facet_match_expression(
    profile_key: str,
    facets: list[tuple[str, dict]],
    cus: list[tuple[str, dict]],
) -> str | None:
    """Build the Facet + CU comparison expression for *profile_key*.

    For each selectable Facet with an emitted toggle symbol, requires the
    toggle to match that profile's Facet default (OPC-10000-7 §4.2 and
    §4.3): ``MUC_OPCUA_FACET_<NAME>`` when the profile default is true,
    ``!MUC_OPCUA_FACET_<NAME>`` when false.

    For each selectable contained CU with an emitted config symbol,
    requires the CU to match that profile's CU default (OPC-10000-7 §4.2,
    §4.3, and each CU item's ``opc_reference``): ``MUC_OPCUA_CU_<NAME>``
    when the profile default is true, ``!MUC_OPCUA_CU_<NAME>`` when false.

    CU comparisons are ANDed with the Facet comparisons (T038) so the
    match helper only evaluates to ``y`` when both Facet toggles and CU
    configs exactly match the profile's controlled defaults.  Returns
    ``None`` when there are no controlled Facets and no controlled CUs,
    so the caller can keep the safe ``default n`` fallback without
    emitting a misleading positive condition.
    """
    parts: list[str] = []
    for symbol, facet_item in facets:
        pd = facet_item.get("profile_defaults")
        default_true = isinstance(pd, dict) and pd.get(profile_key) is True
        if default_true:
            parts.append(symbol)
        else:
            parts.append("!" + symbol)
    for symbol, cu_item in cus:
        pd = cu_item.get("profile_defaults")
        default_true = isinstance(pd, dict) and pd.get(profile_key) is True
        if default_true:
            parts.append(symbol)
        else:
            parts.append("!" + symbol)
    if not parts:
        return None
    return " && ".join(parts)


def _emit_internal_profile_symbols(
    lines: list[str], profile_symbols: dict[str, str], profiles: dict,
    facet_implies: dict[str, list[str]] | None = None,
) -> None:
    """Emit invisible ``MUC_OPCUA_INTERN_PROFILE_<TOKEN>`` cascade symbols.

    The cascade encodes the profile inclusion hierarchy
    (OPC-10000-7 §4.3): selecting a higher named profile auto-activates all
    lower internal profile symbols.  ``full`` activates ``standard``;
    ``standard`` activates ``embedded``; ``embedded`` activates ``micro``;
    ``micro`` activates ``nano``.

    Facet / CU / capacity ``default`` expressions reference the lowest
    internal profile symbol that reaches them, so a single condition
    (e.g. ``default y if MUC_OPCUA_INTERN_PROFILE_NANO_<TOKEN>``) covers
    every profile at that tier and above.  ``custom`` has no internal symbol;
    it uses the effective choice symbol directly.

    When *facet_implies* is provided, each cascade symbol also carries
    ``imply <facet>`` lines for every facet whose minimum (lowest) named
    profile matches that cascade level.  Because higher named profiles
    activate lower cascade symbols, selecting a higher profile transitively
    implies all facets at or below that tier -- exactly the "changing profile
    resets facet defaults" behaviour (OPC-10000-7 §4.3).  ``imply`` nudges
    the default to ``y`` while still letting the user turn a facet off, so
    it composes safely with the facet's own ``default y if`` line.
    """
    lines.append("# -- Internal profile cascade symbols (OPC-10000-7 §4.3) --------------")
    lines.append("# Invisible bool symbols encoding the profile inclusion hierarchy.")
    lines.append("# Selecting a higher named profile auto-activates all lower cascade")
    lines.append("# symbols.  Facet / CU / capacity defaults reference the lowest internal")
    lines.append("# profile symbol that reaches them.  custom has no cascade symbol.")
    if facet_implies:
        lines.append("# Each cascade symbol also implies the Facet toggles whose minimum")
        lines.append("# profile matches this level so a profile switch re-seeds facet defaults.")
    lines.append("")

    # Emit in reverse cascade order (full -> nano) so each symbol can
    # reference the one above it.  full is the top of the chain.
    for pk in reversed(_CASCADE_ORDER):
        intern = _internal_profile_symbol(profile_symbols[pk])
        choice = profile_symbols[pk]
        lines.append("config " + intern)
        lines.append("\tbool")
        lines.append("\tdefault y if " + choice)
        # Cascade: each level also activates from the next-higher internal symbol.
        higher = _cascade_higher(pk)
        if higher is not None:
            higher_intern = _internal_profile_symbol(profile_symbols[higher])
            lines.append("\tdefault y if " + higher_intern)
        # imply: nudge facet defaults so a profile switch re-seeds them.
        if facet_implies:
            for facet_sym in facet_implies.get(pk, []):
                lines.append("\timply " + facet_sym)
        lines.append("")

    lines.append("")


def _cascade_higher(pk: str) -> str | None:
    """Return the next-higher profile key in the cascade, or ``None``."""
    idx = _CASCADE_ORDER.index(pk)
    if idx + 1 < len(_CASCADE_ORDER):
        return _CASCADE_ORDER[idx + 1]
    return None


# ---------------------------------------------------------------------------
# Kconfig generation
# ---------------------------------------------------------------------------

def generate_kconfig(manifest: dict) -> str:
    """Return the full contents of the generated ``Kconfig`` file."""
    lines: list[str] = []

    # -- Header -----------------------------------------------------------
    lines.append("# muc-opcua feature configuration (Kconfig, consumed via kconfiglib).")
    lines.append("#")
    lines.append("# Generated from profiles/opcua-profile-manifest.yaml by")
    lines.append("# scripts/profile_manifest/generate.py -- do not edit; regenerate with:")
    lines.append("#   python3 scripts/profile_manifest/generate.py \\")
    lines.append("#       --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig")
    lines.append("#")
    lines.append("# Symbol names are bare here; the build sets CONFIG_=MUC_OPCUA_ so the generated")
    lines.append("# autoconf.h emits `#define MUC_OPCUA_<SYM> 1` (undefined for n -- #ifdef-safe) and")
    lines.append("# .config/defconfig lines read `MUC_OPCUA_<SYM>=y`.  C sources keep their existing")
    lines.append("# MUC_OPCUA_* macro names verbatim (no #ifdef-site changes).")
    lines.append("#")
    lines.append("# Model:")
    lines.append("#   - `depends on`              = hard facet containment / build requirement.")
    lines.append("#   - `default y if INTERN_*`   = internal cascade seeds defaults by lowest profile.")
    lines.append("#   - Profile choice seeds defaults only; all facet/CU menus stay globally editable.")
    lines.append("#   - Profiles are defconfigs (configs/<profile>.defconfig) selecting the choice.")
    lines.append("#   - Unimplemented OPC items appear as Kconfig ``comment`` directives so they")
    lines.append("#     are always visible in menuconfig but cannot be toggled (no config symbol).")
    lines.append("")

    # -- mainmenu ---------------------------------------------------------
    lines.append('mainmenu "muc-opcua configuration"')
    lines.append("")

    # -- Profile choice ---------------------------------------------------
    # Profile selection seeds defaults only; it never gates menu visibility
    # or auto-switches to custom.  The defconfig (or user) sets the choice
    # symbol directly (e.g. ``MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER=y``).
    # Feature edits below change the resolved facet/CU set but never change
    # which choice symbol is selected.  ``custom`` is the choice-level
    # fallback default (used when no defconfig selects a named profile).
    profiles = manifest.get("profiles", {})

    lines.append("choice")
    lines.append('\tprompt "Server profile"')
    lines.append("\tdefault MUC_OPCUA_PROFILE_CUSTOM")
    lines.append("\thelp")
    lines.append("\t  Starting facet set for the OPC UA Server.  Each named tier seeds")
    lines.append("\t  its default facets; individual facets below remain togglable without")
    lines.append('\t  changing the selected profile.  "custom" starts from nothing.')
    lines.append("")

    for pk in _DEFAULT_PROFILES:
        profile = profiles.get(pk, {})
        pid = profile.get("id", _profile_symbol(pk))
        display = profile.get("display", pk)
        uri = profile.get("opc_profile_uri")
        opc_display_name = profile.get("opc_display_name")
        if pk == "custom":
            symbol = "MUC_OPCUA_PROFILE_CUSTOM"
            prompt = display
        elif isinstance(opc_display_name, str) and opc_display_name:
            symbol = compute_kconfig_symbol(opc_display_name, "profile")
            prompt = opc_display_name
        else:
            symbol = pid
            prompt = display
        lines.append("config " + symbol)
        lines.append('\tbool "' + prompt + '"')
        # No per-symbol default: the choice-level ``default`` directive above
        # picks custom as the fallback.  Named profiles are selected by the
        # defconfig setting their choice symbol to y.
        if uri:
            lines.append("\thelp")
            lines.append("\t  OPC UA Server Profile URI: " + uri)
            lines.append("")
        else:
            lines.append("")

    lines.append("endchoice")
    lines.append("")

    profile_symbols = {
        pk: _resolve_profile_symbol(profiles, pk)
        for pk in _DEFAULT_PROFILES
    }

    # -- Advertised profile markers ---------------------------------------
    for marker in manifest.get("advertised_profile_markers", []):
        note = marker.get("note")
        if note:
            lines.append("# " + note)
        marker_id = _MARKER_ID_RENAMES.get(marker["id"], marker["id"])
        lines.append("config " + marker_id)
        lines.append("\tbool")
        conditions = marker.get("default_if", [])
        if conditions:
            lines.append("\tdefault y if " + " || ".join(conditions))
        else:
            lines.append("\tdefault n")
        lines.append("")

    # -- Internal profile cascade symbols --------------------------------
    # Invisible bool symbols encoding the profile inclusion hierarchy.
    # Facet / CU / capacity defaults reference these so a single condition
    # covers every profile at that tier and above.  Each cascade symbol also
    # implies the Facet toggles whose minimum profile matches this level so a
    # profile switch re-seeds facet defaults (OPC-10000-7 §4.3).
    items = manifest.get("items", [])
    items_by_id = {
        i.get("id"): i for i in items if isinstance(i, dict) and i.get("id")
    }
    facet_implies = _compute_facet_implies(manifest, items_by_id)
    _emit_internal_profile_symbols(
        lines, profile_symbols, profiles, facet_implies,
    )

    # -- Server menu ------------------------------------------------------
    lines.append('menu "OPC UA Facets and Conformance Units"')
    lines.append("")

    # Contained CUs are emitted under their canonical Facet menu below
    # (OPC-10000-7 §4.2); they are skipped in the flat emission to avoid
    # duplicate Kconfig ``config`` entries.
    contained_cus = _contained_cu_ids(manifest)

    # -- Profile drilldown sections (OPC-named facet/CU symbols) ----------
    _emit_profile_sections(lines, manifest, items_by_id, profile_symbols, profiles)

    # -- Legacy flat symbols (C source compatibility) --------------------
    # These project-centric symbols (BASE_NODES, SECURITY, etc.) are emitted
    # alongside the OPC-named symbols until source migration removes them.
    # They follow the same internal-cascade default logic.
    # Exclude facet items already shown in profile sections (facet_containment keys).
    facet_ids = set(manifest.get("facet_containment", {}).keys()) if isinstance(
        manifest.get("facet_containment"), dict
    ) else set()
    selectable_flat = [
        i for i in items
        if i.get("implementation_state") in _SELECTABLE_STATES
        and i.get("kind") != "optimization"
        and i.get("id") not in contained_cus
        and i.get("id") not in facet_ids
    ]
    if selectable_flat:
        lines.append('comment "Additional selectable items"')
        lines.append("")
        for item in selectable_flat:
            _emit_selectable(lines, item, profile_symbols)

    # -- Unimplemented items (visible comments, not selectable) ----------
    unselectable_flat = [
        i for i in items
        if i.get("implementation_state") in _UNSELECTABLE_STATES
        and i.get("kind") != "optimization"
        and i.get("id") not in contained_cus
        and i.get("id") not in facet_ids
    ]
    if unselectable_flat:
        lines.append('comment "Unimplemented OPC items (visible but not selectable)"')
        lines.append("")
        for item in unselectable_flat:
            _emit_unselectable(lines, item)

    lines.append("endmenu")
    lines.append("")

    # -- Project options (non-OPC optimization items) --------------------
    # Optimization items are project implementation controls, not OPC UA
    # Facets or Conformance Units.  They are emitted under a separate menu
    # to keep the OPC tree (OPC-10000-7 §4.2) structurally clean.
    optimization_items = [
        i for i in items
        if i.get("kind") == "optimization"
        and i.get("implementation_state") in _SELECTABLE_STATES
    ]
    if optimization_items:
        lines.append('menu "Project options"')
        lines.append("")
        for item in optimization_items:
            _emit_selectable(lines, item, profile_symbols)
        lines.append("endmenu")
        lines.append("")

    # -- Capacities menu --------------------------------------------------
    capacities = manifest.get("capacities", [])
    if capacities:
        lines.append('menu "Capacities"')
        lines.append("")
        lines.append("# Capacity int symbols.  Defaults come from the manifest; legacy")
        lines.append("# -DMU_MAX_*=N overrides are translated by CMakeLists.txt into")
        lines.append("# MUC_OPCUA_<SYM>=N lines in the override fragment so the resolved")
        lines.append("# value visible in .config / menuconfig matches the compile-time value.")
        lines.append("# C sources still consume MU_INTERN_* macros from the generated")
        lines.append("# include/muc_opcua/capacities.h (same default<profile<user cascade).")
        lines.append("")
        for cap in capacities:
            _emit_capacity_kconfig(lines, cap, profile_symbols)
        lines.append("endmenu")
        lines.append("")

    # -- Footer -----------------------------------------------------------
    lines.append("# The Client surface (MUC_OPCUA_CLIENT_NANO) is driven by the separate")
    lines.append("# MUC_OPCUA_CLIENT_PROFILE axis in CMake, not this Server tree.")
    lines.append("")

    return "\n".join(lines)


def _emit_selectable(
    lines: list[str],
    item: dict,
    profile_symbols: dict[str, str],
    facet_symbol: str | None = None,
) -> None:
    """Emit a selectable Kconfig ``config`` entry for *item*.

    When *facet_symbol* is set, the CU ``select``s its parent facet so
    toggling any CU auto-enables the facet.  CUs are independently
    selectable regardless of the facet toggle state.  The facet acts as
    a visual grouping and convenience group-toggle with tristate display
    (    all-on / all-off / partial).
    """
    sym = _cu_symbol(item) if item.get("kind") == "conformance_unit" else item.get("kconfig_symbol")
    if not sym:
        return

    prompt = _item_prompt(item)
    if item.get("kind") == "conformance_unit":
        opc_display_name = item.get("opc_display_name")
        if isinstance(opc_display_name, str) and opc_display_name:
            prompt = "CU: " + opc_display_name

    lines.append("config " + sym)
    lines.append('\tbool "' + prompt + '"')

    depends_on = item.get("depends_on") or []
    depends_expr = _depends_expr(depends_on, item.get("depends_on_op"))
    if depends_expr:
        lines.append("\tdepends on " + depends_expr)

    _emit_default(lines, item, profile_symbols)

    _emit_help(lines, item)
    lines.append("")


def _emit_unselectable(lines: list[str], item: dict) -> None:
    """Emit a visible, non-selectable Kconfig ``comment`` for an unimplemented item.

    Kconfig ``comment`` directives are always visible in menuconfig regardless of
    dependency state, unlike prompted ``bool`` symbols whose prompts can be hidden
    when ``depends on`` is unmet.  Because ``comment`` blocks cannot carry ``help``
    text in standard Kconfig, source/help context is emitted as ``#`` comment lines
    immediately above the ``comment`` directive.  No config symbol is emitted, so the
    item cannot appear in ``.config`` / ``config.cmake`` output and cannot be set.
    """
    name = _facet_name(item)
    opc_ref = item.get("opc_reference")
    source = _opc_source_string(opc_ref if isinstance(opc_ref, dict) else None)
    detail = _opc_detail_string(opc_ref if isinstance(opc_ref, dict) else None)
    state = item.get("implementation_state", "unknown")
    item_id = item.get("id", "")

    # Context comment lines (readable in the Kconfig file, not in menuconfig).
    lines.append("# " + name + " (" + item_id + ")")
    lines.append("#   Implementation state: " + state + " -- visible but not selectable.")
    if source:
        lines.append("#   OPC source: " + source + ".")
    if detail:
        lines.append("#   Detail: " + detail + ".")
    source_meta = item.get("source_metadata")
    if isinstance(source_meta, dict):
        imported_from = source_meta.get("imported_from")
        if imported_from:
            lines.append("#   Imported from: " + imported_from + ".")
    notes = item.get("notes")
    if notes:
        lines.append("#   Notes: " + _sanitize_kconfig_text(notes))

    # Visible comment directive in menuconfig (always shown, never toggleable).
    label = "DOCUMENTED" if state == "documented" else "NOT IMPLEMENTED"
    if source:
        lines.append('comment "' + name + ' (' + label + ') [' + source + ']"')
    else:
        lines.append('comment "' + name + ' (' + label + ')"')
    lines.append("")


def _facet_toggle_symbol(facet_item: dict) -> str | None:
    """Compute the Facet group toggle symbol ``MUC_OPCUA_FACET_<NAME>``.

    Uses :func:`compute_kconfig_symbol` with the facet's
    ``opc_display_name`` (OPC-10000-7 §4.2 and the facet item's
    ``opc_reference``) when present; otherwise falls back to the item's
    existing ``kconfig_symbol`` field to preserve legacy behavior when the
    OPC display name is unavailable.
    """
    opc_display_name = facet_item.get("opc_display_name")
    if isinstance(opc_display_name, str) and opc_display_name:
        return compute_kconfig_symbol(opc_display_name, "facet")
    return facet_item.get("kconfig_symbol")


def _facet_default_profile_keys(
    facet_item: dict, contained_cu_items: list[dict],
) -> list[str]:
    """Return the ordered profile keys driving the Facet toggle default.

    Prefers the facet item's own ``profile_defaults`` when present (these
    reflect existing manifest semantics, OPC-10000-7 §4.2 and §4.3).
    Otherwise derives the union of profile keys where any contained,
    selectable (implemented/claimed) Conformance Unit defaults to true,
    so the Facet toggle tracks the same profile presets that enable its
    contained CUs (OPC-10000-7 §4.2).

    Returns keys in ``_DEFAULT_PROFILES`` order for deterministic output.
    """
    profile_defaults = facet_item.get("profile_defaults")
    if isinstance(profile_defaults, dict) and profile_defaults:
        return [
            pk for pk in _DEFAULT_PROFILES
            if profile_defaults.get(pk) is True
        ]
    derived: set[str] = set()
    for cu in contained_cu_items:
        if not isinstance(cu, dict):
            continue
        if cu.get("implementation_state") not in _SELECTABLE_STATES:
            continue
        cu_pd = cu.get("profile_defaults")
        if not isinstance(cu_pd, dict):
            continue
        for pk in _DEFAULT_PROFILES:
            if cu_pd.get(pk) is True:
                derived.add(pk)
    return [pk for pk in _DEFAULT_PROFILES if pk in derived]


def _emit_facet_toggle_default(
    lines: list[str], facet_item: dict, contained_cu_items: list[dict],
    profile_symbols: dict[str, str],
) -> None:
    """Emit the ``default`` line for a Facet group toggle.

    Uses the lowest internal profile symbol that reaches the facet (or any
    of its contained CUs) so a single cascade condition covers every profile
    at that tier and above (OPC-10000-7 §4.2 and §4.3).  Does not emit
    unconditional defaults unless the facet item explicitly carries
    ``default_unconditional: true``.
    """
    if facet_item.get("default_unconditional") is True:
        lines.append("\tdefault y")
        return
    true_profiles = _facet_default_profile_keys(facet_item, contained_cu_items)
    # Find the lowest profile in cascade order and handle custom separately.
    lowest: str | None = None
    for pk in _CASCADE_ORDER:
        if pk in true_profiles:
            lowest = pk
            break
    custom_true = "custom" in true_profiles
    conditions: list[str] = []
    if lowest is not None:
        conditions.append(_default_condition_symbol(lowest, profile_symbols))
    if custom_true:
        conditions.append(_default_condition_symbol("custom", profile_symbols))
    if conditions:
        lines.append("\tdefault y if " + " || ".join(conditions))
    else:
        lines.append("\tdefault n")


def _facet_lowest_profile(
    facet_item: dict, contained_cu_items: list[dict],
) -> str | None:
    """Return the lowest named profile key where the facet (or its CUs) defaults true."""
    true_profiles = _facet_default_profile_keys(facet_item, contained_cu_items)
    for pk in _CASCADE_ORDER:
        if pk in true_profiles:
            return pk
    return None


def _compute_facet_implies(
    manifest: dict, items_by_id: dict[str, dict],
) -> dict[str, list[str]]:
    """Map each cascade profile key to the facet symbols it should imply.

    For every selectable facet, finds its lowest named profile (the cascade
    level where it first defaults to y) and maps that level to the facet's
    Kconfig toggle symbol.  The resulting dict is consumed by
    :func:`_emit_internal_profile_symbols` to emit ``imply`` directives on
    each internal cascade symbol so a profile switch re-seeds facet defaults
    (OPC-10000-7 §4.3).  Facet symbols within each level are sorted for
    deterministic output.
    """
    facet_containment = manifest.get("facet_containment")
    result: dict[str, list[str]] = {pk: [] for pk in _CASCADE_ORDER}
    if not isinstance(facet_containment, dict):
        return result
    for facet_id in facet_containment:
        facet_item = items_by_id.get(facet_id)
        if not isinstance(facet_item, dict):
            continue
        if facet_item.get("implementation_state") not in _SELECTABLE_STATES:
            continue
        facet_symbol = _facet_toggle_symbol(facet_item)
        if not facet_symbol:
            continue
        cu_ids = facet_containment[facet_id]
        contained_cu_items: list[dict] = []
        if isinstance(cu_ids, list):
            for cu_id in cu_ids:
                cu_item = items_by_id.get(cu_id)
                if isinstance(cu_item, dict):
                    contained_cu_items.append(cu_item)
        lowest = _facet_lowest_profile(facet_item, contained_cu_items)
        if lowest is not None and lowest in result:
            result[lowest].append(facet_symbol)
    for pk in result:
        result[pk].sort()
    return result


def _emit_one_facet_menu(
    lines: list[str], facet_id: str, facet_item: dict,
    facet_containment: dict, items_by_id: dict,
    profile_symbols: dict[str, str],
    emitted_facet_symbols: set[str],
) -> None:
    """Emit a single facet as ``menuconfig`` with its toggle and contained CUs.

    Selectable facets use the ``menuconfig <SYM>`` / ``if <SYM>`` / ``endif``
    pattern (OPC-10000-7 §4.2).  The ``if`` block gates every contained CU so
    CU entries inside the block omit the per-CU ``depends on <SYM>`` (the
    ``if`` already enforces facet-off → CU-off, preserving group-off
    behaviour).  Unselectable (unimplemented) facets retain the ``menu`` /
    ``comment`` / ``endmenu`` wrapper because they have no toggle symbol to
    drive a ``menuconfig`` header.
    """
    state = facet_item.get("implementation_state")
    if state not in _SELECTABLE_STATES and state not in _UNSELECTABLE_STATES:
        return
    prompt = _item_prompt(facet_item)

    cu_ids = facet_containment[facet_id]
    contained_cu_items: list[dict] = []
    if isinstance(cu_ids, list):
        for cu_id in cu_ids:
            if not isinstance(cu_id, str):
                continue
            cu_item = items_by_id.get(cu_id)
            if isinstance(cu_item, dict):
                contained_cu_items.append(cu_item)

    facet_symbol = (
        _facet_toggle_symbol(facet_item)
        if state in _SELECTABLE_STATES
        else None
    )

    # -- Facet header -------------------------------------------------------
    has_children = any(
        cu_item.get("implementation_state") in _SELECTABLE_STATES
        or cu_item.get("implementation_state") in _UNSELECTABLE_STATES
        for cu_item in contained_cu_items
    )
    use_menuconfig = (
        state in _SELECTABLE_STATES and facet_symbol is not None and has_children
    )
    use_plain_config = (
        state in _SELECTABLE_STATES and facet_symbol is not None and not has_children
    )
    if use_menuconfig:
        assert facet_symbol is not None
        if facet_symbol not in emitted_facet_symbols:
            emitted_facet_symbols.add(facet_symbol)
            lines.append("menuconfig " + facet_symbol)
            lines.append('\tbool "' + prompt + '"')
            _emit_facet_toggle_default(
                lines, facet_item, contained_cu_items, profile_symbols,
            )
            _emit_help(lines, facet_item)
            lines.append("")
        lines.append("if " + facet_symbol)
        lines.append("")
    elif use_plain_config:
        assert facet_symbol is not None
        if facet_symbol not in emitted_facet_symbols:
            emitted_facet_symbols.add(facet_symbol)
            lines.append("config " + facet_symbol)
            lines.append('\tbool "' + prompt + '"')
            _emit_facet_toggle_default(
                lines, facet_item, contained_cu_items, profile_symbols,
            )
            _emit_help(lines, facet_item)
            lines.append("")
    else:
        # Unimplemented facet: no toggle symbol, keep menu/comment/endmenu
        # so the item is traceable in the generated Kconfig (its item_id and
        # state are visible) even when the facet has zero contained CUs.
        lines.append('menu "Facet: ' + prompt + '"')
        lines.append("")
        _emit_unselectable(lines, facet_item)

    # -- Contained CUs -----------------------------------------------------
    # Emit selectable (claimed/implemented/deferred) CUs FIRST so the working
    # toggles lead the facet block, then emit unselectable (unimplemented) CUs
    # as trailing NOT IMPLEMENTED comments.  Inside a ``menuconfig``/``if``
    # block the block-level condition already gates the CUs, so the per-CU
    # ``depends on <facet>`` is suppressed (facet_symbol=None) to avoid
    # redundancy; the ``if`` enforces facet-off → CU-off.
    selectable_in_facet = [
        cu_item for cu_item in contained_cu_items
        if cu_item.get("implementation_state") in _SELECTABLE_STATES
    ]
    unselectable_in_facet = [
        cu_item for cu_item in contained_cu_items
        if cu_item.get("implementation_state") in _UNSELECTABLE_STATES
    ]
    cu_facet_gate: str | None = None  # if-block handles the dependency
    for cu_item in selectable_in_facet:
        _emit_selectable(
            lines, cu_item, profile_symbols,
            facet_symbol=cu_facet_gate,
        )
    for cu_item in unselectable_in_facet:
        _emit_unselectable(lines, cu_item)

    # -- Close block -------------------------------------------------------
    if use_menuconfig:
        lines.append("endif")
        lines.append("")
    elif use_plain_config:
        pass  # config with no children -- no block to close
    else:
        lines.append("endmenu")
        lines.append("")


def _emit_profile_sections(
    lines: list[str], manifest: dict, items_by_id: dict,
    profile_symbols: dict[str, str], profiles: dict,
) -> None:
    """Emit profile drilldown sections, each globally visible.

    Each named profile (nano through full) gets a ``menu "Profile: ..."``
    section containing the facets whose lowest cascade profile matches and
    any bare CUs (not owned by a facet) at that level.  All sections are
    always visible — profile selection seeds defaults only and never gates
    menu visibility (OPC-10000-7 §4.3).  Each facet/CU appears in exactly
    one canonical location (the lowest profile that reaches it).
    """
    facet_containment = manifest.get("facet_containment")
    if not isinstance(facet_containment, dict) or not facet_containment:
        return

    contained_cus = _contained_cu_ids(manifest)

    # Classify each facet by its lowest profile.
    facet_lowest: dict[str, str | None] = {}
    for facet_id in facet_containment:
        facet_item = items_by_id.get(facet_id)
        if not isinstance(facet_item, dict):
            continue
        cu_ids = facet_containment[facet_id]
        contained_cu_items: list[dict] = []
        if isinstance(cu_ids, list):
            for cu_id in cu_ids:
                cu_item = items_by_id.get(cu_id)
                if isinstance(cu_item, dict):
                    contained_cu_items.append(cu_item)
        facet_lowest[facet_id] = _facet_lowest_profile(facet_item, contained_cu_items)

    # Classify bare CUs (selectable CUs not inside any facet).
    bare_cu_lowest: dict[str, str | None] = {}
    for item in manifest.get("items", []):
        if not isinstance(item, dict):
            continue
        if item.get("kind") != "conformance_unit":
            continue
        if item.get("id") in contained_cus:
            continue
        if item.get("implementation_state") not in _SELECTABLE_STATES:
            continue
        bare_cu_lowest[item["id"]] = _lowest_default_profile(item)

    lines.append("# -- Profile drilldown sections (OPC-10000-7 §4.3) ---------------")
    lines.append("# All profile sections are always visible.  Profile selection seeds")
    lines.append("# defaults only; users can enable facets/CUs from any profile section.")
    lines.append("")

    emitted_facet_symbols: set[str] = set()

    for pk in _CASCADE_ORDER:
        profile = profiles.get(pk, {})
        opc_display_name = profile.get("opc_display_name")
        display = profile.get("display", pk)
        prompt = opc_display_name if opc_display_name else display

        section_facets = [
            fid for fid in facet_containment
            if facet_lowest.get(fid) == pk
        ]
        section_bare_cus = [
            cu_id for cu_id, lowest in bare_cu_lowest.items()
            if lowest == pk
        ]

        # Emit a menu for every named profile so users can always drill
        # down into any profile section, selected or not.
        lines.append('menu "Profile: ' + prompt + '"')
        lines.append("")

        for facet_id in section_facets:
            facet_item = items_by_id[facet_id]
            _emit_one_facet_menu(
                lines, facet_id, facet_item, facet_containment,
                items_by_id, profile_symbols, emitted_facet_symbols,
            )

        for cu_id in section_bare_cus:
            cu_item = items_by_id[cu_id]
            _emit_selectable(lines, cu_item, profile_symbols)

        if not section_facets and not section_bare_cus:
            lines.append('comment "No unique implemented Facets or CUs at this profile level"')
            lines.append("")

        lines.append("endmenu")
        lines.append("")

    # Facets with no profile default (unimplemented with all-false defaults)
    # still appear as visible empty menus for roadmap awareness.
    other_facets = [
        fid for fid in facet_containment
        if facet_lowest.get(fid) is None
        and isinstance(items_by_id.get(fid), dict)
    ]
    if other_facets:
        lines.append("# -- Unimplemented facets (visible but not selectable) --------------")
        lines.append("")
        for facet_id in other_facets:
            facet_item = items_by_id[facet_id]
            state = facet_item.get("implementation_state")
            if state not in _UNSELECTABLE_STATES:
                continue
            # Include context lines with the item id for traceability.
            _emit_unselectable(lines, facet_item)
            # Emit the facet's contained unimplemented Conformance Units as
            # visible comments too, so roadmap awareness of the facet's CUs is
            # preserved in menuconfig (OPC-10000-7 §4.2).  These CUs are
            # excluded from the flat emission (they are listed in
            # facet_containment), so without this they would be dropped.
            cu_ids = facet_containment.get(facet_id)
            if isinstance(cu_ids, list):
                for cu_id in cu_ids:
                    if not isinstance(cu_id, str):
                        continue
                    cu_item = items_by_id.get(cu_id)
                    if not isinstance(cu_item, dict):
                        continue
                    if cu_item.get("implementation_state") in _UNSELECTABLE_STATES:
                        _emit_unselectable(lines, cu_item)


def _emit_default(lines: list[str], item: dict, profile_symbols: dict[str, str]) -> None:
    default_unconditional = item.get("default_unconditional")

    if default_unconditional is True:
        lines.append("\tdefault y")
        return

    # Defaults always come from profile_defaults (the lowest reaching
    # profile + custom), never from depends_on: depends_on only expresses
    # "is this facet/CU selectable at all", not "is it on by default" --
    # a multi-facet CU must not default on just because one facet is on.

    # Use the lowest internal profile that reaches this item so a single
    # cascade condition covers every profile at that tier and above.
    lowest = _lowest_default_profile(item)
    profile_defaults = item.get("profile_defaults", {})
    custom_true = profile_defaults.get("custom") is True

    conditions: list[str] = []
    if lowest is not None:
        conditions.append(_default_condition_symbol(lowest, profile_symbols))
    if custom_true:
        conditions.append(_default_condition_symbol("custom", profile_symbols))

    if conditions:
        lines.append("\tdefault y if " + " || ".join(conditions))
    else:
        lines.append("\tdefault n")


def _emit_help(lines: list[str], item: dict) -> None:
    lines.append("\thelp")

    state = item.get("implementation_state", "unknown")
    opc_ref = item.get("opc_reference")
    source = _opc_source_string(opc_ref if isinstance(opc_ref, dict) else None)
    detail = _opc_detail_string(opc_ref if isinstance(opc_ref, dict) else None)

    lines.append("\t  Implementation state: " + state + ".")

    if source and detail:
        lines.append("\t  OPC source: " + source + " -- " + detail + ".")
    elif source:
        lines.append("\t  OPC source: " + source + ".")
    elif detail:
        lines.append("\t  OPC item: " + detail + ".")

    backing_tests = item.get("backing_tests") or []
    if backing_tests:
        lines.append("\t  Backing tests: " + ", ".join(backing_tests) + ".")

    source_meta = item.get("source_metadata")
    if isinstance(source_meta, dict):
        imported_from = source_meta.get("imported_from")
        if imported_from:
            lines.append("\t  Imported from: " + imported_from + ".")

    notes = item.get("notes")
    if notes:
        lines.append("\t  Notes: " + _sanitize_kconfig_text(notes))


# ---------------------------------------------------------------------------
# Capacity Kconfig generation
# ---------------------------------------------------------------------------

def _capacity_prompt(cap: dict) -> str:
    """Derive a human-readable Kconfig prompt from the capacity id."""
    cap_id = cap.get("id", "")
    return cap_id.replace("_", " ").capitalize()


def _emit_capacity_kconfig(lines: list[str], cap: dict, profile_symbols: dict[str, str]) -> None:
    """Emit a Kconfig ``int`` symbol for a capacity entry.

    Profile-conditioned defaults reference the internal cascade symbols
    (e.g. ``MUC_OPCUA_INTERN_PROFILE_STANDARD_2017_UA_SERVER``).  Defaults
    are emitted from the highest profile to the lowest so the cascade
    resolves correctly: the first matching ``default ... if`` wins, and
    higher internal symbols are a superset of lower ones.
    """
    sym = cap.get("kconfig_symbol")
    if not sym:
        return

    defaults = cap.get("defaults", {})
    baseline = cap.get("baseline_default", 0)

    lines.append("config " + sym)
    lines.append('\tint "' + _capacity_prompt(cap) + '"')

    # Emit defaults from highest to lowest cascade level so higher profiles
    # match before their lower cascade subsets.
    for pk in reversed(_CASCADE_ORDER):
        val = defaults.get(pk)
        if val is None or val == baseline:
            continue
        intern = _default_condition_symbol(pk, profile_symbols)
        lines.append("\tdefault " + str(val) + " if " + intern)

    lines.append("\tdefault " + str(baseline))

    _emit_capacity_help(lines, cap)
    lines.append("")


def _emit_capacity_help(lines: list[str], cap: dict) -> None:
    lines.append("\thelp")

    cap_id = cap.get("id", "")
    public_override = cap.get("public_override", "")
    internal_macro = cap.get("internal_macro", "")
    kind = cap.get("kind", "")

    lines.append("\t  Capacity: " + cap_id + ".")
    lines.append("\t  Public override: " + public_override + " (e.g. -D" + public_override + "=N).")
    lines.append("\t  Internal macro: " + internal_macro + ".")
    lines.append("\t  Kind: " + kind + ".")

    valid_range = cap.get("valid_range")
    if valid_range:
        lines.append("\t  Valid range: " + str(valid_range) + ".")

    if kind == "derived":
        derives_from = cap.get("derives_from")
        if derives_from:
            lines.append("\t  Derives from: " + derives_from + ".")

    opc_ref = cap.get("opc_reference")
    if isinstance(opc_ref, dict):
        note = opc_ref.get("note")
        if note:
            lines.append("\t  " + note)

    notes = cap.get("notes")
    if notes:
        lines.append("\t  Notes: " + _sanitize_kconfig_text(notes))


# ---------------------------------------------------------------------------
# capacities.h generation
# ---------------------------------------------------------------------------

_PROFILE_ORDER = ("nano", "micro", "embedded", "standard", "full")


def _capacity_comment(lines: list[str], cap: dict) -> None:
    """Emit a ``/* ---- ... ---- */`` comment header for *cap*."""
    cap_id = cap.get("id", "")
    public_override = cap.get("public_override", "")
    lines.append("/* ---- " + cap_id.replace("_", " ").capitalize() + " "
                 + "-" * max(1, 60 - len(cap_id)) + " */")
    lines.append("/* Public override: " + public_override + " */")


def _emit_capacity_profile_varying(
    lines: list[str], cap: dict, profile_symbols: dict[str, str],
) -> None:
    """Emit a profile-varying capacity with the three-stage cascade.

    The PROFILE stage ``#if``/``#elif`` guards reference internal cascade
    symbols (``MUC_OPCUA_INTERN_PROFILE_<TOKEN>``, OPC-10000-7 §4.3).
    Guards are emitted from the highest profile to the lowest so the C
    preprocessor first-match semantics align with the cascade: higher
    internal symbols are a superset of lower ones, so the highest matching
    guard wins.  Profiles whose value equals the baseline are skipped.  The
    public ``-DMU_MAX_*=N`` override (stage 3) still wins.
    """
    internal = cap["internal_macro"]
    public = cap["public_override"]
    baseline = cap["baseline_default"]
    defaults = cap.get("defaults", {})

    lines.append("")
    _capacity_comment(lines, cap)
    lines.append("#define " + internal + " " + str(baseline))

    # Emit from highest to lowest cascade level so first-match semantics
    # in the C preprocessor resolve correctly (INTERN_PROFILE_FULL implies
    # INTERN_PROFILE_STANDARD implies ... implies INTERN_PROFILE_NANO).
    first = True
    for pk in reversed(_PROFILE_ORDER):
        val = defaults.get(pk)
        if val is None or val == baseline:
            continue
        intern = _internal_profile_symbol(profile_symbols[pk])
        keyword = "#if" if first else "#elif"
        lines.append(keyword + " defined(" + intern + ")")
        lines.append("#undef " + internal)
        lines.append("#define " + internal + " " + str(val))
        first = False

    if not first:
        lines.append("#endif")

    lines.append("#ifdef " + public)
    lines.append("#undef " + internal)
    lines.append("#define " + internal + " " + public)
    lines.append("#endif")


def _emit_capacity_invariant(lines: list[str], cap: dict) -> None:
    """Emit a profile-invariant capacity (baseline + user override only)."""
    internal = cap["internal_macro"]
    public = cap["public_override"]
    baseline = cap["baseline_default"]

    lines.append("")
    _capacity_comment(lines, cap)
    lines.append("#define " + internal + " " + str(baseline))
    lines.append("#ifdef " + public)
    lines.append("#undef " + internal)
    lines.append("#define " + internal + " " + public)
    lines.append("#endif")


def _emit_capacity_derived(lines: list[str], cap: dict) -> None:
    """Emit a derived capacity (references its source capacity + override)."""
    internal = cap["internal_macro"]
    public = cap["public_override"]

    source_cap = cap.get("_source_cap")
    if isinstance(source_cap, dict):
        source_macro = source_cap.get("internal_macro", "")
    else:
        source_macro = ""

    lines.append("")
    _capacity_comment(lines, cap)
    if source_macro:
        lines.append("#define " + internal + " " + source_macro)
    else:
        baseline = cap.get("baseline_default", 0)
        lines.append("#define " + internal + " " + str(baseline))
    lines.append("#ifdef " + public)
    lines.append("#undef " + internal)
    lines.append("#define " + internal + " " + public)
    lines.append("#endif")


def generate_capacities_h(manifest: dict) -> str:
    """Return the full contents of the generated ``capacities.h``."""
    lines: list[str] = []

    lines.append("/* include/muc_opcua/capacities.h")
    lines.append(" *")
    lines.append(" * GENERATED from profiles/opcua-profile-manifest.yaml by")
    lines.append(" * scripts/profile_manifest/generate.py -- do not edit; regenerate with:")
    lines.append(" *   python3 scripts/profile_manifest/generate.py \\")
    lines.append(" *       --manifest profiles/opcua-profile-manifest.yaml --outputs capacities_h")
    lines.append(" *")
    lines.append(" * SINGLE SOURCE OF TRUTH for every tunable capacity dimension.")
    lines.append(" *")
    lines.append(" * Each capacity is resolved by a three-stage cascade, in order:")
    lines.append(" *")
    lines.append(" *   1. DEFAULT  -- an unconditional baseline (the minimal/nano values).")
    lines.append(" *   2. PROFILE  -- if an internal cascade symbol is active (Kconfig emits")
    lines.append(" *                  MUC_OPCUA_INTERN_PROFILE_<TOKEN>), redefine to that")
    lines.append(" *                  profile's value.  The cascade follows the selected")
    lines.append(" *                  base profile (OPC-10000-7 §4.3).")
    lines.append(" *   3. USER     -- if the public MU_MAX_* macro is defined (emitted by")
    lines.append(" *                  src/CMakeLists.txt from the Kconfig-resolved int symbol),")
    lines.append(" *                  redefine to it. The public knob wins over any profile.")
    lines.append(" *")
    lines.append(" * Precedence: default < profile < user (Kconfig-resolved).")
    lines.append(" *")
    lines.append(" * ALL library/application/test code compiles off the MU_INTERN_* macros below.")
    lines.append(" * The public MU_MAX_* macros are INPUT: they are emitted from resolved Kconfig")
    lines.append(" * capacity int symbols by src/CMakeLists.txt, so changes via -DMU_MAX_*=N,")
    lines.append(" * menuconfig, or MUC_OPCUA_KCONFIG_CONFIG all reach the compiler here.")
    lines.append(" */")
    lines.append("#ifndef MUC_OPCUA_CAPACITIES_H")
    lines.append("#define MUC_OPCUA_CAPACITIES_H")
    lines.append("")

    capacities = manifest.get("capacities", [])

    cap_by_id = {c.get("id"): c for c in capacities if isinstance(c, dict)}
    for cap in capacities:
        if isinstance(cap, dict) and cap.get("kind") == "derived":
            derives_from = cap.get("derives_from")
            cap["_source_cap"] = cap_by_id.get(derives_from)

    profile_varying = [c for c in capacities if isinstance(c, dict) and c.get("kind") == "profile_varying"]
    invariant = [c for c in capacities if isinstance(c, dict) and c.get("kind") == "invariant"]
    derived = [c for c in capacities if isinstance(c, dict) and c.get("kind") == "derived"]

    if profile_varying:
        lines.append("/* =====================================================================")
        lines.append(" * Profile-varying capacities (all three stages).")
        lines.append(" * ===================================================================== */")
        profile_symbols = {
            pk: _resolve_profile_symbol(manifest.get("profiles", {}), pk)
            for pk in _DEFAULT_PROFILES
        }
        for cap in profile_varying:
            _emit_capacity_profile_varying(lines, cap, profile_symbols)
        lines.append("")

    if invariant:
        lines.append("/* =====================================================================")
        lines.append(" * Profile-invariant capacities (stage 1 baseline + stage 3 user override).")
        lines.append(" * ===================================================================== */")
        for cap in invariant:
            _emit_capacity_invariant(lines, cap)
        lines.append("")

    if derived:
        lines.append("/* =====================================================================")
        lines.append(" * Derived capacities (depend on a resolved capacity above; must come last).")
        lines.append(" * ===================================================================== */")
        for cap in derived:
            _emit_capacity_derived(lines, cap)
        lines.append("")

    lines.append("#endif /* MUC_OPCUA_CAPACITIES_H */")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Defconfig generation
# ---------------------------------------------------------------------------

def generate_defconfig(manifest: dict, profile_key: str) -> str:
    """Return the defconfig contents for *profile_key*.

    For named profiles (nano/micro/embedded/standard/full) the defconfig
    sets only the effective profile choice symbol (e.g.
    ``MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER``).  The internal cascade
    symbols are derived automatically from the choice symbol; they need
    not appear in the defconfig (OPC-10000-7 §4.3).

    For ``custom`` only ``MUC_OPCUA_PROFILE_CUSTOM=y`` is emitted.  When
    ``opc_display_name`` is absent the legacy
    ``<prefix><PROFILE_KEY>=y`` fallback is preserved.
    """
    project = manifest.get("project", {})
    prefix = project.get("build_symbol_prefix", "MUC_OPCUA_")
    profiles = manifest.get("profiles", {})
    profile = profiles.get(profile_key, {})

    if profile_key == "custom":
        return "MUC_OPCUA_PROFILE_CUSTOM=y\n"

    opc_display_name = profile.get("opc_display_name")
    if isinstance(opc_display_name, str) and opc_display_name:
        profile_sym = compute_kconfig_symbol(opc_display_name, "profile")
    else:
        pid = profile.get("id", _profile_symbol(profile_key))
        profile_sym = prefix + pid

    return profile_sym + "=y\n"


# ---------------------------------------------------------------------------
# Claim map generation
# ---------------------------------------------------------------------------

_CLAIM_STATES = ("claimed", "implemented")
_STANDARD_PROFILES_NO_CUSTOM = ("nano", "micro", "embedded", "standard", "full")


def _claim_name(item: dict) -> str:
    """Derive a human-readable claim name from opc_reference or item id."""
    opc_ref = item.get("opc_reference")
    facet = cu_name = None
    if isinstance(opc_ref, dict):
        facet = opc_ref.get("facet")
        cu_name = opc_ref.get("cu_name")
    if facet and cu_name:
        return str(facet) + ": " + str(cu_name)
    if facet:
        return str(facet)
    if cu_name:
        return str(cu_name)
    return _item_prompt(item)


def _claim_section(item: dict) -> str:
    """Build the OPC § column from opc_reference."""
    opc_ref = item.get("opc_reference")
    if not isinstance(opc_ref, dict):
        return ""
    spec = opc_ref.get("spec")
    section = opc_ref.get("section")
    if spec and section:
        return str(spec) + " §" + str(section)
    if spec:
        return str(spec)
    return ""


def _claim_profiles(item: dict) -> str:
    """Derive the profiles cell from profile_defaults.

    Returns ``all`` if the item is enabled in every standard profile
    (nano through full), otherwise a comma-separated list.
    """
    pd = item.get("profile_defaults") or {}
    true_profiles = [
        p for p in _STANDARD_PROFILES_NO_CUSTOM if pd.get(p) is True
    ]
    if len(true_profiles) == len(_STANDARD_PROFILES_NO_CUSTOM):
        return "all"
    return ", ".join(true_profiles)


def generate_claim_map(manifest: dict) -> str:
    """Return the full contents of ``tests/conformance/claim_test_map.md``.

    Only manifest items with implementation_state ``claimed`` or
    ``implemented`` AND a non-empty ``backing_tests`` list produce rows.
    The table format is identical to what ``check_claim_map.py`` expects:
    four pipe-delimited columns (Claim, OPC §, Profiles, Backing test).
    """
    lines: list[str] = []

    lines.append("# Claim → Test Map")
    lines.append("")
    lines.append("GENERATED from profiles/opcua-profile-manifest.yaml by")
    lines.append("scripts/profile_manifest/generate.py — do not edit; regenerate with:")
    lines.append("  python3 scripts/profile_manifest/generate.py \\")
    lines.append("      --manifest profiles/opcua-profile-manifest.yaml --outputs claim_map")
    lines.append("")
    lines.append("Machine-checked mapping from each claimed OPC UA conformance unit /")
    lines.append("behavior to the test(s) that verify it, and the profiles those tests")
    lines.append("must run in. Enforced by `check_claim_map.py` (registered as the")
    lines.append("`test_claim_map` ctest in every profile build): for the profile a build")
    lines.append("targets, every row listing that profile MUST have a backing test that is")
    lines.append("registered in that build. A claimed unit whose backing test is absent")
    lines.append("from its profile fails the build (OPC-10000-7 §4.2/§4.3).")
    lines.append("")
    lines.append("Profiles column: comma-separated subset of")
    lines.append("`nano,micro,embedded,standard,full`, or `all`.")
    lines.append("Backing test column: comma-separated ctest names (as registered).")
    lines.append("")
    lines.append("| Claim / conformance unit | OPC UA § | Profiles | Backing test |")
    lines.append("|--------------------------|----------|----------|--------------|")

    items = manifest.get("items", [])
    for item in items:
        if not isinstance(item, dict):
            continue
        state = item.get("implementation_state")
        if state not in _CLAIM_STATES:
            continue
        backing = item.get("backing_tests") or []
        if not backing:
            continue

        name = _claim_name(item)
        section = _claim_section(item)
        profiles = _claim_profiles(item)
        backing_str = ", ".join(backing)

        lines.append(
            "| " + name + " | " + section + " | " + profiles + " | " + backing_str + " |"
        )

    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Roadmap generation
# ---------------------------------------------------------------------------

_ROADMAP_STATE_LABELS = {
    "claimed": "claimed",
    "implemented": "implemented",
    "deferred": "deferred",
    "unimplemented": "unimplemented",
}


def _roadmap_opc_ref(item: dict) -> str:
    """Compact OPC reference string for the roadmap table."""
    opc_ref = item.get("opc_reference")
    if not isinstance(opc_ref, dict):
        return ""
    spec = opc_ref.get("spec")
    section = opc_ref.get("section")
    facet = opc_ref.get("facet")
    parts: list[str] = []
    if spec:
        parts.append(str(spec))
    if section:
        parts.append("§" + str(section))
    if facet:
        parts.append(str(facet))
    return " ".join(parts) if parts else ""


def _roadmap_profiles(item: dict) -> str:
    """Compact profiles cell: e.g. ``all`` or ``micro,embedded,full``."""
    pd = item.get("profile_defaults") or {}
    true_profiles = [
        p for p in _STANDARD_PROFILES_NO_CUSTOM if pd.get(p) is True
    ]
    if len(true_profiles) == len(_STANDARD_PROFILES_NO_CUSTOM):
        return "all"
    return ", ".join(true_profiles) if true_profiles else "—"


def generate_roadmap(manifest: dict) -> str:
    """Return the full contents of ``docs/conformance/opc-profile-roadmap.md``.

    Includes every manifest item: implemented, claimed, deferred, and
    unimplemented.  Provides a single-page view of the OPC item matrix.
    """
    lines: list[str] = []

    lines.append("# OPC UA Profile Roadmap")
    lines.append("")
    lines.append("GENERATED from profiles/opcua-profile-manifest.yaml by")
    lines.append("scripts/profile_manifest/generate.py — do not edit; regenerate with:")
    lines.append("  python3 scripts/profile_manifest/generate.py \\")
    lines.append("      --manifest profiles/opcua-profile-manifest.yaml --outputs roadmap")
    lines.append("")
    lines.append("Full OPC UA item matrix tracking implementation state, Kconfig mapping,")
    lines.append("profile availability, and test coverage. Items span implemented, claimed,")
    lines.append("deferred, and unimplemented states. This document is a roadmap, not a")
    lines.append("compliance certificate — it records what the project implements and what")
    lines.append("remains future work.")
    lines.append("")

    items = manifest.get("items", [])

    # -- Summary counts ---------------------------------------------------
    state_counts: dict[str, int] = {}
    for item in items:
        if not isinstance(item, dict):
            continue
        state = item.get("implementation_state", "unknown")
        state_counts[state] = state_counts.get(state, 0) + 1

    lines.append("## Summary")
    lines.append("")
    lines.append("| State | Count |")
    lines.append("|-------|-------|")
    for state in ("claimed", "implemented", "documented", "deferred", "unimplemented"):
        count = state_counts.get(state, 0)
        if count:
            lines.append("| " + state + " | " + str(count) + " |")
    lines.append("")

    # -- Item matrix ------------------------------------------------------
    lines.append("## Item matrix")
    lines.append("")
    lines.append("| Item | Kind | State | OPC reference | Kconfig | Profiles | Backing tests |")
    lines.append("|------|------|-------|---------------|---------|----------|---------------|")

    for item in items:
        if not isinstance(item, dict):
            continue

        item_id = item.get("id", "")
        kind = item.get("kind", "")
        state = item.get("implementation_state", "unknown")
        state_label = _ROADMAP_STATE_LABELS.get(state, state)
        opc_ref = _roadmap_opc_ref(item)
        kconfig = item.get("kconfig_symbol") or "—"
        profiles = _roadmap_profiles(item)
        backing = item.get("backing_tests") or []
        backing_str = ", ".join(backing) if backing else "—"

        lines.append(
            "| " + item_id + " | " + kind + " | " + state_label + " | "
            + opc_ref + " | " + str(kconfig) + " | " + profiles + " | "
            + backing_str + " |"
        )

    lines.append("")

    # -- Capacities summary ----------------------------------------------
    capacities = manifest.get("capacities", [])
    if capacities:
        lines.append("## Capacities")
        lines.append("")
        lines.append("| Capacity | Kind | Override | nano | micro | embedded | standard | full |")
        lines.append("|----------|------|----------|------|-------|----------|----------|------|")

        for cap in capacities:
            if not isinstance(cap, dict):
                continue
            cap_id = cap.get("id", "")
            kind = cap.get("kind", "")
            override = cap.get("public_override", "")
            defaults = cap.get("defaults") or {}
            vals = []
            for pk in _STANDARD_PROFILES_NO_CUSTOM:
                val = defaults.get(pk, cap.get("baseline_default", ""))
                vals.append(str(val))
            lines.append(
                "| " + cap_id + " | " + kind + " | " + override + " | "
                + " | ".join(vals) + " |"
            )

        lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Build-and-gating docs generation
# ---------------------------------------------------------------------------

_BUILD_DOCS_BEGIN = "<!-- BEGIN GENERATED MANIFEST TABLES -->"
_BUILD_DOCS_END = "<!-- END GENERATED MANIFEST TABLES -->"


def generate_build_docs_section(manifest: dict) -> str:
    """Return the generated tables block for ``docs/build-and-gating.md``.

    The returned string is the content *between* the begin/end markers
    (markers themselves are added by ``update_build_docs``).
    """
    lines: list[str] = []

    lines.append("## Manifest-generated reference tables")
    lines.append("")
    lines.append("The tables below are generated from")
    lines.append("`profiles/opcua-profile-manifest.yaml` by")
    lines.append("`scripts/profile_manifest/generate.py --outputs build_docs`.")
    lines.append("Do not edit between the BEGIN/END markers; run the generator")
    lines.append("to refresh.")
    lines.append("")

    # -- Feature table ----------------------------------------------------
    items = manifest.get("items", [])
    selectable = [
        i for i in items
        if isinstance(i, dict) and i.get("implementation_state") in _SELECTABLE_STATES
    ]

    lines.append("### Feature symbols")
    lines.append("")
    lines.append("| Kconfig | Item | State | nano | micro | embedded | standard | full | Depends on |")
    lines.append("|---------|------|-------|------|-------|----------|----------|------|------------|")

    for item in selectable:
        kconfig = item.get("kconfig_symbol") or "—"
        item_id = item.get("id", "")
        state = item.get("implementation_state", "")
        pd = item.get("profile_defaults") or {}
        cells = []
        for pk in _STANDARD_PROFILES_NO_CUSTOM:
            cells.append("✅" if pd.get(pk) is True else "")
        depends_on = item.get("depends_on") or []
        dep_str = ", ".join(depends_on) if depends_on else ""
        lines.append(
            "| " + kconfig + " | " + item_id + " | " + state + " | "
            + " | ".join(cells) + " | " + dep_str + " |"
        )

    lines.append("")

    # -- Capacity table ---------------------------------------------------
    capacities = manifest.get("capacities", [])
    if capacities:
        lines.append("### Capacity symbols")
        lines.append("")
        lines.append("| Kconfig | Capacity | nano | micro | embedded | standard | full | Override |")
        lines.append("|---------|----------|------|-------|----------|----------|------|----------|")

        for cap in capacities:
            if not isinstance(cap, dict):
                continue
            kconfig = cap.get("kconfig_symbol", "")
            cap_id = cap.get("id", "")
            defaults = cap.get("defaults") or {}
            vals = []
            for pk in _STANDARD_PROFILES_NO_CUSTOM:
                vals.append(str(defaults.get(pk, cap.get("baseline_default", ""))))
            override = cap.get("public_override", "")
            lines.append(
                "| " + kconfig + " | " + cap_id + " | "
                + " | ".join(vals) + " | " + override + " |"
            )

        lines.append("")

    # -- Unavailable items ------------------------------------------------
    unavailable = [
        i for i in items
        if isinstance(i, dict) and i.get("implementation_state") in _UNSELECTABLE_STATES
    ]

    lines.append("### Unavailable OPC items in Kconfig")
    lines.append("")
    lines.append(
        "The following OPC items are tracked in the manifest but are NOT implemented. "
        "They appear in the generated `Kconfig` as visible `comment` directives so "
        "they show up in `menuconfig` for roadmap awareness, but they carry no config "
        "symbol and cannot be selected, toggled, or set in `.config`. This makes the "
        "full OPC feature surface visible to developers without implying any "
        "implementation claim."
    )
    lines.append("")

    if unavailable:
        lines.append("| Item | OPC reference | State | Notes |")
        lines.append("|------|---------------|-------|-------|")
        for item in unavailable:
            item_id = item.get("id", "")
            opc_ref = _roadmap_opc_ref(item)
            state = item.get("implementation_state", "")
            notes = _sanitize_kconfig_text(item.get("notes"))
            lines.append(
                "| " + item_id + " | " + opc_ref + " | " + state + " | " + notes + " |"
            )
        lines.append("")
    else:
        lines.append("(none)")
        lines.append("")

    return "\n".join(lines)


def update_build_docs(manifest: dict, path: str) -> None:
    """Update the generated section within ``docs/build-and-gating.md``.

    Reads the existing file, finds the BEGIN/END markers, and replaces the
    content between them. If the markers are not present, inserts the
    generated block before the ``## Verifying gating behavior`` heading (or
    appends at end of file if that heading is absent).
    """
    section = generate_build_docs_section(manifest)
    blocked = _BUILD_DOCS_BEGIN + "\n" + section + _BUILD_DOCS_END + "\n"

    try:
        with open(path, "r", encoding="utf-8") as fh:
            content = fh.read()
    except FileNotFoundError:
        content = ""

    begin_idx = content.find(_BUILD_DOCS_BEGIN)
    end_idx = content.find(_BUILD_DOCS_END)

    if begin_idx != -1 and end_idx != -1 and end_idx > begin_idx:
        before = content[:begin_idx]
        after = content[end_idx + len(_BUILD_DOCS_END):]
        new_content = before + blocked + after
    else:
        anchor = "\n## Verifying gating behavior\n"
        insertion = "\n" + blocked + "\n"
        anchor_idx = content.find(anchor)
        if anchor_idx != -1:
            new_content = content[:anchor_idx] + insertion + content[anchor_idx:]
        elif content.endswith("\n"):
            new_content = content + "\n" + blocked
        else:
            new_content = content + "\n\n" + blocked

    with open(path, "w", encoding="utf-8") as fh:
        fh.write(new_content)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate Kconfig and defconfigs from the OPC UA profile manifest.",
    )
    parser.add_argument(
        "--manifest",
        required=True,
        help="Path to the manifest .yaml/.json file.",
    )
    parser.add_argument(
        "--outputs",
        default="kconfig,defconfigs",
        help="Comma-separated list of outputs to write "
        "(kconfig, defconfigs, capacities_h, claim_map, roadmap, build_docs, completion).",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)

    try:
        manifest = load_manifest(args.manifest)
    except (FileNotFoundError, ValueError) as exc:
        print("generate: FAIL")
        print("  load error: " + str(exc))
        return 1

    # Live join: the composition graph is the authoritative source for
    # depends_on/profile_defaults on every graph-mapped conformance_unit.
    # In-memory only -- the manifest is never written back to disk.
    graph_path = os.path.join(_REPO_ROOT, "profiles", "opcua-profile-graph.json")
    graph_deps.resolve_into(manifest, graph_deps.load_graph(graph_path))

    errors = validate_manifest(manifest)
    if errors:
        print("generate: FAIL (manifest invalid)")
        for err in errors:
            print("  - " + err)
        return 1

    outputs = [o.strip() for o in args.outputs.split(",") if o.strip()]

    repo_root = _REPO_ROOT

    for output in outputs:
        if output == "kconfig":
            kconfig_path = os.path.join(repo_root, "Kconfig")
            content = generate_kconfig(manifest)
            with open(kconfig_path, "w", encoding="utf-8") as fh:
                fh.write(content)
            print("wrote " + kconfig_path)
        elif output == "defconfigs":
            for pk in _DEFAULT_PROFILES:
                defconfig_path = os.path.join(repo_root, "configs", pk + ".defconfig")
                content = generate_defconfig(manifest, pk)
                with open(defconfig_path, "w", encoding="utf-8") as fh:
                    fh.write(content)
                print("wrote " + defconfig_path)
        elif output == "capacities_h":
            cap_path = os.path.join(repo_root, "include", "muc_opcua", "capacities.h")
            content = generate_capacities_h(manifest)
            with open(cap_path, "w", encoding="utf-8") as fh:
                fh.write(content)
            print("wrote " + cap_path)
        elif output == "claim_map":
            claim_path = os.path.join(
                repo_root, "tests", "conformance", "claim_test_map.md"
            )
            content = generate_claim_map(manifest)
            with open(claim_path, "w", encoding="utf-8") as fh:
                fh.write(content)
            print("wrote " + claim_path)
        elif output == "roadmap":
            docs_dir = os.path.join(repo_root, "docs", "conformance")
            os.makedirs(docs_dir, exist_ok=True)
            roadmap_path = os.path.join(docs_dir, "opc-profile-roadmap.md")
            content = generate_roadmap(manifest)
            with open(roadmap_path, "w", encoding="utf-8") as fh:
                fh.write(content)
            print("wrote " + roadmap_path)
        elif output == "build_docs":
            build_docs_path = os.path.join(repo_root, "docs", "build-and-gating.md")
            update_build_docs(manifest, build_docs_path)
            print("updated " + build_docs_path)
        elif output == "completion":
            import json as _json
            docs_dir = os.path.join(repo_root, "docs", "conformance")
            os.makedirs(docs_dir, exist_ok=True)
            snap_path = os.path.join(repo_root, "profiles", "opcua-profile-normalized-snapshot.json")
            with open(snap_path, encoding="utf-8") as fh:
                snapshot = _json.load(fh)
            cat_path = os.path.join(repo_root, "profiles", "opcua-server-conformance.json")
            catalog = None
            if os.path.exists(cat_path):
                with open(cat_path, encoding="utf-8") as fh:
                    catalog = _json.load(fh)
            content = _completion.render_report(manifest, snapshot, catalog)
            completion_path = os.path.join(docs_dir, "completion.md")
            with open(completion_path, "w", encoding="utf-8") as fh:
                fh.write(content)
            print("wrote " + completion_path)
        else:
            print("generate: FAIL (unknown output '" + output + "')")
            return 1

    print("generate: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
