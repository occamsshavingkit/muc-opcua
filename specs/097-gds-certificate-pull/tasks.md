# Tasks: GDS Certificate Pull Management

**Input**: Design documents from `specs/097-gds-certificate-pull/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Mandatory per Constitution §IV for protocol behavior (Method dispatch, StatusCode paths).

**Organization**: Tasks grouped by user story for independent implementation.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup & Foundational

**Purpose**: Kconfig symbol, public adapter header, compile-gating infrastructure.

- [ ] T001 Create `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` Kconfig symbol (depends on `METHOD_SERVER && BASE_INFO_TYPE_INFORMATION`, full-only default, off in nano/micro/embedded) in `src/Kconfig`
- [ ] T002 [P] Add compile-definition gating in `src/CMakeLists.txt` for `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL`
- [ ] T003 [P] Create public header `include/muc_opcua/services/certificate_manager.h` with NodeId constants and `mu_certificate_manager_adapter_t` (4 callbacks: `start_signing_request`, `finish_request`, `get_rejected_list`, `start_new_key_pair`) per data-model.md
- [ ] T004 [P] Add `#include` and `certificate_manager_adapter` field to `mu_server_config_t` in `include/muc_opcua/services/server.h`

**Checkpoint**: Kconfig + header compiling. Build with `cmake -B b -DMUC_OPCUA_PROFILE=full && cmake --build b`.

---

## Phase 2: User Story 1 — Browse CertificateManager Type Hierarchy (Priority: P1) [MVP]

**Goal**: All 8 ObjectTypes and 3 InstanceDeclarations are browsable in address space.

**Independent Test**: Browse subtypes of BaseObjectType → CertificateDirectoryType, CertificateGroupType, CertificateType present. Browse CertificateGroups Folder → DefaultApplicationGroup(15625) visible.

### Implementation for User Story 1

- [ ] T005 [US1] Add BrowseName strings for CertificateDirectoryType(i=15594), CertificateGroupType(i=12555), CertificateType(i=12556), ApplicationCertificateType(i=12557), HttpsCertificateType(i=12558), UserCertificateType(i=15017), RsaSha256ApplicationCertificateType(i=12559), RsaMinApplicationCertificateType(i=15421) in `src/address_space/base_nodes.c`
- [ ] T006 [US1] Add HasSubtype forward refs from BaseObjectType to CertificateDirectoryType, CertificateGroupType, and CertificateType in `src/address_space/base_nodes.c`
- [ ] T007 [US1] Add HasSubtype refs from CertificateType to ApplicationCertificateType, HttpsCertificateType, UserCertificateType in `src/address_space/base_nodes.c`
- [ ] T008 [US1] Add HasSubtype refs from ApplicationCertificateType to RsaSha256ApplicationCertificateType, RsaMinApplicationCertificateType in `src/address_space/base_nodes.c`
- [ ] T009 [US1] Add CertificateGroups Folder(i=15624) with Organized refs to DefaultApplicationGroup(i=15625), DefaultHttpsGroup(i=15626), DefaultUserTokenGroup(i=15627) in `src/address_space/base_nodes.c`
- [ ] T010 [US1] Gate all new type nodes and ref arrays inside `#if MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` blocks in `src/address_space/base_nodes.c`

### Tests for User Story 1

- [ ] T011 [P] [US1] Add test: `test_certificate_manager_browse_types` — verify CertificateDirectoryType, CertificateGroupType, CertificateType HasSubtype refs from BaseObjectType in `tests/unit/test_certificate_manager.c`
- [ ] T012 [P] [US1] Add test: `test_certificate_manager_browse_instances` — verify CertificateGroups Folder and DefaultApplicationGroup instance exist in `tests/unit/test_certificate_manager.c`
- [ ] T013 [P] [US1] Add test: compile-out — build nano profile, verify mu_certificate_manager_adapter_t absent from binary (nm | grep cert_manager) in `tests/unit/test_certificate_manager.c` (compile check)

**Checkpoint**: Build + type-system tests passing. Type hierarchy browsable.

---

## Phase 3: User Story 2 — StartSigningRequest + FinishRequest Methods (Priority: P2)

**Goal**: Server accepts StartSigningRequest and FinishRequest Method calls, delegates to adapter, returns correct StatusCodes.

**Independent Test**: StartSigningRequest with valid CSR → Good + requestId. FinishRequest with valid requestId → Good + certificate bytes. StartSigningRequest with empty CSR → Bad_InvalidArgument. FinishRequest with unknown requestId → Bad_NotFound.

**Reference**: OPC-10000-12 §7.9.6 (StartSigningRequest), §7.9.9 (FinishRequest)

### Implementation for User Story 2

- [ ] T014 [US2] Create internal header `src/cu/core_2022_server/certificate_manager/cert_manager.h` with Method handler declarations and registration function
- [ ] T015 [US2] Create `src/cu/core_2022_server/certificate_manager/cert_manager.c` with `mu_cert_manager_init()` reg function (gated on CU symbol)
- [ ] T016 [US2] Implement `mu_cm_handle_start_signing_request` — validate input (CSR non-null, size > 0), delegate to adapter, return Bad_InvalidArgument or Good with requestId in `src/cu/core_2022_server/certificate_manager/cert_manager.c`
- [ ] T017 [US2] Implement `mu_cm_handle_finish_request` — validate requestId, delegate to adapter, return Bad_NotFound or Good with cert/key/issuers in `src/cu/core_2022_server/certificate_manager/cert_manager.c`
- [ ] T018 [US2] Add CMake compile: `src/cu/core_2022_server/certificate_manager/cert_manager.c` gated on `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` in `src/CMakeLists.txt`
- [ ] T019 [US2] Call `mu_cert_manager_init()` from `mu_server_init()` in `src/core/server/init.c` (gated)

### Tests for User Story 2

- [ ] T020 [P] [US2] Add test: `test_start_signing_request_valid` — mock adapter returns requestId=42, verify Method returns Good with 42 in `tests/unit/test_certificate_manager.c`
- [ ] T021 [P] [US2] Add test: `test_start_signing_request_invalid` — null/empty CSR → Bad_InvalidArgument in `tests/unit/test_certificate_manager.c`
- [ ] T022 [P] [US2] Add test: `test_finish_request_found` — mock adapter provides cert bytes → Good in `tests/unit/test_certificate_manager.c`
- [ ] T023 [P] [US2] Add test: `test_finish_request_not_found` — unknown requestId → Bad_NotFound in `tests/unit/test_certificate_manager.c`

**Checkpoint**: StartSigningRequest + FinishRequest working with mock adapter.

---

## Phase 4: User Story 3 — GetRejectedList Method (Priority: P3)

**Goal**: Server returns rejected certificate request list on GetRejectedList call.

**Independent Test**: Mock adapter returns 2 rejected entries → list with 2 entries returned.

**Reference**: OPC-10000-12 §7.9.10 (GetRejectedList)

### Implementation for User Story 3

- [ ] T024 [US3] Implement `mu_cm_handle_get_rejected_list` — delegate to adapter, return rejected entries or empty list in `src/cu/core_2022_server/certificate_manager/cert_manager.c`

### Tests for User Story 3

- [ ] T025 [P] [US3] Add test: `test_get_rejected_list_populated` — mock adapter returns entries → Good with list in `tests/unit/test_certificate_manager.c`
- [ ] T026 [P] [US3] Add test: `test_get_rejected_list_empty` — mock adapter returns zero entries → Good with empty list in `tests/unit/test_certificate_manager.c`

**Checkpoint**: All 3 Methods working.

---

## Phase 5: User Story 4 — StartNewKeyPairRequest Method (Priority: P3)

**Goal**: Server accepts StartNewKeyPairRequest as alternative to StartSigningRequest.

**Independent Test**: StartNewKeyPairRequest with valid key spec → Good + requestId. With null spec → Bad_InvalidArgument.

**Reference**: OPC-10000-12 §7.9.7 (StartNewKeyPairRequest)

### Implementation for User Story 4

- [ ] T027 [US4] Implement `mu_cm_handle_start_new_key_pair_request` — validate key spec, delegate to adapter, return Bad_InvalidArgument or Good with requestId in `src/cu/core_2022_server/certificate_manager/cert_manager.c`

### Tests for User Story 4

- [ ] T028 [P] [US4] Add test: `test_start_new_key_pair_valid` — mock adapter returns requestId → Good in `tests/unit/test_certificate_manager.c`
- [ ] T029 [P] [US4] Add test: `test_start_new_key_pair_invalid` — null key spec → Bad_InvalidArgument in `tests/unit/test_certificate_manager.c`

**Checkpoint**: All 4 Methods implemented.

---

## Phase 6: Polish & Cross-Cutting

**Purpose**: Size validation, formatting, manifest, conformance docs.

- [ ] T030 Run BrowseName length checker: `python3 scripts/check_browsename_lengths.py` — fix any mismatches in `src/address_space/base_nodes.c`
- [ ] T031 [P] Run clang-format on all new files: `clang-format -i include/muc_opcua/services/certificate_manager.h src/cu/core_2022_server/certificate_manager/*.* tests/unit/test_certificate_manager.c`
- [ ] T032 [P] Add manifest entry in `profiles/opcua-profile-manifest.yaml` for CU 1631 (Global Certificate Management 2022 Server) with all newly tested behaviors
- [ ] T033 [P] Add conformance row in `docs/conformance/status.md` for CU 1631 status
- [ ] T034 Verify nano compile-out: `cmake -B b_nano -DMUC_OPCUA_PROFILE=nano && cmake --build b_nano` — zero certificate_manager symbols
- [ ] T035 Verify full profile build + all tests pass: `ctest --test-dir b -R test_certificate_manager --output-on-failure`
- [ ] T036 Measure .text contribution: compare `size` of libmuc_opcua.a with CU enabled vs disabled
- [ ] T037 Update `TODO.md` — mark D-GDS as completed

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
