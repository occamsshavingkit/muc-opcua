# Research: OPC-Named Kconfig Redesign

## Decision 1: Profile Fidelity Reporting

**Original (superseded)**: Use a companion generated Kconfig file with fidelity-check symbols to auto-switch the profile choice to `custom` when Facet/CU overrides diverge from the named profile.

**Revised 2026-08-01**: Keep profile choice seeds stable and emit each advertised-profile marker directly from the conjunction of currently selectable mandatory CUs for its manifest `required_profile`. `MUC_OPCUA_FACETS_MATCH_*` is obsolete.

**Rationale**: Kconfig `choice` symbols represent the user's explicitly selected seed profile, while the resolved Facet/CU state may intentionally differ. Automatically switching the choice to `custom` is misleading. Instead, `generate.py` derives the required selectable CU symbols for each advertisement marker and emits `default y if CU_A && ...`; when no complete selectable closure exists, it emits `default n`. The current Standard marker therefore remains off. `custom` is always explicit.

**Alternatives considered**:
- ~~Post-configuration script: Too fragile. Requires running an extra step after every Kconfig resolution. Rejected.~~
- ~~`select`-based approach: Each CU override could `select PROFILE_CUSTOM`. This works but pollutes each symbol with a `select` and requires non-bool symbols for profile choice. Rejected for complexity.~~
- Move profile selection entirely to CMake (not Kconfig): Contradicts the spec's requirement that the Kconfig surface starts with a profile selector.

## Decision 2: Facet Group Toggle ↔ CU Interaction

**Decision**: CU symbols `depends on MUC_OPCUA_FACET_<NAME>` for a hard gate. Facet is ON by default per profile. When OFF, CUs are forced OFF and not user-changeable. CUs default to profile-set values when Facet is ON.

**Rationale**: This matches the spec requirement: the Facet toggle gates all implemented CUs as a group, while individual CUs remain separately toggleable when the Facet is enabled. Kconfig's `depends on` semantics are a natural fit because Facet OFF forces CUs off without making Facet ON force every CU on.

**Alternatives considered**:
- `select` from CU→Facet: Would auto-enable Facet when any CU is enabled. This prevents a clean "Facet OFF = all CUs OFF" model. Rejected.
- No Facet toggle (just menus): Loses the group-control convenience. Rejected.

## Decision 3: Symbol Naming Algorithm Precision

**Decision**: The `opc_kind_suffix` field in each manifest item controls whether a trailing word is stripped. For Profiles: strip "Profile". For Facets: strip "Facet". For CUs: no stripping (CU names don't end with "Conformance Unit"). The generator applies this rule deterministically.

**Rationale**: Some OPC names end with "Facet" (e.g., "Core 2017 Server Facet"). Since the symbol prefix already contains `FACET`, repeating it is redundant. The spec explicitly forbids `MUC_OPCUA_FACET_CORE_2017_SERVER_FACET`.

**Algorithm**:
```python
def compute_kconfig_name(opc_display_name: str, kind: str) -> str:
    name = opc_display_name
    # Strip trailing kind word if redundant
    kind_words = {"profile": "profile", "facet": "facet"}
    if kind in kind_words:
        suffix = kind_words[kind]
        if name.lower().endswith(" " + suffix):
            name = name[:-(len(suffix) + 1)]
    return name.upper().replace(" ", "_").replace("-", "_")
```

**Examples**:
| OPC Name | Kind | Result |
|----------|------|--------|
| Embedded 2025 UA Server Profile | profile | EMBEDDED_2025_UA_SERVER |
| Standard 2025 UA Server Profile | profile | STANDARD_2025_UA_SERVER |
| Core 2017 Server Facet | facet | CORE_2017_SERVER |
| Subscription Server Facet | facet | SUBSCRIPTION_SERVER |
| Attribute Read | conformance_unit | ATTRIBUTE_READ |
| Monitor Items 500 | conformance_unit | MONITOR_ITEMS_500 |

## Decision 4: Manifest Restructuring Approach

**Decision**: Add `facet_containment` top-level key, `opc_display_name` to each item, and make `kconfig_symbol` optional (computed by generator when absent). Keep backward-compatible by generating `kconfig_symbol` during `load_manifest()` or at generation time.

**Rationale**: The manifest is the single source of truth. Facet containment is a new concept not present in the current flat structure. Adding it as a top-level key keeps it discoverable and avoids deep nesting.

**Alternatives considered**:
- Nest CUs inside Facets in the manifest: Would make the items list hierarchical. This is cleaner but requires a major schema rewrite. The flat list + facet_containment map approach is simpler to implement incrementally. Rejected as premature restructuring.

## Decision 5: Legacy Symbol Removal Strategy

**Decision**: Remove all old project-centric `kconfig_symbol` values from the manifest. The generator computes new OPC-name-derived symbols. `CMakeLists.txt` discovers every generated selectable canonical boolean directly from Kconfig instead of duplicating a partial list. No backward-compatibility aliases.

**Rationale**: The spec explicitly states "Do not preserve old project-internal MUC_OPCUA_* feature aliases." Old symbols like `MUC_OPCUA_SECURITY`, `MUC_OPCUA_SUBSCRIPTIONS` are replaced by `MUC_OPCUA_FACET_SECURITY`, `MUC_OPCUA_FACET_SUBSCRIPTIONS` (computed from OPC names).

**Impact**: The `test_profile_gating.sh` script and `CMakeLists.txt` symbol lists must be updated. C source files using `#ifdef MUC_OPCUA_<OLD_SYM>` must be updated to the new names. This is the largest single change in the redesign.

## Decision 6: STANDARD_PROFILE Marker Renaming

**Decision**: Rename `STANDARD_PROFILE` to `MUC_OPCUA_MARKER_STANDARD_PROFILE`. This is an internal CMake/C helper symbol, not a user-selectable profile.

**Rationale**: The old name confuses it with profile selection (`PROFILE_STANDARD`). The new name clarifies it's a marker that CMake resolves based on which profile is selected.
