# OPC-Named Kconfig Redesign — Implementation Plan

**Goal**: Replace project-centric Kconfig symbol names with OPC UA standard names for Profiles, Facets, and Conformance Units, restructure the Kconfig menu tree into an OPC-first drilldown, and remove legacy feature aliases.

**Architecture**: The manifest (`profiles/opcua-profile-manifest.yaml`) is extended with Facet containment (which CUs belong to which Facet) and OPC UA standard display names. The generator (`scripts/profile_manifest/generate.py`) produces a new Kconfig tree: top-level profile choice, Facet drilldown menus with group toggles, CU entries inside Facets, a separate Capacities menu, and a Project options menu. Validation ensures no redundant kind suffixes in generated symbols. CMake and test scripts are updated to use the new symbol names.

**Tech Stack**: Python 3 (stdlib + vendored kconfiglib), CMake 3.20+, bash, Kconfig, C11.

## Global Constraints

- All OPC claims must cite exact OPC-10000 part/section numbers (Constitution I).
- The C core must remain freestanding C11-compatible (Constitution II).
- No new OPC conformance claims; rename/restructure only (Constitution V).
- Generated Kconfig must be reproducible; drift check must pass (Constitution VI).
- No silent size regression; existing profiles must produce identical or smaller binaries (Constitution VII).
- No legacy aliases for old `MUC_OPCUA_*` feature names in Kconfig (Spec Non-goals).
- Capacity override support (`-DMU_MAX_*=N`) must remain working (Spec FR-010).
- Full test suite (132 tests) must pass after regeneration (Spec Success Criteria).

---

## Technical Context

### Domain
OPC UA Server profile/facet/conformance-unit configuration via Linux kernel-style Kconfig. The project uses a manifest-driven code generation approach: a single YAML/JSON manifest declares all OPC items (profiles, facets, CUs, capacities), and a Python generator produces Kconfig, defconfigs, capacity headers, claim maps, roadmaps, and build documentation.

### Current State
- `Kconfig` is generated flat: all selectable items are listed under `menu "Server"` with project-centric symbol names like `SERVICE_READ`, `SECURITY`, `SUBSCRIPTIONS`.
- Profile names in Kconfig are project-centric: `PROFILE_NANO`, `PROFILE_STANDARD`, etc.
- Facets and CUs are not grouped; the menu shows a flat list of all items.
- Unimplemented items appear as Kconfig `comment` directives (visible, unselectable).
- The generator emits symbols using `item.get("kconfig_symbol")` directly from the manifest.
- The manifest currently stores short project-centric `kconfig_symbol` values like `"BASE_NODES"`, `"SERVICE_READ"`.

### Target State (per approved design doc)
- Profile selector uses OPC standard display names and `MUC_OPCUA_PROFILE_<NAME>` symbols.
- Facets appear as drilldown menus labeled `Facet: <OPC UA standard name>` with group toggles.
- CUs appear inside Facet menus labeled `CU: <OPC UA standard name>`.
- Symbols follow `MUC_OPCUA_{PROFILE|FACET|CU}_<NAME>` without redundant kind suffixes.
- Profile→custom transition when user deviates from named profile's Facet/CU set.
- Capacities remain in separate menu (existing model is correct).
- Project options isolated in `menu "Project options"`.
- No legacy aliases for old project-centric feature names.

### Key Technical Decisions

1. **Manifest restructuring**: Add `facet_containment` field mapping Facet IDs to lists of CU IDs. Add `opc_display_name` fields for Profiles, Facets, and CUs. Change `kconfig_symbol` values to OPC-name-derived symbols.

2. **Symbol naming algorithm**: `OPC standard name → remove trailing kind word if redundant → uppercase → non-alnum to `_` → trim → prefix with `MUC_OPCUA_{PROFILE|FACET|CU}_`.

3. **Facet group toggle**: Use a `MUC_OPCUA_FACET_<NAME>` bool that defaults to `y` when any of its CUs should be enabled per the profile. CU symbols use `depends on MUC_OPCUA_FACET_<NAME>` so setting the Facet to `n` forces contained CUs off. CUs do not `select` the Facet, preserving the explicit "Facet OFF = all contained CUs OFF" model.

4. **Profile→custom**: Generate `MUC_OPCUA_FACETS_MATCH_<PROFILE>` helper symbols that compare resolved Facet/CU state against each named profile's expected set. Named profile choice defaults reference these helpers; `custom` is the unconditional fallback when no named profile matches.

5. **One symbol, one location**: Kconfig allows each symbol at one menu location. The manifest must assign each CU to exactly one canonical Facet for Kconfig display purposes. Cross-references go in help text.

### Resolved Planning Decisions

1. **Profile→custom mechanism**: Use generated fidelity-check symbols `MUC_OPCUA_FACETS_MATCH_<PROFILE>` that compare resolved Facet/CU state against each named profile. Named profile choice defaults reference these helpers; `custom` is the unconditional fallback.

2. **Facet group toggle ↔ CU interaction**: CU symbols are gated with `depends on MUC_OPCUA_FACET_<NAME>`. A disabled Facet forces contained CUs off and prevents CU edits until the Facet is re-enabled. CUs do not `select` the Facet.

---

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Spec Fidelity | PASS | No new OPC claims. Existing `opc_reference` citations preserved. Only names/menus change. |
| II. Embedded-First C Core | PASS | Pure manifest/Kconfig/CMake change. No C code changes. No new allocations. |
| III. Minimal OPC UA Surface | PASS | No new services/features. Same features, new names. |
| IV. Protocol Correctness Gates | PASS | All 132 existing tests must pass. No parsing/serialization changes. |
| V. Security and Conformance Honesty | PASS | Conformance claims unchanged. Only display names and symbols change. |
| VI. Fixed Toolchain | PASS | Generated Kconfig must be byte-identical after regeneration. `kconfiglib` vendored. CMake unchanged. |
| VII. Size Discipline | PASS | No C code changes → no binary size change. Regeneration only affects text files. |

**Gate result**: PASS. No violations.

---

## Phase 0: Research

See [research.md](./research.md) for resolved decisions.

### Key Decisions

1. **Profile→custom mechanism**: Use a Kconfig `choice` with named profiles plus `custom`. Each named profile sets `default y if FACETS_MATCH_EXPECTED`. A companion generated symbol `MUC_OPCUA_FACETS_MATCH_<profile>` evaluates whether all profile-controlled Facets/CUs match their profile defaults. If any doesn't match, the `custom` choice becomes selected instead.

2. **Facet-CU interaction**: CU symbols `depends on MUC_OPCUA_FACET_<NAME>`. The Facet toggle is a normal bool. CUs default to profile defaults. Changing a CU does not automatically change the Facet toggle (the toggle is a group setting, not a dependency constraint). Kconfig's `depends on` means: if Facet is OFF, CUs are forced OFF and not user-changeable.

3. **`STANDARD_PROFILE` marker**: Renamed to `MUC_OPCUA_MARKER_STANDARD_PROFILE` to clarify it's a profile marker, not a profile choice. Kept as an internal helper symbol that CMake resolves.

4. **Manifest field additions**:
   - `facet_containment`: `{ "facet_id": ["cu_id_1", "cu_id_2", ...] }`
   - `opc_display_name`: OPC UA standard name string for each item
   - `opc_kind_suffix`: null → symbol name has no trailing kind word. Non-null → trailing word is preserved (e.g., "File Server Facet" → `MUC_OPCUA_FACET_FILE_SERVER` — "Facet" stripped because kind=FACET).

5. **Naming algorithm precision**: The rule "remove a trailing kind word when it duplicates the symbol kind" means: if the symbol is a FACET and the OPC name ends with "Facet", strip that word. If the OPC name is "Core 2017 Server Facet", the result is `CORE_2017_SERVER`. If the OPC name is "Attribute Read" (a CU, no trailing "Conformance Unit"), the result is `ATTRIBUTE_READ`.

---

## Phase 1: Design & Contracts

### Data Model

See [data-model.md](./data-model.md) for the full manifest schema.

#### Manifest extension: Facet containment

```yaml
facet_containment:
  core_2017_server:
    - service_read     # id of CU item, not kconfig_symbol
    - service_browse
    - service_discovery
    - service_register_nodes
    - base_nodes
```

#### Manifest extension: OPC display names

Each item gains an `opc_display_name` field. The generator uses this for Kconfig prompts:

```yaml
items:
  - id: "service_read"
    kind: "conformance_unit"
    opc_display_name: "Attribute Read"
    kconfig_symbol: null  # computed by generator from opc_display_name
```

#### Symbol computation

The generator computes `kconfig_symbol` from `opc_display_name` and `kind`:

```
def compute_kconfig_symbol(display_name: str, kind: str) -> str:
    prefix = {"profile": "MUC_OPCUA_PROFILE_",
              "facet": "MUC_OPCUA_FACET_",
              "conformance_unit": "MUC_OPCUA_CU_",
              "optimization": "MUC_OPCUA_OPT_"}
    name = display_name.upper()
    # Strip trailing kind word if redundant
    kind_suffixes = { "profile": "PROFILE", "facet": "FACET" }
    # ... apply naming rules from spec
    return prefix[kind] + name
```

The manifest's `kconfig_symbol` field becomes optional/generated. Items that are unimplemented still have `kconfig_symbol: null` and generate `comment` directives.

#### Profile→custom detection

The generator produces a secondary Kconfig include file with helper symbols:

```kconfig
# Generated: profile_fidelity_check.generated.kconfig
config MUC_OPCUA_FACETS_MATCH_EMBEDDED_2017_UA_SERVER
    bool
    default y if MUC_OPCUA_FACET_CORE_2017_SERVER=y && MUC_OPCUA_FACET_BASE_INFO_TYPE_SYSTEM=y && ...
    # ... one condition per profile-controlled Facet/CU
```

These symbols feed the profile choice's `default` values. When all conditions match, the named profile is the default; otherwise `custom` becomes the default.

### Contracts

See [contracts/](./contracts/) for detailed interface contracts.

#### Kconfig generation contract

The generator MUST produce:

1. **Profile choice**: `choice` block with `config MUC_OPCUA_PROFILE_<NAME>` entries.
2. **Profile fidelity helpers**: `config MUC_OPCUA_FACETS_MATCH_<NAME>` bool symbols.
3. **Facet menus**: `menu "Facet: <OPC display name>"` containing a `config MUC_OPCUA_FACET_<NAME>` group toggle and nested `config MUC_OPCUA_CU_<NAME>` entries.
4. **Capacities menu**: Unchanged from current generation.
5. **Project options menu**: `menu "Project options"` for optimization/non-OPC items.
6. **Unimplemented comments**: `comment "<name> (NOT IMPLEMENTED) [<source>]"` for unavailable items.

#### CMake integration contract

The `CMakeLists.txt` `MUC_OPCUA_KCONFIG_FEATURES` list must be updated to list all new OPC-named symbols. The `gen_config.py` script must resolve these symbols from Kconfig and emit them into `muc_opcua_config.cmake`.

#### Validation contract

`validate.py --all` must additionally check:
- No generated symbol has redundant kind suffix.
- Facet menus are labeled correctly.
- CU entries appear inside their declared Facet.
- Unimplemented items are `comment` directives (no symbol).

### Quickstart

See [quickstart.md](./quickstart.md) for the developer verification path.

```sh
# 1. Restructure manifest with facet containment and OPC names
python3 scripts/profile_manifest/validate.py --manifest-only

# 2. Regenerate all artifacts
python3 scripts/profile_manifest/generate.py \
    --manifest profiles/opcua-profile-manifest.yaml \
    --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs

# 3. Validate generated output
python3 scripts/profile_manifest/validate.py --all

# 4. Run profile gating tests
bash scripts/test_profile_gating.sh

# 5. Full test suite
cmake -S . -B build/standard-check -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/standard-check && ctest --test-dir build/standard-check --output-on-failure

cmake -S . -B build/full-check -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/full-check && ctest --test-dir build/full-check --output-on-failure
```
