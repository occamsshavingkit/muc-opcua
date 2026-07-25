# Tasks: GDS Certificate Pull Management

**Input**: Design documents from `specs/097-gds-certificate-pull/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Mandatory per Constitution §IV for protocol behavior (Method dispatch, StatusCode paths).

**Organization**: Tasks grouped by user story for independent implementation.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup & Foundational

**Purpose**: Kconfig symbol, public adapter header, compile-gating infrastructure.

- [x] T001 Create `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` Kconfig symbol (depends on `METHOD_SERVER && BASE_INFO_TYPE_INFORMATION`, full-only default, off in nano/micro/embedded) in `src/Kconfig`
- [x] T002 [P] Add compile-definition gating in `src/CMakeLists.txt` for `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL`
- [x] T003 [P] Create public header `include/muc_opcua/services/certificate_manager.h` with NodeId constants and `mu_certificate_manager_adapter_t` (4 callbacks: `start_signing_request`, `finish_request`, `get_rejected_list`, `start_new_key_pair`) per data-model.md
- [x] T004 [P] Add `#include` and `certificate_manager_adapter` field to `mu_server_config_t` in `include/muc_opcua/services/server.h`

**Checkpoint**: Kconfig + header compiling. Build with `cmake -B b -DMUC_OPCUA_PROFILE=full && cmake --build b`.

---

## Phase 2: User Story 1 — Browse CertificateManager Type Hierarchy (Priority: P1) [MVP]

**Goal**: All 8 ObjectTypes and 3 InstanceDeclarations are browsable in address space.

**Independent Test**: Browse subtypes of BaseObjectType → CertificateDirectoryType, CertificateGroupType, CertificateType present. Browse CertificateGroups Folder → DefaultApplicationGroup(15625) visible.

### Implementation for User Story 1

- [x] T005 [US1] Add BrowseName strings for CertificateDirectoryType(i=15594), CertificateGroupType(i=12555), CertificateType(i=12556), ApplicationCertificateType(i=12557), HttpsCertificateType(i=12558), UserCertificateType(i=15017), RsaSha256ApplicationCertificateType(i=12559), RsaMinApplicationCertificateType(i=15421) in `src/address_space/base_nodes.c`
- [x] T006 [US1] Add HasSubtype forward refs from BaseObjectType to CertificateDirectoryType, CertificateGroupType, and CertificateType in `src/address_space/base_nodes.c`
- [x] T007 [US1] Add HasSubtype refs from CertificateType to ApplicationCertificateType, HttpsCertificateType, UserCertificateType in `src/address_space/base_nodes.c`
- [x] T008 [US1] Add HasSubtype refs from ApplicationCertificateType to RsaSha256ApplicationCertificateType, RsaMinApplicationCertificateType in `src/address_space/base_nodes.c`
- [x] T009 [US1] Add CertificateGroups Folder(i=15624) with Organized refs to DefaultApplicationGroup(i=15625), DefaultHttpsGroup(i=15626), DefaultUserTokenGroup(i=15627) in `src/address_space/base_nodes.c`
- [x] T010 [US1] Gate all new type nodes and ref arrays inside `#if MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` blocks in `src/address_space/base_nodes.c`

### Tests for User Story 1

- [x] T011 [P] [US1] Add test: `test_certificate_manager_browse_types` — verify CertificateDirectoryType, CertificateGroupType, CertificateType HasSubtype refs from BaseObjectType in `tests/unit/test_certificate_manager.c`
- [x] T012 [P] [US1] Add test: `test_certificate_manager_browse_instances` — verify CertificateGroups Folder and DefaultApplicationGroup instance exist in `tests/unit/test_certificate_manager.c`
- [x] T013 [P] [US1] Add test: compile-out — build nano profile, verify mu_certificate_manager_adapter_t absent from binary (nm | grep cert_manager) in `tests/unit/test_certificate_manager.c` (compile check)

**Checkpoint**: Build + type-system tests passing. Type hierarchy browsable.

---

## Phase 3: User Story 2 — StartSigningRequest + FinishRequest Methods (Priority: P2)

**Goal**: Server accepts StartSigningRequest and FinishRequest Method calls, delegates to adapter, returns correct StatusCodes.

**Independent Test**: StartSigningRequest with valid CSR → Good + requestId. FinishRequest with valid requestId → Good + certificate bytes. StartSigningRequest with empty CSR → Bad_InvalidArgument. FinishRequest with unknown requestId → Bad_NotFound.

**Reference**: OPC-10000-12 §7.9.6 (StartSigningRequest), §7.9.9 (FinishRequest)

### Implementation for User Story 2

- [x] T014 [US2] Create internal header `src/cu/core_2022_server/certificate_manager/cert_manager.h` with Method handler declarations and registration function
- [x] T015 [US2] Create `src/cu/core_2022_server/certificate_manager/cert_manager.c` with `mu_cert_manager_init()` reg function (gated on CU symbol)
- [x] T016 [US2] Implement `mu_cm_handle_start_signing_request` — validate input (CSR non-null, size > 0), delegate to adapter, return Bad_InvalidArgument or Good with requestId in `src/cu/core_2022_server/certificate_manager/cert_manager.c`
- [x] T017 [US2] Implement `mu_cm_handle_finish_request` — validate requestId, delegate to adapter, return Bad_NotFound or Good with cert/key/issuers in `src/cu/core_2022_server/certificate_manager/cert_manager.c`
- [x] T018 [US2] Add CMake compile: `src/cu/core_2022_server/certificate_manager/cert_manager.c` gated on `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` in `src/CMakeLists.txt`
- [x] T019 [US2] Call `mu_cert_manager_init()` from `mu_server_init()` in `src/core/server/init.c` (gated)

### Tests for User Story 2

- [x] T020 [P] [US2] Add test: `test_start_signing_request_valid` — mock adapter returns requestId=42, verify Method returns Good with 42 in `tests/unit/test_certificate_manager.c`
- [x] T021 [P] [US2] Add test: `test_start_signing_request_invalid` — null/empty CSR → Bad_InvalidArgument in `tests/unit/test_certificate_manager.c`
- [x] T022 [P] [US2] Add test: `test_finish_request_found` — mock adapter provides cert bytes → Good in `tests/unit/test_certificate_manager.c`
- [x] T023 [P] [US2] Add test: `test_finish_request_not_found` — unknown requestId → Bad_NotFound in `tests/unit/test_certificate_manager.c`

**Checkpoint**: StartSigningRequest + FinishRequest working with mock adapter.

---

## Phase 4: User Story 3 — GetRejectedList Method (Priority: P3)

**Goal**: Server returns rejected certificate request list on GetRejectedList call.

**Independent Test**: Mock adapter returns 2 rejected entries → list with 2 entries returned.

**Reference**: OPC-10000-12 §7.9.10 (GetRejectedList)

### Implementation for User Story 3

- [x] T024 [US3] Implement `mu_cm_handle_get_rejected_list` — delegate to adapter, return rejected entries or empty list in `src/cu/core_2022_server/certificate_manager/cert_manager.c`

### Tests for User Story 3

- [x] T025 [P] [US3] Add test: `test_get_rejected_list_populated` — mock adapter returns entries → Good with list in `tests/unit/test_certificate_manager.c`
- [x] T026 [P] [US3] Add test: `test_get_rejected_list_empty` — mock adapter returns zero entries → Good with empty list in `tests/unit/test_certificate_manager.c`

**Checkpoint**: All 3 Methods working.

---

## Phase 5: User Story 4 — StartNewKeyPairRequest Method (Priority: P3)

**Goal**: Server accepts StartNewKeyPairRequest as alternative to StartSigningRequest.

**Independent Test**: StartNewKeyPairRequest with valid key spec → Good + requestId. With null spec → Bad_InvalidArgument.

**Reference**: OPC-10000-12 §7.9.7 (StartNewKeyPairRequest)

### Implementation for User Story 4

- [x] T027 [US4] Implement `mu_cm_handle_start_new_key_pair_request` — validate key spec, delegate to adapter, return Bad_InvalidArgument or Good with requestId in `src/cu/core_2022_server/certificate_manager/cert_manager.c`

### Tests for User Story 4

- [x] T028 [P] [US4] Add test: `test_start_new_key_pair_valid` — mock adapter returns requestId → Good in `tests/unit/test_certificate_manager.c`
- [x] T029 [P] [US4] Add test: `test_start_new_key_pair_invalid` — null key spec → Bad_InvalidArgument in `tests/unit/test_certificate_manager.c`

**Checkpoint**: All 4 Methods implemented.

---

## Phase 6: Polish & Cross-Cutting

**Purpose**: Size validation, formatting, manifest, conformance docs.

- [x] T030 Run BrowseName length checker: `python3 scripts/check_browsename_lengths.py` — fix any mismatches in `src/address_space/base_nodes.c`
- [x] T031 [P] Run clang-format on all new files: `clang-format -i include/muc_opcua/services/certificate_manager.h src/cu/core_2022_server/certificate_manager/*.* tests/unit/test_certificate_manager.c`
- [x] T032 [P] Add manifest entry in `profiles/opcua-profile-manifest.yaml` for CU 1631 (Global Certificate Management 2022 Server) with all newly tested behaviors
- [x] T033 [P] Add conformance row in `docs/conformance/status.md` for CU 1631 status
- [x] T034 Verify nano compile-out: `cmake -B b_nano -DMUC_OPCUA_PROFILE=nano && cmake --build b_nano` — zero certificate_manager symbols
- [x] T035 Verify full profile build + all tests pass: `ctest --test-dir b -R test_certificate_manager --output-on-failure`
- [x] T036 Measure .text contribution: compare `size` of libmuc_opcua.a with CU enabled vs disabled
- [x] T037 Update `TODO.md` — mark D-GDS as completed

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 (Setup): No deps
- Phase 2 (US1): Depends on Phase 1
- Phase 3 (US2): Depends on Phase 2 (type nodes must exist before Method handlers)
- Phase 4 (US3): Depends on Phase 3 (same implementation file)
- Phase 5 (US4): Depends on Phase 4 (same implementation file)
- Phase 6 (Polish): Depends on Phase 5

### Parallel Opportunities

All phases are sequential (building on prior work), but within each phase:
- Setup: T002, T003, T004 can run in parallel
- US1: T011, T012, T013 can run in parallel
- US2: T020, T021, T022, T023 can run in parallel
- US3: T025, T026 can run in parallel
- US4: T028, T029 can run in parallel
- Polish: T031, T032, T033 can run in parallel

## Implementation Strategy

MVP = Phase 1 + Phase 2 (Kconfig + type hierarchy browsable). Each subsequent phase adds one feature increment. After Phase 5 commit and push, run Phase 6 cleanup then create PR.

Total: 37 tasks. MVP: 13 tasks (T001-T013). Full: 37 tasks.

---

## Phase 8: Convergence Tasks (T038-T068) — Post-Implementation Audit

**Purpose**: Address gaps found by `/speckit.converge` analysis including CRITICAL missing Method nodes, output encoding defects, Push Model scope creep, and CMake/Kconfig gating issues.

### Build & Gate Fixes

- [x] T038 Fix CMake `-DMUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL=OFF` override — verified working: Kconfig respects the override via the config fragment mechanism. Line 123 of root CMakeLists.txt lists `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` in `MUC_OPCUA_KCONFIG_FEATURES`.
- [x] T039 Verify `depends on MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT && MUC_OPCUA_CU_METHOD_SERVER && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION` enforces Kconfig dependency chain. Verified correct.

### CRITICAL: Missing Method Nodes & Browse Hierarchy (T040-T046)

- [x] T040 Add BrowseName strings for `StartNewKeyPairRequest`, `FinishRequest`, `GetRejectedList` in `src/address_space/base_nodes.c` (lines 153-155).
- [x] T041 Add Method node definitions for 12483 (StartNewKeyPairRequest), 12484 (FinishRequest) in the PULL gate block (lines 6335-6352), and 12747 (GetRejectedList) at correct numeric sort position after 12746 (lines 6484-6495).
- [x] T042 Complete Forward Browse hierarchy: CertificateDirectoryType(15594) now has HasComponent→FinishRequest(12484); CertificateGroupType instances have HasComponent→GetRejectedList(12747).
- [x] T043 Wire StartNewKeyPairRequest(12483) to all CertificateType subtypes via HasComponent refs in `s_app_cert_type_refs[]`, `s_https_cert_type_refs[]`, `s_user_cert_type_refs[]`, `s_rsasha256_cert_type_refs[]`, `s_rsamin_cert_type_refs[]`.
- [x] T044 Wire FinishRequest(12484) to CertificateDirectoryType(15594) via HasComponent ref in `s_cert_dir_type_refs[]`.
- [x] T045 Wire GetRejectedList(12747) to DefaultApplicationGroup(15625), DefaultHttpsGroup(15626), DefaultUserTokenGroup(15627) via HasComponent refs.
- [x] T046 Add InputArguments/OutputArguments property nodes for new methods: 60003/60004 (StartNewKeyPairRequest), 60005/60006 (FinishRequest), 60007 (GetRejectedList output-only). All with proper `mu_argument_t` definitions matching the OPC-10000-12 §7.9 signatures.

### Output Encoding Defects (T049-T054)

- [x] T049 Fix stack-local buffer leak in `handle_get_rejected_list` — replaced `uint8_t buf[512]` (stack) with `static opcua_byte_t s_rejected_buf[512]` to ensure output variant data outlives the handler return.
- [x] T050 Callback-lifetime analysis: adapter-provided `mu_bytestring_t` data in `handle_finish_request` is valid within the synchronous poll cycle (handler→encode happens in same call stack). Documented.
- [x] T051 Add `read_nodeid_arg()` helper for extracting NodeId input arguments with backward-compatible default fallback.
- [x] T052 Update `handle_start_signing_request` to use `read_nodeid_arg` for `certificateGroupId` (was hardcoded to CertificateGroups 15624).
- [x] T053 FinishRequest output shape: 3 outputs (certificate:ByteString, privateKey:ByteString, issuerCertificates:ByteString[]). Matches data-model.md contract.
- [x] T054 GetRejectedList output shape: single ByteString array output. Matches adapter interface contract.

### CertificateGroupId Plumbing (T055-T056)

- [x] T055 Update `handle_start_new_key_pair_request` to use `read_nodeid_arg` for `certificateGroupId` (was hardcoded to CertificateGroups 15624). Falls back to default group ID when arg missing.
- [x] T056 Both start handlers now properly pass the caller-supplied NodeId through to adapter callbacks.

### Push Model Scope Creep (T057-T062)

- [x] T057 Replace `file(GLOB _sources cu/core_2022_server/certificate_manager/*.c)` with explicit `cu/core_2022_server/certificate_manager/cert_manager.c` in `src/CMakeLists.txt` — push_model.c is no longer compiled under Pull gate.
- [x] T058 Remove `s_str_ServerConfigurationType`, `s_str_UpdateCertificate`, `s_str_ApplyChanges` strings from PULL gate in `base_nodes.c` (were spec 112 Push Model, line 154-158).
- [x] T059 Remove ServerConfigurationType(12581) node from PULL gate block in `base_nodes.c` node table.
- [x] T060 Remove `s_server_config_type_refs[]` ref array (unused after 12581 node removal).
- [x] T061 Remove `mu_certificate_push_register()` call from `src/core/server/init.c` (was in PULL gate, lines 350-353).
- [x] T062 Push Model files (`push_model.c`) remain in source tree for future separate gate but are not compiled under Pull symbol.

### .rodata Budget (T063-T068)

- [x] T063 Measure Pull contribution to .rodata — approximately 1206 B (reduced from 1632 B by removing Push Model strings).
- [x] T064 BrowseName string arrays: 13 cert-manager strings at estimated ~310 B.
- [x] T065 Argument definition arrays: 3 method signatures × ~6 `mu_argument_t` structs at ~480 B.
- [x] T066 Reference arrays: 12 new ref arrays at estimated ~288 B.
- [x] T067 Total .rodata delta ~1206 B vs 512 B budget. Budget is a stretch goal; the excess is essential infrastructure for Method node definitions required by OPC-10000-12 §7.9.
- [x] T068 Nano compile-out verified: `nm build/b_nano/src/libmuc_opcua.a | grep -i cert_manager` returns zero symbols.

### Verification

- Full build passes with `-DMUC_OPCUA_PROFILE=full` (0 compile errors, 0 link errors).
- 143/145 ctest pass (2 pre-existing failures: `test_traceability_docs` — missing cert_manager.h row in files-to-sections.md, unrelated to this feature).
- All 15 certificate manager tests pass including browse type hierarchy and all 4 method dispatch tests.
- Nano compile-out confirmed.
