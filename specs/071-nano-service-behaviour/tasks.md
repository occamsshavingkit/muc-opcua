<!-- markdownlint-disable MD013 -->

# Tasks: Server Nano Core Service Behaviour CUs

**Input**: Design documents from `specs/071-nano-service-behaviour/`
**Prerequisites**: `spec.md`, `plan.md`, `research.md`, `data-model.md`, `class-diagram.md`, `quickstart.md`, `contracts/`, `checklists/behavior-testability.md`
**Propagated**: 2026-07-14 — Added atomic dedicated-symbol behavior-gating remediation tasks and propagated the approved `opc_cu_3147` deferral.

**OPC UA Normative References**: OPC-10000-7 section 4.2; OPC-10000-4 sections 5.5.1, 5.5.2, 5.5.4, 5.7.2, 5.7.3, 5.9.2, 5.9.3, 5.9.4, 5.11.4, 7.32, 7.38; OPC-10000-5 sections 6.3.1, 6.3.3, 8.3.2, 12.9; OPC-10000-6 section 5.2.1.

**Organization**: Tasks are grouped by user story and preserve the planned M+U scope: manifest/profile claim work plus unit, integration, fixture, and profile validation for observable service behaviour.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel with other tasks that touch different files and do not depend on incomplete tasks.
- **[Story]**: User story label. Setup, foundational, and final review tasks intentionally omit story labels.

## Phase 1: Setup and Audit

- [X] T001 Create the in-scope CU audit ledger in `specs/071-nano-service-behaviour/cu-audit.md` classifying `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, `opc_cu_2389`, `opc_cu_2400`, `opc_cu_2936`, `opc_cu_3147`, `opc_cu_3192`, `opc_cu_3530`, and `opc_cu_3983` as satisfied, needs symbol, needs tests, or needs behaviour, with OPC-10000 section evidence.
- [X] T002 [P] Audit current Discovery and View service proofs in `tests/unit/test_discovery_services.c`, `tests/unit/test_discovery_endpoint.c`, `tests/unit/test_browse_service.c`, and `tests/unit/test_browse_limits.c` against `SCN-001`, `CASE-001`, `CASE-002`, `CASE-008`, and `CASE-010` in `specs/071-nano-service-behaviour/cu-audit.md`.
- [X] T003 [P] Audit current Write and Session service proofs in `tests/unit/test_write_service.c`, `tests/unit/test_write_errors.c`, `tests/unit/test_write_response.c`, `tests/unit/test_write_decoder.c`, `tests/unit/test_session_auth.c`, and `tests/unit/test_session.c` against `SCN-002`, `CASE-003`, `CASE-004`, `CASE-007`, and `CASE-009` in `specs/071-nano-service-behaviour/cu-audit.md`.
- [X] T004 [P] Audit current Diagnostics proofs in `tests/unit/test_diagnostics.c`, `src/cu/core_2022_server/diagnostics/diagnostics.c`, and `src/address_space/base_nodes.c` against `SCN-003`, `CASE-005`, and `CASE-007` in `specs/071-nano-service-behaviour/cu-audit.md`.
- [X] T005 Record the final M+U scope decision for each in-scope CU in `specs/071-nano-service-behaviour/cu-audit.md`, preserving nano-default claims only for manifest entries whose `profile_defaults.nano` is true.

## Phase 2: Foundational Manifest and Test Harness Work

- [X] T006 Add reusable profile-default assertion coverage for the ten in-scope CU ids in `tests/conformance/check_claim_map.py`, binding evidence to `SCN-001`, `CASE-010`, and quickstart path 4.
- [X] T007 Add reusable CU claim/backing-test manifest validation coverage in `scripts/profile_manifest/validate.py`, enforcing `kconfig_symbol` and non-empty `backing_tests` before any in-scope CU becomes `implemented` per OPC-10000-7 section 4.2.
- [X] T008 [P] Add fixture helpers for encoded malformed-array, excessive-count, truncated-body, and invalid-NodeId requests in `tests/unit/test_write_decoder.c`, asserting `MU_STATUS_BAD_DECODINGERROR`, `MU_STATUS_BAD_REQUESTHEADERINVALID`, or `MU_STATUS_BAD_TOOMANYOPERATIONS` plus no state mutation for `SCN-004`, `CASE-006`, `CASE-008`, and OPC-10000-6 section 5.2.1.
- [X] T009 [P] Add fixture helpers for nano and optional profile CU enablement checks in `tests/unit/test_profile_surface.c`, binding evidence to `SCN-001`, `CASE-010`, and quickstart path 4.

## Phase 3: User Story 1 - Claim Existing Mandatory Discovery and View Behaviour (MVP)

**Goal**: Claim nano-default Discovery and View CUs only where observable service behaviour and tests back the manifest state.

**Independent Test**: Build the nano profile and verify `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, and `opc_cu_3530` have dedicated symbols, backing tests, and passing Discovery/View service behaviour without claiming optional write, session change-user, or diagnostics CUs.

- [X] T010 [US1] Add or update FindServers self tests in `tests/unit/test_discovery_services.c` for `SCN-001`, `CASE-001`, `opc_cu_2352`, and OPC-10000-4 section 5.5.2.
- [X] T011 [US1] Add or update GetEndpoints filter tests in `tests/unit/test_discovery_endpoint.c` for `SCN-001`, `CASE-001`, `opc_cu_2328`, and OPC-10000-4 sections 5.5.1 and 5.5.4.
- [X] T012 [US1] Add or update Browse and BrowseNext continuation-point tests in `tests/unit/test_browse_service.c` and `tests/unit/test_browse_limits.c` for `SCN-001`, `CASE-002`, `CASE-008`, `opc_cu_3530`, and OPC-10000-4 sections 5.9.2 and 5.9.3.
- [X] T013 [US1] Add or update TranslateBrowsePathsToNodeIds success and no-match tests in `tests/unit/test_browse_service.c` for `SCN-001`, `CASE-002`, `opc_cu_2317`, and OPC-10000-4 section 5.9.4.
- [X] T014 [US1] Align FindServers self behaviour in `src/cu/core_2022_server/discovery/find_servers.c` with `SCN-001`, `CASE-001`, `opc_cu_2352`, and OPC-10000-4 section 5.5.2.
- [X] T015 [US1] Align GetEndpoints filter behaviour in `src/cu/core_2022_server/discovery/get_endpoints.c` with `SCN-001`, `CASE-001`, `opc_cu_2328`, and OPC-10000-4 sections 5.5.1 and 5.5.4.
- [X] T016 [US1] Align Browse and BrowseNext bounded continuation-point behaviour in `src/cu/core_2022_server/view_basic_translate/browse.c` and `src/cu/core_2022_server/view_basic_translate/browse_next.c` with `SCN-001`, `CASE-002`, `CASE-008`, `opc_cu_3530`, and OPC-10000-4 sections 5.9.2 and 5.9.3.
- [X] T017 [US1] Align TranslateBrowsePathsToNodeIds result StatusCode behaviour in `src/cu/core_2022_server/view_basic_translate/translate_paths.c` with `SCN-001`, `CASE-002`, `opc_cu_2317`, and OPC-10000-4 section 5.9.4.
- [X] T018 [US1] Promote `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, and `opc_cu_3530` in `profiles/opcua-profile-manifest.yaml` with dedicated `MUC_OPCUA_CU_*` symbols, `implemented` state, nano-default preservation, backing test paths, and OPC-10000-4 section evidence.
- [X] T019 [US1] Regenerate profile artifacts from the manifest into `Kconfig`, `configs/nano.defconfig`, `configs/micro.defconfig`, `configs/embedded.defconfig`, `configs/standard.defconfig`, `configs/full.defconfig`, `configs/custom.defconfig`, `include/muc_opcua/capacities.h`, `tests/conformance/claim_test_map.md`, and `docs/conformance/` using quickstart path 2.
- [X] T020 [US1] Validate US1 contract evidence with `python3 scripts/profile_manifest/validate.py --all`, `bash scripts/test_profile_gating.sh`, and `ctest --test-dir build/pr-check --output-on-failure -R 'discovery|browse'`, recording command output in `specs/071-nano-service-behaviour/cu-audit.md`.

### Phase 3A: US1 Dedicated-Symbol Gating Remediation

- [X] T050 [US1] Add RED profile/service tests and wire `MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS` through `CMakeLists.txt`, `src/CMakeLists.txt`, `src/core/service_dispatch/dispatch_core.c`, and `src/cu/core_2022_server/discovery/get_endpoints.c` so disabling only `opc_cu_2328` returns an explicit unsupported StatusCode while other Discovery behavior remains available, grounded to OPC-10000-4 sections 5.5.1 and 5.5.4.
- [X] T051 [US1] Add RED profile/service tests and wire `MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF` through `CMakeLists.txt`, `src/CMakeLists.txt`, `src/core/service_dispatch/dispatch_core.c`, and `src/cu/core_2022_server/discovery/find_servers.c` so disabling only `opc_cu_2352` returns an explicit unsupported StatusCode while GetEndpoints remains available, grounded to OPC-10000-4 section 5.5.2.
- [X] T052 [US1] Add RED profile/service tests and wire `MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH` through `CMakeLists.txt`, `src/CMakeLists.txt`, and `src/cu/core_2022_server/view_basic_translate/view_handler.c` so disabling only `opc_cu_2317` rejects TranslateBrowsePathsToNodeIds without disabling Browse/BrowseNext, grounded to OPC-10000-4 section 5.9.4.
- [X] T053 [US1] Add RED profile/service tests and wire `MUC_OPCUA_CU_VIEW_BASIC_2` through `CMakeLists.txt`, `src/CMakeLists.txt`, `src/cu/core_2022_server/view_basic_translate/browse.c`, and `src/cu/core_2022_server/view_basic_translate/view_handler.c` so disabling only `opc_cu_3530` rejects Browse/BrowseNext without disabling TranslateBrowsePathsToNodeIds, grounded to OPC-10000-4 sections 5.9.2 and 5.9.3.

## Phase 4: User Story 2 - Complete Claim-Ready Optional Write and Session Behaviour

**Goal**: Make claim-ready optional Write and ActivateSession change-user CUs explicit, tested, and gated for micro-or-higher or custom profiles without enabling them in nano; preserve deferred `opc_cu_3147` as explicitly unsupported and unclaimed.

**Independent Test**: Enable the claim-ready CU symbols and verify Write value, StatusCode/Timestamp, disabled-CU, invalid target/type, and ActivateSession change-user requests produce cited service-level and operation-level StatusCodes; verify non-empty IndexRange writes remain unchanged and return `MU_STATUS_BAD_WRITENOTSUPPORTED` without claiming `opc_cu_3147`.

- [X] T021 [US2] Add or update Write value positive and invalid-target tests in `tests/unit/test_write_service.c` and `tests/unit/test_write_errors.c` for `SCN-002`, `CASE-003`, `CASE-009`, `opc_cu_2389`, and OPC-10000-4 section 5.11.4.
- [X] T022 [US2] Add or update Write StatusCode and Timestamp tests in `tests/unit/test_write_service.c` and `tests/unit/test_write_response.c` for `SCN-002`, `CASE-003`, `opc_cu_2936`, and OPC-10000-4 section 5.11.4.
- [X] T023 [US2] Add or update Write IndexRange success and invalid-range tests in `tests/unit/test_write_service.c` and `tests/unit/test_write_errors.c` for `SCN-002`, `CASE-003`, `CASE-009`, `opc_cu_3147`, and OPC-10000-4 section 5.11.4.
- [X] T024 [US2] Add or update disabled Write CU tests in `tests/unit/test_write_errors.c`, asserting `MU_STATUS_BAD_SERVICEUNSUPPORTED`, `MU_STATUS_BAD_NOTSUPPORTED`, or `MU_STATUS_BAD_WRITENOTSUPPORTED` plus unchanged target values for `SCN-002`, `CASE-007`, `opc_cu_2389`, `opc_cu_2936`, `opc_cu_3147`, and OPC-10000-4 section 5.11.4.
- [X] T025 [US2] Add or update ActivateSession change-user success and invalid-identity tests in `tests/unit/test_session_auth.c` and `tests/unit/test_session.c`, asserting `MU_STATUS_GOOD`, `MU_STATUS_BAD_IDENTITYTOKENINVALID`, `MU_STATUS_BAD_IDENTITYTOKENREJECTED`, `MU_STATUS_BAD_SESSIONIDINVALID`, or `MU_STATUS_BAD_SESSIONNOTACTIVATED` plus user/nonce invariants for `SCN-002`, `CASE-004`, `opc_cu_2400`, and OPC-10000-4 sections 5.7.2 and 5.7.3.
- [X] T026 [US2] Align Write value behaviour and per-node result handling in `src/cu/core_2022_server/attribute_write/write.c` with `SCN-002`, `CASE-003`, `CASE-009`, `opc_cu_2389`, and OPC-10000-4 section 5.11.4.
- [X] T027 [US2] Align Write StatusCode and Timestamp acceptance/rejection behaviour in `src/cu/core_2022_server/attribute_write/write.c` with `SCN-002`, `CASE-003`, `opc_cu_2936`, and OPC-10000-4 section 5.11.4.
- [X] T028 [US2] Align Write IndexRange partial-update and invalid-range behaviour in `src/core/service_dispatch/attribute_handler.c` with `SCN-002`, `CASE-003`, `CASE-009`, `opc_cu_3147`, and OPC-10000-4 section 5.11.4.
- [X] T029 [US2] Align ActivateSession change-user state transition and server nonce refresh in `src/core/service_dispatch/activate_session.c` and `src/services/session.c` with `SCN-002`, `CASE-004`, `opc_cu_2400`, and OPC-10000-4 sections 5.7.2 and 5.7.3.

### Phase 4A: US2 Dedicated-Symbol Gating Remediation

- [X] T054 [US2] Add RED enabled/disabled profile tests, promote `opc_cu_2389` with `MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES`, regenerate its profile artifacts, and wire the symbol through `CMakeLists.txt`, `src/CMakeLists.txt`, `src/core/service_dispatch/dispatch_core.c`, and `src/core/service_dispatch/attribute_handler.c` so Write Value behavior is available for micro-or-higher/custom opt-in and unavailable for nano/disabled custom builds, grounded to OPC-10000-4 section 5.11.4.
- [X] T055 [US2] Add RED enabled/disabled field tests, promote `opc_cu_2936` with `MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP` depending on `MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES`, regenerate its profile artifacts, and gate StatusCode/Timestamp acceptance in `src/core/service_dispatch/attribute_handler.c` so disabled builds explicitly reject those fields without disabling ordinary Value writes, grounded to OPC-10000-4 section 5.11.4.
- [X] T056 [US2] Add RED reactivation-disabled tests, promote `opc_cu_2400` with `MUC_OPCUA_CU_SESSION_CHANGE_USER`, regenerate its profile artifacts, and gate only already-activated Session identity replacement in `CMakeLists.txt`, `src/CMakeLists.txt`, and `src/core/service_dispatch/activate_session.c` while preserving initial ActivateSession behavior, grounded to OPC-10000-4 sections 5.7.2 and 5.7.3.
- [X] T057 [US2] Add a failing invalid-AttributeId Write test and align `src/core/service_dispatch/attribute_handler.c` to return the exact OPC-10000-4 section 5.11.4 operation-level StatusCode for an AttributeId that is invalid for the target Node, while retaining `MU_STATUS_BAD_NOTWRITABLE` for valid but non-writable attributes.

- [X] T030 [US2] Verify `opc_cu_2389`, `opc_cu_2400`, and `opc_cu_2936` manifest entries and generated artifacts have dedicated symbols that independently gate executable behavior, micro-or-higher defaults, nano default false, backing tests, dependencies, and OPC-10000-4 evidence; preserve deferred `opc_cu_3147` as unimplemented and unclaimed.
- [X] T031 [US2] Validate Write and Session data side effects with `ctest --test-dir build/pr-check --output-on-failure -R 'write|session'`, asserting field-level value/status/timestamp updates, concrete `MU_STATUS_BAD_NODEIDUNKNOWN`, `MU_STATUS_BAD_ATTRIBUTEIDINVALID`, `MU_STATUS_BAD_TYPEMISMATCH`, `MU_STATUS_BAD_NOTWRITABLE`, `MU_STATUS_BAD_WRITENOTSUPPORTED`, unchanged-value behaviour for unsupported IndexRange writes, and session user/nonce invariants in `specs/071-nano-service-behaviour/cu-audit.md` without claiming `opc_cu_3147`.

**Deferral resolved (2026-07-15)**: T023 and T028 are complete; Write IndexRange partial array updates are implemented in `write_value_index_range()` (`src/core/service_dispatch/attribute_handler.c`), bounded by the target array's own configured length. `opc_cu_3147` is now promoted and claimed: it has a dedicated `MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE` symbol (depends on `MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES`), enabled/disabled gating tests in `tests/unit/test_write_service.c`, regenerated profile artifacts, and `profile_defaults.nano: false` preserved, following the T054-T056 pattern. `TODO.md` item `S071-D1` is closed; see `specs/071-nano-service-behaviour/cu-audit.md`'s "T023/T028 Resolution" and "Promotion" sections for full evidence. All ten in-scope CUs from this feature are now claimed.

## Phase 5: User Story 3 - Expose and Return Diagnostics Consistently

**Goal**: Make Base Info Diagnostics and Base Services Diagnostics observable through address-space diagnostics nodes and response diagnostics only when enabled.

**Independent Test**: Enable diagnostics CUs, drive successful and failed services, read ServerDiagnostics nodes, request `returnDiagnostics`, and verify counters and diagnostic response fields match the enabled claims.

- [X] T032 [US3] Add or update ServerDiagnosticsSummary node tests in `tests/unit/test_diagnostics.c` for `SCN-003`, `CASE-005`, `opc_cu_3192`, and OPC-10000-5 sections 6.3.1, 6.3.3, 8.3.2, and 12.9.
- [X] T033 [US3] Add or update `returnDiagnostics` response-header tests in `tests/unit/test_diagnostics.c` for `SCN-003`, `CASE-005`, `opc_cu_3983`, and OPC-10000-4 sections 7.32 and 7.38.
- [X] T034 [US3] Add or update diagnostics-disabled tests in `tests/unit/test_diagnostics.c`, asserting no diagnostics CU claim, no fabricated DiagnosticInfo, and only explicit empty diagnostics or `MU_STATUS_BAD_NOTSUPPORTED`/`MU_STATUS_BAD_SERVICEUNSUPPORTED` where the service rejects diagnostics for `SCN-003`, `CASE-007`, `opc_cu_3192`, `opc_cu_3983`, and OPC-009 explicit no-false-claim behaviour.
- [X] T035 [US3] Align live diagnostics counter updates in `src/cu/core_2022_server/diagnostics/diagnostics.c`, `src/core/service_dispatch/dispatch_core.c`, `src/core/service_dispatch/create_session.c`, `src/core/service_dispatch/close_session.c`, and `src/core/server/poll.c` with `SCN-003`, `CASE-005`, `opc_cu_3192`, and OPC-10000-5 section 12.9.
- [X] T036 [US3] Align ServerDiagnostics address-space node exposure in `src/address_space/base_nodes.c` with `SCN-003`, `CASE-005`, `opc_cu_3192`, and OPC-10000-5 sections 6.3.1, 6.3.3, and 8.3.2.
- [X] T037 [US3] Align response-header diagnostic information handling in `src/services/service_header.c`, `src/core/service_dispatch/response_encode.c`, and `src/core/service_dispatch/dispatch_core.c` with `SCN-003`, `CASE-005`, `opc_cu_3983`, and OPC-10000-4 sections 7.32 and 7.38.

### Phase 5A: US3 Dedicated-Symbol Gating Remediation

- [X] T058 [US3] Add RED enabled/disabled diagnostics-surface tests, promote `opc_cu_3192` with a dedicated symbol, regenerate its profile artifacts, and wire the symbol through `CMakeLists.txt`, `src/CMakeLists.txt`, `src/cu/core_2022_server/diagnostics/diagnostics.c`, and `src/address_space/base_nodes.c` so Base Info Diagnostics counters/nodes compile out or remain unavailable when disabled, grounded to OPC-10000-5 sections 6.3.1, 6.3.3, 8.3.2, and 12.9.
- [X] T059 [US3] Add RED enabled/disabled response-diagnostics tests, promote `opc_cu_3983` with a dedicated symbol, regenerate its profile artifacts, and wire the symbol through `CMakeLists.txt`, `src/CMakeLists.txt`, `src/services/service_header.c`, `src/core/service_dispatch/response_encode.c`, and `src/core/service_dispatch/dispatch_core.c` so response diagnostics are unavailable and unclaimed when disabled, grounded to OPC-10000-4 sections 7.32 and 7.38.

- [X] T038 [US3] Verify `opc_cu_3192` and `opc_cu_3983` manifest entries and generated artifacts have dedicated symbols that independently gate executable behavior, micro-or-higher defaults, nano default false, backing tests, and OPC-10000-4/5 evidence.
- [X] T039 [US3] Validate diagnostics contract evidence with `ctest --test-dir build/pr-check --output-on-failure -R 'diagnostics'`, recording address-space node values, response diagnostics, disabled-CU behaviour, and quickstart path 3 output in `specs/071-nano-service-behaviour/cu-audit.md`.

## Phase 6: Cross-Story Artifact Regeneration and Integration Validation

- [X] T040 Regenerate all manifest-derived artifacts in `Kconfig`, `configs/nano.defconfig`, `configs/micro.defconfig`, `configs/embedded.defconfig`, `configs/standard.defconfig`, `configs/full.defconfig`, `configs/custom.defconfig`, `include/muc_opcua/capacities.h`, `tests/conformance/claim_test_map.md`, and `docs/conformance/` using quickstart path 2.
- [X] T041 Validate manifest and generated artifact consistency with `python3 scripts/profile_manifest/validate.py --all` and record evidence in `specs/071-nano-service-behaviour/cu-audit.md`.
- [X] T042 Validate focused service behaviour with `cmake --build build/pr-check --target test_discovery_services test_discovery_endpoint test_browse_service test_write_service test_write_errors test_session_auth test_session test_diagnostics` and `ctest --test-dir build/pr-check --output-on-failure -R 'discovery|browse|write|session|diagnostics'`, binding evidence to quickstart path 3.
- [X] T043 Validate profile gating with `bash scripts/test_profile_gating.sh`, verifying nano includes only `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, and `opc_cu_3530` from the in-scope set while optional CUs remain off unless enabled, binding evidence to `SCN-001`, `CASE-010`, and quickstart path 4.
- [X] T044 Run standard local quality gates with `git diff --check`, `cmake --build build/pr-check --target format-check`, `cmake --build build/pr-check`, and `ctest --test-dir build/pr-check --output-on-failure`, binding evidence to quickstart path 5.
- [X] T045 Measure default nano size with `scripts/measure_size.sh nano` and record flash/RAM deltas in `docs/size/feature-size-ledger.md` and `specs/071-nano-service-behaviour/cu-audit.md`, preserving the 0.5-2 KB expected growth target.

## Phase 7: Final Review and Cleanup

- [X] T046 Perform boundary review in `specs/071-nano-service-behaviour/cu-audit.md` confirming implementation stayed within the planned M+U scope, did not modify `spec.md`, `contracts/`, `research.md`, `quickstart.md`, or readiness checklists to force a pass, and cites OPC-10000 sections for every protocol-visible change.
- [X] T047 Perform interface-contract review against `specs/071-nano-service-behaviour/contracts/service-contracts.md`, `specs/071-nano-service-behaviour/contracts/sequences.md`, `profiles/opcua-profile-manifest.yaml`, `Kconfig`, `tests/conformance/claim_test_map.md`, and `docs/conformance/`, confirming each claimed CU has a dedicated symbol, implemented state, profile defaults, backing tests, generated artifacts, and validation evidence.
- [X] T048 Perform data-side-effect review in `specs/071-nano-service-behaviour/cu-audit.md` for `src/cu/core_2022_server/attribute_write/write.c`, `src/core/service_dispatch/activate_session.c`, `src/services/session.c`, `src/cu/core_2022_server/diagnostics/diagnostics.c`, and `src/address_space/base_nodes.c`, covering value/status/timestamp/index updates, user identity and nonce changes, diagnostics counters, rollback/no-change failure paths, and disabled-CU invariants.
- [X] T049 Document any intentionally unclaimed in-scope CU with exact missing behaviour and follow-up in `specs/071-nano-service-behaviour/cu-audit.md` and `.specify/memory/roadmap.md`; if all ten are claimed, document that no deferred CU remains.

## Dependencies

```text
Phase 1 setup/audit (T001-T005) must complete before user-story implementation.
Phase 2 foundational tasks (T006-T009) must complete before final claim promotion.
US1 MVP (T010-T020) depends on T001-T009 and can be delivered independently.
US1 claim repair T050-T053 depends on T018-T020 and must complete before T030, T040, or final claim review.
US2 (T021-T031) depends on T001-T009; T054-T057 depend on T021-T029 and must complete before T030/T031.
US3 (T032-T039) depends on T001-T009; T058-T059 depend on T032-T037 and must complete before T038/T039.
Cross-story validation (T040-T045) depends on all claimed-user-story manifest changes.
Final review (T046-T049) depends on validation evidence from T040-T045.
```

## Parallel Opportunities

```text
T002, T003, and T004 can run in parallel because they audit different service families.
T006, T007, T008, and T009 can run in parallel after T001 creates the audit ledger.
Within US1, T010-T013 can run in parallel as test tasks before T014-T017 implementation tasks.
Within US2, T021-T025 can run in parallel as test tasks before T026-T029 implementation tasks.
Within US3, T032-T034 can run in parallel as test tasks before T035-T037 implementation tasks.
T050-T053 and T054-T059 modify shared build/dispatch surfaces and must run sequentially, one complete atomic task per subagent assignment.
```

## Implementation Strategy

**MVP**: Complete T001-T020 plus T050-T053 to claim only the nano-default Discovery and View CUs (`opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, `opc_cu_3530`) with independently effective symbols, generated artifacts, and focused Discovery/View/profile-gating validation.

**Increment 2**: Complete the non-deferred T021-T031 work plus T054-T057 to claim independently gated `opc_cu_2389`, `opc_cu_2400`, and `opc_cu_2936` while preserving nano default false; leave T023/T028 unchecked and `opc_cu_3147` unclaimed under approved deferral `S071-D1`.

**Increment 3**: Complete T032-T039 plus T058-T059 to claim independently gated optional Diagnostics CUs while preserving nano default false.

**Full Validation**: Complete T040-T049 before considering implementation done.
