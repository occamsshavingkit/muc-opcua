# Tasks: Server Nano Core Type System & Base Info CUs

**Input**: Design documents from `/specs/069-nano-type-system/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**OPC UA Normative References**: OPC-10000-3 §4 (AddressSpace), OPC-10000-5 (NodeIds), OPC-10000-7 §4.2 (Conformance Units, Core 2022 Server Facet id 1322)

**Organization**: Tasks grouped by CU category. Each task is atomic — one CU or tightly coupled CU pair.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different CUs, no shared code)
- **[Story]**: User story label (US-REF = ReferenceTypes, US-VT = VariableTypes, US-DT = DataTypes/ObjectTypes, US-PROP = Server Properties, US-KC = Kconfig, US-ACC = AccessLevelEx, US-DOC = Documentation)

---

## Phase 1: Foundation — NodeId Constants

- [ ] T001 [P] Add NodeId constants for known CUs to `include/muc_opcua/opcua_ids.h`:
  - `MU_NODEID_HASADDIN = 17604` (OPC-10000-5 §11.21)
  - `MU_NODEID_BASEINTERFACETYPE = 17602` (OPC-10000-5 §6.9)
  - `MU_NODEID_HASINTERFACE = 17603` (OPC-10000-5 §11.20)
  - `MU_NODEID_OPTIONSETTYPE = 11487` (OPC-10000-5 §7.17)
  - `MU_NODEID_SELECTIONLISTTYPE = 16309` (OPC-10000-5 §7.18)
  - `MU_NODEID_TIMEZONEDATATYPE = 8912` (OPC-10000-5 §12.2.12.11)
  - `MU_NODEID_EUINFORMATION = 887` (OPC-10000-5 §12.2.12.1 if confirmed)
  - `MU_NODEID_CURRENCYUNITTYPE = 23498` (OPC-10000-5 §12.2.12.2)

- [ ] T002 Look up remaining NodeIds from OPC-10000-5 and add to `include/muc_opcua/opcua_ids.h`:
  - Server.LocalTime Property NodeId
  - Server.EstimatedReturnTime Property NodeId
  - Locations Folder NodeId (under Objects)
  - InterfaceTypes Folder NodeId (under Types)
  - CurrencyUnit Property NodeId
  - ValueAsText Property NodeId
  - DefaultInstanceBrowseName Property NodeId
  - EngineeringUnits Property NodeId

## Phase 2: ReferenceType CUs (US-REF)

- [ ] T003 [P] [US-REF] Implement CU 2446 (Address Space AddIn Reference, optional):
  Add HasAddIn ReferenceType node (i=17604) under References folder in `src/address_space/base_nodes.c`.
  Gated on `#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE`.
  Add ReferenceType node with subtype of NonHierarchicalReferences (i=32).
  Add inverse reference from References folder.
  Add to Kconfig: `config MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE`, `default n`.
  OPC-10000-5 §11.21, OPC-10000-7 CU 2446.

- [ ] T004 [P] [US-REF] Implement CU 3560 (Address Space Interfaces, optional):
  Add BaseInterfaceType ObjectType (i=17602) under ObjectTypes folder in `src/address_space/base_nodes.c`.
  Add HasInterface ReferenceType (i=17603) under References folder.
  Add InterfaceTypes Folder under Types.
  Gated on `#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES`.
  Add to Kconfig: `config MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES`, `default n`.
  OPC-10000-5 §6.9, §11.20, OPC-10000-7 CU 3560.

## Phase 3: VariableType CUs (US-VT)

- [ ] T005 [P] [US-VT] Implement CU 3127 (Base Info OptionSet, optional):
  Add OptionSetType VariableType (i=11487) under VariableTypes folder in `src/address_space/base_nodes.c`.
  Subtype of BaseDataVariableType (i=63).
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_OPTIONSET`.
  Add to Kconfig: `config MUC_OPCUA_CU_BASE_INFO_OPTIONSET`, `default n`.
  OPC-10000-5 §7.17, OPC-10000-7 CU 3127.

- [ ] T006 [P] [US-VT] Implement CU 2711 (Base Info Selection List, optional):
  Add SelectionListType VariableType (i=16309) under VariableTypes folder in `src/address_space/base_nodes.c`.
  Subtype of BaseDataVariableType (i=63).
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST`.
  Add to Kconfig: `config MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST`, `default n`.
  OPC-10000-5 §7.18, OPC-10000-7 CU 2711.

## Phase 4: DataType & ObjectType CUs (US-DT)

- [ ] T007 [P] [US-DT] Implement CU 2476 part 1 (Base Info LocalTime, optional):
  Add TimeZoneDataType DataType node (i=8912) under DataTypes folder in `src/address_space/base_nodes.c`.
  Subtype of Structure (i=22).
  Add Encoding Object node for binary encoding.
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_LOCALTIME`.
  OPC-10000-5 §12.2.12.11, OPC-10000-7 CU 2476.

- [ ] T008 [P] [US-DT] Implement CU 5592 (Base Info Engineering Units, optional):
  Add EUInformation DataType node under DataTypes folder in `src/address_space/base_nodes.c`.
  Add EngineeringUnits as a Property Type on Variables.
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS`.
  Add to Kconfig: `config MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS`, `default n`.
  OPC-10000-7 CU 5592.

- [ ] T009 [P] [US-DT] Implement CU 5240 part 1 (Base Info Currency, optional):
  Add CurrencyUnitType DataType node (i=23498) under DataTypes folder in `src/address_space/base_nodes.c`.
  Add Encoding Object node.
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY`.
  OPC-10000-5 §12.2.12.2, OPC-10000-7 CU 5240.

## Phase 5: Server Property & Folder CUs (US-PROP)

- [ ] T010 [P] [US-PROP] Implement CU 2476 part 2 (LocalTime Property):
  Add LocalTime Variable on Server Object as HasProperty reference in `src/address_space/base_nodes.c`.
  Type: TimeZoneDataType (i=8912), PropertyType.
  Gated on same `#ifdef MUC_OPCUA_CU_BASE_INFO_LOCALTIME` as T007.
  OPC-10000-5 §6.3.1.

- [ ] T011 [P] [US-PROP] Implement CU 3198 (Base Info Estimated Return Time, optional):
  Add EstimatedReturnTime Variable on Server Object as HasProperty reference in `src/address_space/base_nodes.c`.
  Type: DateTime (i=13), PropertyType.
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME`.
  Add to Kconfig: `config MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME`, `default n`.
  OPC-10000-5 §6.3.1, OPC-10000-7 CU 3198.

- [ ] T012 [P] [US-PROP] Implement CU 4053 (Base Info Locations Object, optional):
  Add Locations Folder (Object) under Objects folder in `src/address_space/base_nodes.c`.
  Type: FolderType (i=61).
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT`.
  Add to Kconfig: `config MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT`, `default n`.
  OPC-10000-7 CU 4053.

- [ ] T013 [P] [US-PROP] Implement CU 5240 part 2 (CurrencyUnit Property):
  Add CurrencyUnit Property definition for DataVariables in `src/address_space/base_nodes.c`.
  Gated on same `#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY` as T009.
  OPC-10000-7 CU 5240.

- [ ] T014 [P] [US-PROP] Implement CU 2969 (Base Info ValueAsText, optional):
  Add ValueAsText Property node as PropertyType in `src/address_space/base_nodes.c`.
  Document that this Property applies to enumerated DataType Variables.
  Gated on `#ifdef MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT`.
  Add to Kconfig: `config MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT`, `default n`.
  OPC-10000-7 CU 2969.

- [ ] T015 [P] [US-PROP] Implement CU 2447 (AddIn DefaultInstanceBrowsename, optional):
  Add DefaultInstanceBrowseName Property for ObjectTypes in `src/address_space/base_nodes.c`.
  Document that ObjectTypes may carry this Property to define the default BrowseName
  for instances.
  Gated on `#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME`.
  Add to Kconfig: `config MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME`, `default n`.
  OPC-10000-7 CU 2447.

## Phase 6: AccessLevelEx Flag CUs (US-ACC)

- [ ] T016 [P] [US-ACC] Implement CU 2809 (Address Space Atomicity, mandatory):
  Add `MU_ACCESS_LEVEL_EX_NONATOMIC_READ` and `MU_ACCESS_LEVEL_EX_NONATOMIC_WRITE` bitmask
  constants to `include/muc_opcua/types.h`. Document usage: set bits in AccessLevelEx
  when Reads/Writes are non-atomic. Gate on `MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY`.
  Add to Kconfig: `config MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY`, `default y if MUC_OPCUA_INTERN_PROFILE_NANO`.
  OPC-10000-7 CU 2809.

- [ ] T017 [P] [US-ACC] Implement CU 2820 (Address Space Full Array Only, mandatory):
  Add `MU_ACCESS_LEVEL_EX_WRITE_FULL_ARRAY_ONLY` bitmask constant to
  `include/muc_opcua/types.h`. Gate on `MUC_OPCUA_CU_ADDRESS_SPACE_FULL_ARRAY_ONLY`.
  Add to Kconfig: `config MUC_OPCUA_CU_ADDRESS_SPACE_FULL_ARRAY_ONLY`, `default y if MUC_OPCUA_INTERN_PROFILE_NANO`.
  OPC-10000-7 CU 2820.

- [ ] T018 [P] [US-ACC] Implement CU 4237 (Address Space NonVolatile and Constant, optional):
  Add `MU_ACCESS_LEVEL_EX_NON_VOLATILE` and `MU_ACCESS_LEVEL_EX_CONSTANT` bitmask
  constants to `include/muc_opcua/types.h`. Gate on `MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT`.
  Add to Kconfig: `config MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT`, `default n`.
  OPC-10000-7 CU 4237.

## Phase 7: Kconfig & Defconfig Integration (US-KC)

- [ ] T019 [US-KC] Add all new `MUC_OPCUA_CU_*` config symbols to Kconfig under
  the `if MUC_OPCUA_FACET_CORE_2022_SERVER` block. Follow existing pattern:
  use `config`, `bool`, `help` with CU description and OPC reference.
  Mandatory CUs (2809, 2820) get `default y if MUC_OPCUA_INTERN_PROFILE_NANO`.
  Optional CUs get `default n`.
  Verify with `python3 scripts/kconfig/gen_config.py --profile nano`.

- [ ] T020 [P] [US-KC] Regenerate Kconfig and nano defconfig from manifest:
  Run `python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfig`.

## Phase 8: Documentation CUs (US-DOC)

- [ ] T021 [P] [US-DOC] Resolve CU 3808 (Documentation - Core Capacities, mandatory):
  Verify `docs/build-and-gating.md` capacity tables cover nano capacities.
  If gaps exist, update table with max_sessions, max_connections, max_subscriptions, etc.
  This is a documentation-only CU; no code changes.
  Mark as resolved in manifest.
  OPC-10000-7 CU 3808.

- [ ] T022 [P] [US-DOC] Resolve CU 3080 (Security Default ApplicationInstance Certificate, mandatory):
  Verify server creates default cert on init in `src/core/server/init.c`.
  Document in `docs/getting-started.md` or `docs/integration-guide.md`.
  Mark as resolved in manifest.
  OPC-10000-7 CU 3080.

- [ ] T023 [P] [US-DOC] Resolve CU 3201 (Base Info Custom Type System, optional):
  Document custom type system gating strategy: when user defines custom types,
  the Exposes Type System CU must be enabled. Document in
  `docs/build-and-gating.md` or `docs/integration-guide.md`.
  No code change needed. Mark as resolved in manifest.
  OPC-10000-7 CU 3201.

- [ ] T024 [P] [US-DOC] Resolve CU 5814 (Security - No Application Authentication, optional):
  Document No Application Authentication configuration option.
  Add to `docs/integration-guide.md` security section.
  No code change needed. Mark as resolved in manifest.
  OPC-10000-7 CU 5814.

## Phase 9: Validation & Verification

- [ ] T025 Build and test nano profile: `cmake -S . -B build/nano -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PROFILE=nano && cmake --build build/nano -j$(nproc) && ctest --test-dir build/nano --output-on-failure`

- [ ] T026 Verify nano size budget: `bash scripts/measure_size.sh nano`. Ensure nano elf_text <= 30 KB (baseline 27,080 + ~3 KB max for new nodes).

- [ ] T027 Run `tests/unit/test_profile_surface` to verify new nodes are browseable with their correct NodeIds and BrowseNames.

- [ ] T028 Run `tests/unit/test_traceability_docs` to ensure all new source references are mapped.

- [ ] T029 Run full CI matrix: `gh pr checks` equivalent — ensure `profile-tests (nano)`, `freestanding-core`, `pico-cross-compile`, `static-analysis`, `interop` all pass.

## Dependencies

```
T001 (NodeId constants)
  ├── T003, T004 (ReferenceType CUs) [P]
  ├── T005, T006 (VariableType CUs) [P]
  ├── T007, T008, T009 (DataType CUs) [P]
  ├── T010-T015 (Server Property CUs) [P]
  └── T016, T017, T018 (AccessLevelEx CUs) [P]
T019, T020 (Kconfig) — depends on all CU tasks above
T021-T024 (Documentation) — independent, can run any time [P]
T025-T029 (Validation) — after all implementation + Kconfig
```

## Parallel Opportunities

- All CU tasks within a phase (T003-T018) can run in parallel — they touch disjoint sections of `base_nodes.c`
- NodeId lookup (T002) can run in parallel with implementation tasks
- Documentation tasks (T021-T024) are independent of code changes
- Kconfig tasks (T019-T020) must wait for CU symbol names to be finalized

## Implementation Strategy

**MVP (first deployable increment)**: T001 + T016 + T017 + T019 + T025
— mandatory CUs only (2809, 2820), verified nano build passes.

**Full**: All tasks — 16 CUs, all optional CUs gated off by default.
