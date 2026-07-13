# Tasks: OPC-Named Kconfig Redesign

**Input**: Design documents from `specs/068-opc-named-kconfig-redesign/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: No new C protocol tests are required because this feature renames and restructures manifest/generator/CMake/Kconfig surfaces without adding parsing, serialization, service dispatch, StatusCode, SecureChannel, Session, address-space, or security behavior. Verification is through `validate.py --all`, `test_profile_gating.sh`, full CTest suite, and size/headroom checks.

**Task quality gate**: Each task has one goal. Do not batch unrelated edits into a single assignment. A task is atomic only if splitting it further would remove the coherent outcome described in that task.

**OPC UA reference gate**: Tasks that affect Profile, Facet, or Conformance Unit structure are grounded in OPC-10000-7 §4.2 (Conformance Units and Conformance Groups) and OPC-10000-7 §4.3 (Server Profiles). CU-specific source sections are the existing manifest `opc_reference.spec` + `opc_reference.section` values, including OPC-10000-4 §5.4, §5.8, §5.9, §5.10.2, §5.13.2, §5.14.2, §5.14.5; OPC-10000-5 §6.3; OPC-10000-6 §5.3 and §5.4. Tasks must preserve these citations and must not create new conformance claims.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions
- Include an `OPC reference` sentence for any task that affects OPC Profile/Facet/CU behavior or citations

---

## Phase 1: Setup (Manifest Restructuring)

**Purpose**: Add OPC display names and Facet containment as data before generator logic changes.

- [X] T001 Add `opc_display_name` to profile entries in `profiles/opcua-profile-manifest.yaml` using OPC UA standard profile names from `profiles/opcua-profile-snapshot.json`. OPC reference: OPC-10000-7 §4.3.
- [X] T002 Add `opc_display_name` to facet items in `profiles/opcua-profile-manifest.yaml` using each item's existing `opc_reference.facet` value. OPC reference: OPC-10000-7 §4.2 and each facet item's existing `opc_reference.spec` + `opc_reference.section`.
- [X] T003 Add `opc_display_name` to conformance-unit items in `profiles/opcua-profile-manifest.yaml` using each item's existing `opc_reference.cu_name` value. OPC reference: OPC-10000-7 §4.2 and each CU item's existing `opc_reference.spec` + `opc_reference.section`.
- [X] T004 Add `opc_display_name` to non-OPC optimization items in `profiles/opcua-profile-manifest.yaml` using descriptive internal names. OPC reference: N/A - project option labels only, no OPC behavior or claim.
- [X] T005 Add `facet_containment` top-level key to `profiles/opcua-profile-manifest.yaml` mapping each facet item ID to the CU item IDs whose `opc_reference.facet` names that Facet. OPC reference: OPC-10000-7 §4.2 and manifest item `opc_reference` fields.
- [X] T006 Update `scripts/profile_manifest/model.py` to require non-empty `opc_display_name` for `kind: "facet"` and `kind: "conformance_unit"` items. OPC reference: OPC-10000-7 §4.2 and manifest item `opc_reference` fields.
- [X] T007 Update `scripts/profile_manifest/model.py` to validate `facet_containment` keys reference `kind: "facet"` items and values reference `kind: "conformance_unit"` items. OPC reference: OPC-10000-7 §4.2.
- [X] T008 Run `python3 scripts/profile_manifest/validate.py --manifest-only` and fix only validation errors introduced by T006-T007. Expected: `manifest: OK`.
- [X] T009 Record pre-redesign standard/full profile binary size and RAM-linked section sizes in implementation notes before generator changes begin. OPC reference: N/A - size discipline baseline only.

**Checkpoint**: Manifest has OPC display names and Facet containment. Validation passes.

---

## Phase 2: Foundational (Symbol Naming Helper)

**Purpose**: Add the symbol naming algorithm utility used by all subsequent generator changes.

- [X] T010 Add `compute_kconfig_symbol(opc_display_name: str, kind: str) -> str` to `scripts/profile_manifest/generate.py` implementing the naming algorithm from `research.md` Decision 3: strip redundant trailing kind word, uppercase, replace non-alnum with underscore, collapse underscores, trim, and prefix with `MUC_OPCUA_{PROFILE|FACET|CU|OPT}_`.
- [X] T011 Add unit tests for `compute_kconfig_symbol` in `scripts/profile_manifest/test_naming.py` covering: "Core 2017 Server Facet" → `MUC_OPCUA_FACET_CORE_2017_SERVER`, "Attribute Read" → `MUC_OPCUA_CU_ATTRIBUTE_READ`, and "Embedded 2017 UA Server Profile" → `MUC_OPCUA_PROFILE_EMBEDDED_2017_UA_SERVER`. Run `python3 -m pytest scripts/profile_manifest/test_naming.py -v`; expected: all pass.
- [X] T012 Update `scripts/profile_manifest/generate.py` `_item_prompt()` to use `opc_display_name` when present instead of id-derived prompts.

**Checkpoint**: Naming algorithm is implemented, tested, and used for prompts.

---

## Phase 3: User Story 1 - Profile Selector with OPC Names (Priority: P1) [MVP]

**Goal**: The Kconfig profile choice uses OPC UA standard names and `MUC_OPCUA_PROFILE_<NAME>` symbols.

**Independent Test**: `python3 scripts/profile_manifest/generate.py --outputs kconfig,defconfigs && grep "Standard 2017 UA Server Profile" Kconfig && grep "MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER" configs/standard.defconfig` - both produce matches.

- [X] T013 [US1] Update `scripts/profile_manifest/generate.py` `generate_kconfig()` profile `choice` entries to use computed `MUC_OPCUA_PROFILE_<NAME>` symbols. OPC reference: OPC-10000-7 §4.3.
- [X] T014 [US1] Update `scripts/profile_manifest/generate.py` `generate_kconfig()` profile `choice` prompts to use OPC UA standard profile display names from `opc_display_name`. OPC reference: OPC-10000-7 §4.3.
- [X] T015 [US1] Update `scripts/profile_manifest/generate.py` profile help text to include each profile's OPC UA profile URI from the manifest or snapshot. OPC reference: OPC-10000-7 §4.3.
- [X] T016 [US1] Update `scripts/profile_manifest/generate.py` `generate_defconfig()` to emit computed OPC profile symbols instead of `PROFILE_NANO`, `PROFILE_MICRO`, `PROFILE_EMBEDDED`, `PROFILE_STANDARD`, and `PROFILE_FULL`. OPC reference: OPC-10000-7 §4.3.
- [X] T017 [US1] Update `scripts/profile_manifest/generate.py` `_emit_default()` to reference computed OPC profile symbols in profile-conditioned defaults. OPC reference: OPC-10000-7 §4.3.
- [X] T018 [US1] Update `scripts/profile_manifest/generate.py` `_capacity_prompt()` to reference computed OPC profile symbols in profile-specific capacity prompts. OPC reference: OPC-10000-7 §4.3 for profile selection; capacity values remain non-OPC project configuration.
- [X] T019 [US1] Regenerate `Kconfig`, `configs/*.defconfig`, and `include/muc_opcua/capacities.h` with `python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfigs,capacities_h`; verify `Standard 2017 UA Server Profile` exists in `Kconfig` and `MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER=y` exists in `configs/standard.defconfig`.

**Checkpoint**: Profile choice uses OPC standard names. Defconfigs select OPC-named symbols.

---

## Phase 4: User Story 2 - Facet/CU Drilldown with OPC Names (Priority: P1)

**Goal**: Kconfig shows Facet drilldown menus with CU entries inside, all using OPC standard display names.

**Independent Test**: `python3 scripts/profile_manifest/generate.py --outputs kconfig && grep "Facet: Core 2017 Server" Kconfig && grep "CU: Attribute Read" Kconfig` - both produce matches.

- [X] T020 [US2] Update `scripts/profile_manifest/generate.py` `generate_kconfig()` to replace the flat `menu "Server"` block with `menu "OPC UA Facets and Conformance Units"`. OPC reference: OPC-10000-7 §4.2.
- [X] T021 [US2] Update `scripts/profile_manifest/generate.py` `generate_kconfig()` to emit one nested `menu "Facet: <opc_display_name>"` for each implemented or visible Facet item in `facet_containment`. OPC reference: OPC-10000-7 §4.2 and each facet item's `opc_reference`.
- [X] T022 [US2] Update `scripts/profile_manifest/generate.py` `generate_kconfig()` to place each contained CU under its canonical Facet menu using `facet_containment`. OPC reference: OPC-10000-7 §4.2 and each CU item's `opc_reference.spec` + `opc_reference.section`.
- [X] T023 [US2] Update `scripts/profile_manifest/generate.py` `_emit_selectable()` to use computed `MUC_OPCUA_CU_<NAME>` symbols for CU entries. OPC reference: OPC-10000-7 §4.2 and each CU item's `opc_reference`.
- [X] T024 [US2] Update `scripts/profile_manifest/generate.py` `_emit_selectable()` to emit CU prompts as `CU: <opc_display_name>`. OPC reference: OPC-10000-7 §4.2 and each CU item's `opc_reference`.
- [X] T025 [US2] Update `scripts/profile_manifest/generate.py` to keep unimplemented or deferred OPC items visible as `comment "<opc_display_name> (NOT IMPLEMENTED) [<source>]"` entries, not `config` symbols. OPC reference: OPC-10000-7 §4.2 and each item's `opc_reference`.
- [X] T026 [US2] Update `scripts/profile_manifest/generate.py` to emit optimization items under `menu "Project options"` and exclude them from the OPC Facet/CU tree. OPC reference: N/A - project options are non-OPC implementation controls.
- [X] T027 [US2] Update `scripts/profile_manifest/validate.py` `_check_generated()` to fail if generated Kconfig has no `Facet:` menu or no `CU:` prompt. OPC reference: OPC-10000-7 §4.2.
- [X] T028 [US2] Update `scripts/profile_manifest/validate.py` `_check_unimplemented_availability()` to verify unimplemented OPC items are Kconfig comments and not selectable symbols. OPC reference: OPC-10000-7 §4.2 and each item's `opc_reference`.
- [X] T029 [US2] Regenerate `Kconfig`, defconfigs, capacities, claim map, roadmap, and build docs; verify `grep -c "Facet:" Kconfig` is at least 1 and `grep -c "CU:" Kconfig` is at least 5.

**Checkpoint**: Kconfig tree has OPC Facet drilldown menus with OPC-named CU entries.

---

## Phase 5: User Story 3 - Facet Group Toggles (Priority: P2)

**Goal**: Each Facet menu has a group toggle (`MUC_OPCUA_FACET_<NAME>`) that disables all contained CUs when off. Individual CUs can be toggled only while their Facet is enabled.

**Independent Test**: Generate Kconfig, verify each Facet menu contains a `config MUC_OPCUA_FACET_<NAME>` bool and all contained CUs have `depends on MUC_OPCUA_FACET_<NAME>`.

- [X] T030 [US3] Update `scripts/profile_manifest/generate.py` to emit a Facet group toggle symbol `MUC_OPCUA_FACET_<NAME>` as the first `config` in each Facet menu. OPC reference: OPC-10000-7 §4.2 and each facet item's `opc_reference`.
- [X] T031 [US3] Update `scripts/profile_manifest/generate.py` to emit Facet toggle prompts as `Enable <opc_display_name>`. OPC reference: OPC-10000-7 §4.2 and each facet item's `opc_reference`.
- [X] T032 [US3] Update `scripts/profile_manifest/generate.py` to set each Facet toggle `default y if` condition from the same profile choice symbols used by contained CU defaults. OPC reference: OPC-10000-7 §4.2 and OPC-10000-7 §4.3.
- [X] T033 [US3] Update `scripts/profile_manifest/generate.py` to add `depends on MUC_OPCUA_FACET_<NAME>` to every CU `config` inside its Facet menu. OPC reference: OPC-10000-7 §4.2 and each CU item's `opc_reference`.
- [X] T034 [US3] Add a validation assertion in `scripts/profile_manifest/validate.py` proving generated CU configs do not `select MUC_OPCUA_FACET_<NAME>`. OPC reference: OPC-10000-7 §4.2; this preserves the researched group-off behavior.
- [X] T035 [US3] Regenerate Kconfig and verify `grep "MUC_OPCUA_FACET_" Kconfig | head -5` shows Facet toggle symbols and `grep "depends on MUC_OPCUA_FACET_" Kconfig | wc -l` matches the generated CU count.

**Checkpoint**: Facet group toggles work. CUs depend on their Facet and do not auto-enable it.

---

## Phase 6: User Story 4 - Profile to Custom Transition (Priority: P2)

**Goal**: When user changes any Facet or CU from the named profile defaults, the effective profile switches to custom.

**Independent Test**: Configure standard profile, override a Facet, and verify `MUC_OPCUA_PROFILE_CUSTOM=y` in the resolved `.config`.

- [X] T036 [US4] Add generation of non-interactive `MUC_OPCUA_FACETS_MATCH_<profile>` bool symbols to `scripts/profile_manifest/generate.py`. OPC reference: OPC-10000-7 §4.2 and OPC-10000-7 §4.3.
- [X] T037 [US4] Make each `MUC_OPCUA_FACETS_MATCH_<profile>` symbol compare only that profile's controlled Facet defaults against resolved Kconfig symbols. OPC reference: OPC-10000-7 §4.2 and OPC-10000-7 §4.3.
- [X] T038 [US4] Make each `MUC_OPCUA_FACETS_MATCH_<profile>` symbol compare that profile's controlled CU defaults against resolved Kconfig symbols. OPC reference: OPC-10000-7 §4.2, OPC-10000-7 §4.3, and each CU item's `opc_reference`.
- [X] T039 [US4] Update the profile choice section in `scripts/profile_manifest/generate.py` so each named profile has `default y if MUC_OPCUA_FACETS_MATCH_<profile>`. OPC reference: OPC-10000-7 §4.3.
- [X] T040 [US4] Update the profile choice section in `scripts/profile_manifest/generate.py` so `MUC_OPCUA_PROFILE_CUSTOM` is the unconditional final fallback after named profile defaults. OPC reference: OPC-10000-7 §4.3 for named profiles; custom is project-defined.
- [X] T041 [US4] Rename the internal generated marker `STANDARD_PROFILE` to `MUC_OPCUA_MARKER_STANDARD_PROFILE` in `scripts/profile_manifest/generate.py`. OPC reference: N/A - internal build marker only.
- [X] T042 [US4] Update `profiles/opcua-profile-manifest.yaml` `advertised_profile_markers` id from `STANDARD_PROFILE` to `MUC_OPCUA_MARKER_STANDARD_PROFILE`. OPC reference: N/A - internal build marker only.
- [X] T043 [US4] Add a kconfiglib-based validation case to `scripts/profile_manifest/validate.py` that resolves each named profile without overrides and confirms the named profile remains selected. OPC reference: OPC-10000-7 §4.3.
- [X] T044 [US4] Add a kconfiglib-based validation case to `scripts/profile_manifest/validate.py` that overrides a standard-profile Facet and confirms `MUC_OPCUA_PROFILE_CUSTOM=y`. OPC reference: OPC-10000-7 §4.2 and OPC-10000-7 §4.3.
- [X] T045 [US4] Regenerate and run `cmake -S . -B build/custom-check -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_FACET_CORE_2017_SERVER=OFF`; verify `grep MUC_OPCUA_PROFILE_CUSTOM build/custom-check/.config` shows `=y`.

**Checkpoint**: Profile→custom transition works when Facet/CU defaults are overridden.

---

## Phase 7: User Story 5 - Capacities and Legacy Override Preservation (Priority: P3)

**Goal**: Capacities remain in their own menu with profile defaults. Legacy `-DMU_MAX_*=N` overrides still work. Build and source gating use new OPC-named symbols.

**Independent Test**: `cmake -S . -B build/capacity-check -DMUC_OPCUA_PROFILE=standard -DMU_MAX_SESSIONS=42 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build/capacity-check` succeeds, and `grep "MU_MAX_SESSIONS" build/capacity-check/compile_commands.json` contains `42`.

- [X] T046 [US5] Inspect generated `include/muc_opcua/capacities.h` references to determine whether any profile symbol guards require renaming. OPC reference: OPC-10000-7 §4.3 for profile-conditioned defaults; capacity values are project configuration.
- [X] T047 [US5] Replace any old profile guards emitted by `scripts/profile_manifest/generate.py` `generate_capacities_h()` with computed OPC-named profile symbols. OPC reference: OPC-10000-7 §4.3 for profile-conditioned defaults.
- [X] T048 [US5] Update `CMakeLists.txt` `MUC_OPCUA_KCONFIG_FEATURES` list to replace old project-centric feature symbols with computed OPC-name-derived Profile/Facet/CU symbols. OPC reference: OPC-10000-7 §4.2, OPC-10000-7 §4.3, and manifest item `opc_reference` fields.
- [X] T049 [US5] Update `CMakeLists.txt` legacy profile shorthand handling so `MUC_OPCUA_STANDARD_PROFILE` and `MUC_OPCUA_EMBEDDED_PROFILE` trigger the new OPC profile symbols. OPC reference: OPC-10000-7 §4.3.
- [X] T050 [US5] Update `src/CMakeLists.txt` build gating references from old project-centric symbols to new OPC-name-derived symbols. OPC reference: OPC-10000-7 §4.2 and manifest item `opc_reference` fields.
- [X] T051 [US5] Update C preprocessor feature guards under `src/` from old project-centric symbols to new OPC-name-derived symbols. OPC reference: OPC-10000-7 §4.2 and manifest item `opc_reference` fields. This task only changes `#if/#ifdef` guard names, not protocol behavior.
- [X] T052 [US5] Update C preprocessor feature guards under `include/` and `tests/` from old project-centric symbols to new OPC-name-derived symbols. OPC reference: OPC-10000-7 §4.2 and manifest item `opc_reference` fields. This task only changes `#if/#ifdef` guard names, not protocol behavior.
- [X] T053 [US5] Verify capacity override end-to-end with `cmake -S . -B build/capacity-check -DMUC_OPCUA_PROFILE=standard -DMU_MAX_SESSIONS=42 -DMUC_OPCUA_BUILD_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build/capacity-check`; expected: build succeeds and compile commands contain `-DMU_MAX_SESSIONS=42`.

**Checkpoint**: Capacities preserved. Legacy overrides work. Build/source gating uses new symbol names.

---

## Phase 8: Polish & Cross-Cutting Validation

**Purpose**: Full test suite, drift checks, naming convention validation, task-level conformance references, and size/headroom verification.

- [X] T054 [P] Update `scripts/test_profile_gating.sh` `assert_cfg` calls to use OPC-name-derived symbols computed from each manifest item's `opc_display_name` and `kind`. OPC reference: OPC-10000-7 §4.2, OPC-10000-7 §4.3, and manifest item `opc_reference` fields.
- [X] T055 [P] Update `scripts/profile_manifest/validate.py` to add naming convention checks: no generated Facet symbol may end with `_FACET` after the prefix, and no generated Profile symbol may end with `_PROFILE` after the prefix.
- [X] T056 [P] Update `scripts/profile_manifest/validate.py` to add Kconfig structure checks: at least one `Facet:` label exists, at least one `CU:` label exists, and `Project options` menu exists. OPC reference: OPC-10000-7 §4.2.
- [X] T057 [P] Update `scripts/profile_manifest/validate.py` to verify every generated Profile/Facet/CU Kconfig help entry includes the corresponding manifest OPC source reference. OPC reference: OPC-10000-7 §4.2, OPC-10000-7 §4.3, and each item `opc_reference`.
- [X] T058 Run full generation and validation: `python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs && python3 scripts/profile_manifest/validate.py --all`. Expected: all checks pass, no drift, no naming violations, and no missing OPC references.
- [X] T059 Run profile gating tests: `bash scripts/test_profile_gating.sh`. Expected: all checks pass, at least 29 checks.
- [X] T060 Run full test suite for standard profile: `cmake -S . -B build/standard-final -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build build/standard-final && ctest --test-dir build/standard-final --output-on-failure`. Expected: 132/132 tests pass.
- [X] T061 Run full test suite for full profile: `cmake -S . -B build/full-final -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build build/full-final && ctest --test-dir build/full-final --output-on-failure`. Expected: 132/132 tests pass.
- [X] T062 Verify no old project-centric symbols appear in generated config: `grep -E 'MUC_OPCUA_SERVICE_READ|MUC_OPCUA_SERVICE_BROWSE|MUC_OPCUA_BASE_NODES|MUC_OPCUA_SECURITY[^_]' build/standard-final/.config`. Expected: no output.
- [X] T063 Compare binary size and RAM-linked sections for `build/standard-final` and `build/full-final` against the pre-redesign baseline recorded by T009. Expected: no increase attributable to Kconfig renaming/menu restructuring; document any delta in implementation notes.
- [X] T064 Verify quickstart.md validation path by following each step in `specs/068-opc-named-kconfig-redesign/quickstart.md` and confirming all commands produce expected output.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - manifest data changes only.
- **Foundational (Phase 2)**: Depends on Phase 1. Naming algorithm needs display names.
- **US1 (Phase 3)**: Depends on Phase 2. Profile selector needs naming helper.
- **US2 (Phase 4)**: Depends on Phase 3. Facet/CU drilldown builds on profile choice.
- **US3 (Phase 5)**: Depends on Phase 4. Facet toggles need Facet menus.
- **US4 (Phase 6)**: Depends on Phase 5. Custom transition needs Facet/CU symbols.
- **US5 (Phase 7)**: Depends on Phase 4. Build/source updates need generated symbol names.
- **Polish (Phase 8)**: Depends on all prior phases.

### Within Each Phase

- Manifest data changes before validation.
- Generator changes before regeneration.
- Regenerate before verifying output.
- CMake/source updates after generator produces final symbol names.
- Size/headroom comparison runs after standard and full builds exist.

### Parallel Opportunities

- T001-T004 can run in parallel if assigned to different manifest item categories.
- T006-T007 can run in parallel after T001-T005 complete.
- T054-T057 can run in parallel after prior phases complete.
- T060 and T061 can run in parallel in separate build directories.

---

## Implementation Strategy

### MVP Scope (Phases 1-4 + Phase 8 validation)

Deliver US1 + US2: OPC-named profile selector with Facet/CU drilldown. This provides immediate user value because users can see OPC structure in Kconfig for the first time. US3-US5 can follow incrementally.

### Incremental Delivery

1. **MVP**: Phases 1-4 + validation -> profile selector + Facet/CU menu (US1+US2)
2. **Increment 2**: Phase 5 -> Facet group toggles (US3)
3. **Increment 3**: Phase 6 -> Profile→custom (US4)
4. **Increment 4**: Phase 7 -> CMake/C source updates + capacity verification (US5)
5. **Final**: Phase 8 -> full validation pass

### Task Count Summary

| Phase | Tasks | Story |
|-------|-------|-------|
| 1: Setup | 9 | - |
| 2: Foundational | 3 | - |
| 3: US1 | 7 | Profile selector |
| 4: US2 | 10 | Facet/CU drilldown |
| 5: US3 | 6 | Facet toggles |
| 6: US4 | 10 | Profile→custom |
| 7: US5 | 8 | Capacities + CMake/source |
| 8: Polish | 11 | Validation |
| **Total** | **64** | |

### Independent Test Criteria

- **US1**: Kconfig contains OPC profile names and defconfigs select OPC symbols.
- **US2**: Kconfig contains `Facet:` menus and `CU:` entries with OPC names.
- **US3**: Facet toggle disables all contained CUs; individual CUs are toggleable only when the Facet is enabled.
- **US4**: Overriding a Facet/CU switches profile to custom.
- **US5**: Capacity override `-DMU_MAX_SESSIONS=42` reaches compiled binary.
