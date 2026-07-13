# Graph-Derived Kconfig Redo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate a Kconfig tree whose profile, facet, CU, and capacity outputs resolve to the build surface the user selected, with profile selection seeding defaults but never hiding unrelated profile/facet/CU subtrees.

**Architecture:** Treat the manifest as a local graph proxy for OPC profile data: profiles include lower profiles, profiles contain facets and bare CUs, and facets contain CUs. Generate invisible `MUC_OPCUA_INTERN_PROFILE_*` cascade symbols from selected profiles, then default every facet/CU from the lowest internal profile that reaches it. Keep profile/facet/CU prompts globally editable so users can add or subtract features outside the selected profile.

**Tech Stack:** Python 3 stdlib, vendored `kconfiglib`, CMake, Kconfig, C11.

## Global Constraints

- Do not distribute downloaded OPC Foundation profile data or the local scrape playbook.
- Do not change the canonical manifest to 2022/2025 API data in this task.
- Profile selection is a default preset only; it must not gate visibility of other profile/facet/CU menus.
- The generated Kconfig must produce `autoconf.h`/`.config`/`muc_opcua_config.cmake` with selected CUs and capacities.
- Existing C code compatibility symbols must keep building until a later source migration removes them.
- No new OPC conformance claims; this is a Kconfig generation semantics change.

---

## File Structure

- Modify `scripts/profile_manifest/generate.py`: replace preset-seed/default logic with internal profile cascade defaults and profile/facet/CU graph menus.
- Modify `scripts/profile_manifest/validate.py`: update Kconfig structure/profile checks to expect internal cascade symbols instead of preset symbols and facets-match-driven profile choice.
- Modify `scripts/test_profile_gating.sh`: update scenarios from preset/facets-match behavior to selected-profile-seeds-defaults behavior and verify cross-profile activation.
- Regenerate `Kconfig`, `configs/*.defconfig`, `include/muc_opcua/capacities.h`, `tests/conformance/claim_test_map.md`, `docs/conformance/opc-profile-roadmap.md`, and `docs/build-and-gating.md` using the existing generator outputs.
- Modify `CMakeLists.txt` only as needed to replace preset feature names in `MUC_OPCUA_KCONFIG_FEATURES` with internal profile symbols.

## Task 1: Replace Profile Presets With Internal Cascade

**Files:**
- Modify: `scripts/profile_manifest/generate.py`
- Modify: `CMakeLists.txt`
- Regenerate: `Kconfig`, `configs/*.defconfig`

**Interfaces:**
- Consumes: `compute_kconfig_symbol(opc_display_name: str, kind: str) -> str`
- Produces: `_internal_profile_symbol(profile_symbol: str) -> str`
- Produces: `_profile_default_symbol(profile_key: str, profile_symbols: dict[str, str]) -> str`

- [ ] **Step 1: Add helper tests mentally before coding**

Expected helper behavior:

```text
MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER -> MUC_OPCUA_INTERN_PROFILE_STANDARD_2017_UA_SERVER
MUC_OPCUA_PROFILE_CUSTOM -> MUC_OPCUA_PROFILE_CUSTOM
```

- [ ] **Step 2: Implement internal profile helpers**

Add a helper near `_preset_symbol` and replace preset references:

```python
def _internal_profile_symbol(profile_symbol: str) -> str:
    prefix = "MUC_OPCUA_PROFILE_"
    if profile_symbol.startswith(prefix):
        token = profile_symbol[len(prefix):]
    else:
        token = profile_symbol
    return "MUC_OPCUA_INTERN_PROFILE_" + token

def _default_condition_symbol(profile_key: str, profile_symbols: dict[str, str]) -> str:
    if profile_key == "custom":
        return profile_symbols[profile_key]
    return _internal_profile_symbol(profile_symbols[profile_key])
```

- [ ] **Step 3: Emit cascade symbols**

Replace `_emit_preset_symbols` with `_emit_internal_profile_symbols`. The cascade order is `standard -> embedded -> micro -> nano`; `full` also enables `standard` because project Full means Standard plus implemented extras.

```text
config MUC_OPCUA_INTERN_PROFILE_STANDARD_...
    bool
    default y if MUC_OPCUA_PROFILE_STANDARD_...
    default y if MUC_OPCUA_PROFILE_FULL_...

config MUC_OPCUA_INTERN_PROFILE_EMBEDDED_...
    bool
    default y if MUC_OPCUA_PROFILE_EMBEDDED_...
    default y if MUC_OPCUA_INTERN_PROFILE_STANDARD_...

config MUC_OPCUA_INTERN_PROFILE_MICRO_...
    bool
    default y if MUC_OPCUA_PROFILE_MICRO_...
    default y if MUC_OPCUA_INTERN_PROFILE_EMBEDDED_...

config MUC_OPCUA_INTERN_PROFILE_NANO_...
    bool
    default y if MUC_OPCUA_PROFILE_NANO_...
    default y if MUC_OPCUA_INTERN_PROFILE_MICRO_...
```

- [ ] **Step 4: Simplify profile choice**

Remove `depends on MUC_OPCUA_FACETS_MATCH_*` and `default y if MUC_OPCUA_FACETS_MATCH_*` from named profile configs. The `choice` should select exactly what the defconfig/user selected. Keep `custom` as fallback default.

- [ ] **Step 5: Update defconfig generation**

For named profiles, emit only the selected `MUC_OPCUA_PROFILE_<TOKEN>=y`. Do not emit preset seeds. For custom, emit `MUC_OPCUA_PROFILE_CUSTOM=y`.

- [ ] **Step 6: Update CMake feature list**

Replace `MUC_OPCUA_PROFILE_PRESET_*` entries in `MUC_OPCUA_KCONFIG_FEATURES` with `MUC_OPCUA_INTERN_PROFILE_*` entries.

## Task 2: Generate Globally Editable Profile/Facet/CU Tree

**Files:**
- Modify: `scripts/profile_manifest/generate.py`
- Regenerate: `Kconfig`

**Interfaces:**
- Consumes: `manifest["profiles"]`, `manifest["items"]`, `manifest["facet_containment"]`, item `profile_defaults`
- Produces: globally visible `menu "Profile: <name>"` sections

- [ ] **Step 1: Derive item profile membership**

Use item/facet `profile_defaults` as the local graph proxy. A profile section includes a facet if the facet defaults true for that profile, or any contained CU defaults true for that profile. A profile section includes a bare CU if the CU defaults true for that profile and is not in `facet_containment`.

- [ ] **Step 2: Keep all profile sections visible**

Emit every named profile section regardless of selected profile. Do not add `depends on MUC_OPCUA_PROFILE_*` to profile menus.

- [ ] **Step 3: Preserve one-symbol-one-location**

Kconfig cannot define the same symbol in multiple menu locations. Use one canonical location per facet/CU. If the same facet/CU belongs to multiple profile sections, emit it in the lowest profile section that reaches it, and add help text listing additional profiles that include it.

- [ ] **Step 4: Emit facet toggles and contained CUs**

Facet toggles default from `_default_condition_symbol(lowest_profile)`. Contained CUs depend on the facet toggle and default from `_default_condition_symbol(lowest_profile)`.

- [ ] **Step 5: Emit bare profile CUs**

Bare profile CUs are CUs not listed under any facet in `facet_containment`. Emit them directly under the canonical profile section. They default from `_default_condition_symbol(lowest_profile)` and do not depend on a facet.

## Task 3: Preserve Build Compatibility and Capacity Behavior

**Files:**
- Modify: `scripts/profile_manifest/generate.py`
- Modify: `include/muc_opcua/capacities.h` through regeneration
- Modify: `src/CMakeLists.txt` only if generated variable names require it

**Interfaces:**
- Consumes: resolved Kconfig symbols from `scripts/kconfig/gen_config.py`
- Produces: unchanged `MU_INTERN_MAX_*` capacity macros and existing C gate variables

- [ ] **Step 1: Leave capacity menu separate**

Capacity defaults should reference internal profile symbols through `_default_condition_symbol`.

- [ ] **Step 2: Keep legacy C gates available**

Until source migration is complete, keep emitting legacy bare symbols such as `SECURITY`, `SUBSCRIPTIONS`, and `BASE_NODES` where source still uses them. Their defaults should follow the same lowest-profile/default graph as the corresponding facet/CU.

- [ ] **Step 3: Confirm autoconf contains selected CU symbols**

After configure, `muc_opcua_autoconf.h` should define `MUC_OPCUA_CU_*` symbols that are selected and omit deselected ones.

## Task 4: Update Validation and Gating Tests

**Files:**
- Modify: `scripts/profile_manifest/validate.py`
- Modify: `scripts/test_profile_gating.sh`

**Interfaces:**
- Consumes: generated `Kconfig`, defconfigs, and CMake output files
- Produces: validation commands that pass with graph-derived semantics

- [ ] **Step 1: Update structure checks**

Validation should require:

```text
MUC_OPCUA_INTERN_PROFILE_ symbols exist
menu "Profile: " exists
menu "Facet: " exists
"CU:" prompts exist
menu "Capacities" exists
```

- [ ] **Step 2: Remove facets-match assumptions**

Any validation that expects named profiles to auto-switch to custom when a facet changes must be replaced with checks that the resulting feature set changes while the selected profile choice remains the user-selected preset.

- [ ] **Step 3: Add cross-profile activation test**

Extend `scripts/test_profile_gating.sh` with a scenario where `nano` is selected and a higher-profile facet/CU is enabled. Expected result: selected higher-profile CU/facet is ON, unrelated higher-profile facets remain OFF.

## Task 5: Regenerate and Verify

**Files:**
- Regenerate generated outputs only through `scripts/profile_manifest/generate.py`

- [ ] **Step 1: Regenerate**

Run:

```bash
python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
```

Expected: command exits 0 and updates generated files.

- [ ] **Step 2: Validate manifest and generated artifacts**

Run:

```bash
python3 scripts/profile_manifest/validate.py --all
```

Expected: `manifest: OK` or equivalent success output.

- [ ] **Step 3: Run profile gating tests**

Run:

```bash
bash scripts/test_profile_gating.sh
```

Expected: all scenarios pass with 0 failures.

- [ ] **Step 4: Run representative CTest builds**

Run:

```bash
cmake -S . -B build/standard-graph-kconfig -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/standard-graph-kconfig
ctest --test-dir build/standard-graph-kconfig --output-on-failure
cmake -S . -B build/full-graph-kconfig -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/full-graph-kconfig
ctest --test-dir build/full-graph-kconfig --output-on-failure
```

Expected: both configure/build/test sequences pass.

## Self-Review

- Spec coverage: covers profile selection as defaults, lower-profile auto-selection, globally editable non-selected profile sections, facet drilldown, bare CU drilldown, CU-level subtraction, and capacity output.
- Placeholder scan: no TBD/TODO placeholders remain.
- Type consistency: helper names and symbol names are consistent with existing generator patterns.
