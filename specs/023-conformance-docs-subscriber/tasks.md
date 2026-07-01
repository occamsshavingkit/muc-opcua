# Tasks: Conformance Docs and PubSub Subscriber

**Input**: Design documents from `/specs/023-conformance-docs-subscriber/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Tests are mandatory before protocol-visible PubSub decode, StatusCode, subscription/DataChange, or conformance-claim behavior changes. Documentation-only tasks use documentation/conformance tests.

**Organization**: Tasks are grouped by user story so documentation cleanup, stale-claim guards, scoped PubSub subscriber decode, and negative-path polish can be delivered independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel when files do not overlap and dependencies are complete.
- **[Story]**: Which user story this task belongs to.
- **OPC refs**: Every task has explicit OPC UA references or an explicit N/A rationale.
- **RESOURCE-RISK**: Marks tasks that may increase Flash, RAM, stack, or throughput cost.

## Phase 1: Setup (Shared Evidence Structure)

**Purpose**: Establish the traceability and evidence files used by all stories.

- [X] T001 Create feature traceability skeleton with rows for docs, checks, PubSub subscriber, and negative-path evidence in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.3/§7.2.4.5.4/§7.2.4.5.5; OPC-10000-4 §7.38.2)
- [X] T002 Record the OPC UA MCP query references used for this feature in `docs/traceability/opcua-mcp-queries.md` (OPC refs: N/A - evidence ledger for cited sections)

---

## Phase 2: Foundational (Shared Documentation/Traceability Guards)

**Purpose**: Add failing/protective checks that block stale documentation and uncited behavior before implementation tasks.

**CRITICAL**: User story implementation should not proceed until these shared guards exist.

- [X] T003 Add a stale public-documentation phrase guard for README and Makefile support claims in `tests/unit/test_conformance_docs.c` (OPC refs: OPC-10000-7 §4.2/§4.3)
- [X] T004 Add a stale Query/NodeManagement section-reference guard in `tests/unit/test_conformance_docs.c` for Query Appendix B §B.2.3/§B.2.4 and NodeManagement §5.8 (OPC refs: OPC-10000-4 Appendix B §B.2.3/§B.2.4; OPC-10000-4 §5.8)
- [X] T005 Add aggregate NodeId stale-value guard in `tests/unit/test_conformance_docs.c` rejecting `11565`, `11569`, and `11570` as Average/Minimum/Maximum identifiers (OPC refs: OPC-10000-4 §7.22.4; OPC-10000-13 §4.2.2.4/§4.2.2.9/§4.2.2.10)
- [X] T006a Add feature-file traceability assertions for PubSub subscriber files in `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5; OPC-10000-6 §5.2.2.16)
- [X] T006b Add feature-file traceability assertions for documentation/conformance check files in `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-4 §7.38.2)
- [X] T007 Update `docs/traceability/files-to-sections.md` with planned feature mappings for `include/micro_opcua/pubsub.h`, `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c`, `tests/unit/test_conformance_docs.c`, and `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5; OPC-10000-6 §5.2.2.16; OPC-10000-4 §7.38.2)
- [X] T008 Update `docs/traceability/sections-to-files.md` with the matching section-to-file rows for the feature mappings in T007 (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5; OPC-10000-6 §5.2.2.16; OPC-10000-4 §7.38.2)

**Checkpoint**: Shared conformance/documentation guards exist and can fail before stale docs or uncited protocol changes ship.

---

## Phase 3: User Story 1 - Use Current Documentation Confidently (Priority: P1) [MVP]

**Goal**: Public docs accurately explain current profile support, implemented optional services, PubSub scope, and reproducible size evidence.

**Independent Test**: `test_conformance_docs` rejects stale support phrases while docs review confirms current support and snapshot labeling.

### Tests for User Story 1

- [X] T009 [US1] Run `ctest --test-dir build/conformance-docs -R '^test_conformance_docs$' --output-on-failure` and capture the expected stale-documentation failures from T003-T005 in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - RED documentation-check evidence)

### Implementation for User Story 1

- [X] T010a [US1] Remove the stale "remaining Micro item" wording and replace the README support surface with current profile-targeting language in `README.md` (OPC refs: OPC-10000-7 §4.2/§4.3)
- [X] T010b [US1] Remove the stale Micro profile support comment from `Makefile` and align it with current profile-targeting language (OPC refs: OPC-10000-7 §4.2/§4.3)
- [X] T011 [US1] Replace the stale README "Not implemented yet: History, NodeManagement, and aggregate subscriptions" limitation with current optional-service and PubSub-scope wording in `README.md` (OPC refs: OPC-10000-4 §5.8; OPC-10000-4 §5.11.3/§5.11.5; OPC-10000-4 §7.22.4; OPC-10000-13 §4.2.2.4/§4.2.2.9/§4.2.2.10)
- [X] T012 [US1] Update the PubSub section in `docs/integration-guide.md` from publisher-only/unsupported-subscriber wording to scoped publisher/subscriber wording and list excluded PubSub features (OPC refs: OPC-10000-14 §5.4.2.1/§5.4.2.2/§5.4.6.2.2/§7.3.2.1)
- [X] T013 [US1] Label profile-size numbers in `docs/integration-guide.md` as dated snapshots with reproduction command `scripts/measure_size.sh all` (OPC refs: N/A - size evidence documentation)
- [X] T014a [US1] Update the PubSub entries in `docs/api-reference.md` to match the scoped subscriber contract (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5)
- [X] T014b [US1] Update optional service entries in `docs/api-reference.md` to match current feature flags for NodeManagement and Query (OPC refs: OPC-10000-4 §5.8; OPC-10000-4 Appendix B §B.2.3/§B.2.4)
- [X] T015 [US1] Update the vague Query service section row in `docs/conformance/services.md` to OPC-10000-4 Appendix B §B.2.3/§B.2.4 (OPC refs: OPC-10000-4 Appendix B §B.2.3/§B.2.4)
- [X] T016 [US1] Update `docs/conformance/services.md` with scoped PubSub publisher/subscriber status and explicit no-profile-compliance language (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-14 §5.4.2.1/§5.4.2.2/§7.3.2.1)
- [X] T017 [US1] Update `docs/traceability/023-conformance-docs-subscriber.md` with completed documentation evidence rows for T010a-T016 (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-4 §5.8; OPC-10000-4 §5.11.3/§5.11.5; OPC-10000-4 Appendix B §B.2.3/§B.2.4; OPC-10000-4 §7.22.4; OPC-10000-13 §4.2.2.4/§4.2.2.9/§4.2.2.10; OPC-10000-14 §5.4.2.1/§5.4.2.2/§5.4.6.2.2/§7.3.2.1)
- [X] T018 [US1] Re-run `test_conformance_docs` and record the passing documentation cleanup evidence in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - validation evidence)

**Checkpoint**: User-facing and implementor docs no longer contain known stale support or numeric claims.

---

## Phase 4: User Story 2 - Catch Stale Claims Before Release (Priority: P1)

**Goal**: In-repo checks detect stale claims, stale numeric constants, and stale section references before release.

**Independent Test**: `test_conformance_docs` and `test_traceability_docs` fail for the stale cases listed in `contracts/documentation-evidence.md`.

### Tests for User Story 2

- [X] T019 [US2] Add required StatusCode documentation guards for `Bad_NotSupported`, `Bad_InvalidArgument`, and `Bad_TooManyOperations` in `tests/unit/test_conformance_docs.c` (OPC refs: OPC-10000-4 §7.38.2)
- [X] T020 [US2] Add documentation guard that measured size/benchmark numbers in public docs are snapshot-labeled or linked to reproduction commands in `tests/unit/test_conformance_docs.c` (OPC refs: N/A - resource evidence guard)
- [X] T021 [P] [US2] Add a traceability assertion that `docs/traceability/023-conformance-docs-subscriber.md` contains evidence rows for each feature story in `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-14 §7.2.4.4.2; OPC-10000-4 §7.38.2)

### Implementation for User Story 2

- [X] T022 [US2] Update `docs/conformance/status.md` to include `Bad_NotSupported`, `Bad_InvalidArgument`, and `Bad_TooManyOperations` in the feature StatusCode tables (OPC refs: OPC-10000-4 §7.38.2)
- [X] T023 [US2] Update `docs/traceability/conformance-claims.md` with the new stale-claim and stale-number guard coverage (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-4 §7.38.2)
- [X] T024 [US2] Re-run `ctest --test-dir build/conformance-docs -R '^(test_conformance_docs|test_traceability_docs)$' --output-on-failure` and record passing guard evidence in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - validation evidence)

**Checkpoint**: In-repo tests catch stale standard claims and stale numbers without external CTT.

---

## Phase 5: User Story 3 - Receive Scoped UADP NetworkMessages (Priority: P2)

**Goal**: Applications can decode the UADP/UDP subset emitted by the existing publisher with caller-provided output storage.

**Independent Test**: `test_uadp_encoding` proves existing publisher bytes decode successfully and malformed/unsupported UADP bytes return cited StatusCodes.

### Tests for User Story 3

- [X] T025 [US3] [RESOURCE-RISK: throughput] Add RED test decoding the current publisher-generated UADP payload in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.3/§7.2.4.5.4/§7.2.4.5.5; OPC-10000-6 §5.2.2.16)
- [X] T026 [US3] [RESOURCE-RISK: throughput] Add RED invalid-argument decoder tests for null input, null decoded-message pointer, and null field output pointer with nonzero field capacity in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-4 §7.38.2)
- [X] T027 [US3] [RESOURCE-RISK: throughput] Add RED malformed UADP decoder tests for truncated header, truncated payload header, and DataSetMessage size mismatch in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.3; OPC-10000-4 §7.38.2)
- [X] T028 [US3] [RESOURCE-RISK: throughput] Add RED output-capacity decoder test for more decoded fields than caller slots in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.5.5; OPC-10000-4 §7.38.2)
- [X] T029a [US3] [RESOURCE-RISK: throughput] Add RED unsupported-layout decoder test for unsupported PublisherId type in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.4.2; OPC-10000-4 §7.38.2)
- [X] T029b [US3] [RESOURCE-RISK: throughput] Add RED unsupported-layout decoder test for UADP security header in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.4.3.1; OPC-10000-4 §7.38.2)
- [X] T029c [US3] [RESOURCE-RISK: throughput] Add RED unsupported-layout decoder test for multiple DataSetMessages in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.5.2; OPC-10000-4 §7.38.2)
- [X] T029d [US3] [RESOURCE-RISK: throughput] Add RED unsupported-layout decoder test for unsupported DataSetMessage type in `tests/unit/test_uadp_encoding.c` (OPC refs: OPC-10000-14 §7.2.4.5.4/§7.2.4.5.6/§7.2.4.5.7; OPC-10000-4 §7.38.2)

### Implementation for User Story 3

- [X] T030 [US3] [RESOURCE-RISK: flash/RAM/throughput] Add scoped subscriber decode result types and API declaration in `include/micro_opcua/pubsub.h` (OPC refs: OPC-10000-14 §5.4.2.1/§5.4.2.2/§7.2.4.4.2; OPC-10000-6 §5.2.2.16)
- [X] T031a [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the UADP version/type flags and UInt32 PublisherId decode path in `src/encoding/uadp_encoder.c` (OPC refs: OPC-10000-14 §7.2.4.4.2)
- [X] T031b [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the PayloadHeader count and DataSetWriterId decode path in `src/encoding/uadp_encoder.c` (OPC refs: OPC-10000-14 §7.2.4.5.2)
- [X] T032a [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement DataSet payload size and Data Key Frame header validation in `src/encoding/uadp_encoder.c` (OPC refs: OPC-10000-14 §7.2.4.5.3/§7.2.4.5.4/§7.2.4.5.5)
- [X] T032b [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement scalar Variant field decode loop in `src/encoding/uadp_encoder.c` (OPC refs: OPC-10000-14 §7.2.4.5.5; OPC-10000-6 §5.2.2.16)
- [X] T033 [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement deterministic `Bad_DecodingError`, `Bad_InvalidArgument`, `Bad_TooManyOperations`, and `Bad_NotSupported` returns in `src/encoding/uadp_encoder.c` (OPC refs: OPC-10000-4 §7.38.2)
- [X] T034 [US3] Update PubSub subscriber API documentation in `docs/api-reference.md` with accepted layout, rejected layout, and StatusCodes (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5; OPC-10000-4 §7.38.2)
- [X] T035 [US3] Update scoped PubSub integration example in `docs/integration-guide.md` to show caller-provided decode output slots (OPC refs: OPC-10000-14 §5.4.2.1/§5.4.2.2/§7.3.2.1)
- [X] T036 [US3] Update `docs/traceability/012-opcua-pubsub.md` with scoped subscriber support and explicit excluded PubSub features (OPC refs: OPC-10000-7 §4.2/§4.3; OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5)
- [X] T037a [US3] [RESOURCE-RISK: flash/RAM/throughput] Measure PubSub subscriber resource impact with `scripts/measure_size.sh all` (OPC refs: N/A - size measurement)
- [X] T037b [US3] Record PubSub subscriber resource impact from T037a in `docs/size/feature-size-ledger.md` (OPC refs: N/A - size evidence documentation)
- [X] T038 [US3] Run `test_uadp_encoding` and `test_pubsub` and record passing PubSub subscriber evidence in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: OPC-10000-14 §7.2.4.4.2/§7.2.4.5.2/§7.2.4.5.5; OPC-10000-6 §5.2.2.16)

**Checkpoint**: Scoped UADP subscriber decode is independently usable and resource impact is recorded.

---

## Phase 6: User Story 4 - Preserve Profile-Relevant Negative Paths (Priority: P3)

**Goal**: Selected Subscription/MonitoredItem/DataChange/status negative paths remain cited, tested, and documented without expanding feature scope.

**Independent Test**: Existing and added checks show the selected negative paths return exact StatusCodes and are traceable to OPC UA sections.

### Tests for User Story 4

- [X] T039a [US4] Add traceability-doc assertion for invalid SubscriptionId and MonitoredItemId evidence rows in `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-4 §5.13.3/§5.13.6/§5.14.3/§5.14.8/§7.38.2)
- [X] T039b [US4] Add traceability-doc assertion for Publish acknowledgement and Republish invalid sequence evidence rows in `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-4 §5.14.5.4/§5.14.6.3/§7.38.2)
- [X] T039c [US4] Add traceability-doc assertion for DataChangeFilter and AggregateFilter evidence rows in `tests/unit/test_traceability_docs.c` (OPC refs: OPC-10000-4 §5.13.2/§5.13.3/§7.22.2/§7.22.4/§7.38.2)
- [X] T040 [P] [US4] Add conformance-doc assertion that `docs/conformance/services.md` names the tests covering selected Subscription/DataChange/Aggregate negative paths in `tests/unit/test_conformance_docs.c` (OPC refs: OPC-10000-4 §5.13.2/§5.13.3/§5.13.6/§5.14.3/§5.14.5.4/§5.14.6.3/§5.14.8/§7.22.2/§7.22.4/§7.38.2)

### Implementation for User Story 4

- [X] T041a [US4] Update `docs/conformance/services.md` with invalid MonitoredItemId and invalid SubscriptionId evidence rows (OPC refs: OPC-10000-4 §5.13.3/§5.13.6/§5.14.3/§5.14.8/§7.38.2)
- [X] T041b [US4] Update `docs/conformance/services.md` with Publish acknowledgement and Republish invalid sequence evidence rows (OPC refs: OPC-10000-4 §5.14.5.4/§5.14.6.3/§7.38.2)
- [X] T041c [US4] Update `docs/conformance/services.md` with unsupported percent deadband, malformed filter, and unsupported aggregate evidence rows (OPC refs: OPC-10000-4 §5.13.2/§5.13.3/§7.22.2/§7.22.4/§7.38.2)
- [X] T042 [US4] Update `docs/traceability/023-conformance-docs-subscriber.md` with Subscription/DataChange/Aggregate negative-path evidence and current test names (OPC refs: OPC-10000-4 §5.13.2/§5.13.3/§5.13.6/§5.14.3/§5.14.5.4/§5.14.6.3/§5.14.8/§7.22.2/§7.22.4/§7.38.2)
- [X] T043 [US4] Run `ctest --test-dir build/conformance-docs -R '^(test_subscriptions_errors|test_aggregate|test_subscriptions|test_conformance_docs|test_traceability_docs)$' --output-on-failure` and record passing evidence in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - validation evidence)

**Checkpoint**: Profile-relevant negative paths are documented and checked as implemented subset evidence.

---

## Phase 7: Compliance, Size, and Polish

**Purpose**: Validate the full feature before completion.

- [X] T044 Run `cmake -S . -B build/conformance-docs -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_PUBSUB=ON -DMICRO_OPCUA_SUBSCRIPTIONS_STANDARD=ON` and build changed tests (OPC refs: N/A - build validation)
- [X] T045 Run `ctest --test-dir build/conformance-docs --output-on-failure` and record summary in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - host validation)
- [X] T046 Run `make all-profiles` and record profile-build result in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - profile build validation)
- [X] T047 Run `scripts/measure_size.sh all` after implementation and compare against the plan budget in `docs/size/feature-size-ledger.md` (OPC refs: N/A - size validation)
- [X] T048 Run CMake targets `format-check`, `cppcheck`, and `clang-tidy` from the validation build and record the result in `docs/traceability/023-conformance-docs-subscriber.md` (OPC refs: N/A - toolchain validation)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup completion and blocks all user stories.
- **US1 and US2**: Depend on Foundational; US1 documentation cleanup should precede final US2 passing guard validation.
- **US3**: Depends on Foundational and can begin after PubSub traceability mappings exist.
- **US4**: Depends on Foundational and can run after current negative-path tests are identified.
- **Compliance/Polish**: Depends on all selected user stories.

### Within Each User Story

- Tests/guards before documentation or protocol implementation.
- Public PubSub API declaration before decoder implementation.
- Decoder header/payload parsing before Variant field parsing.
- Traceability and size records before checkpoint completion.

### Parallel Opportunities

- T006a-T006b touch `test_traceability_docs.c`; edit them sequentially after or alongside the `test_conformance_docs.c` guard work.
- T025-T029d are same-file RED tests and must be edited sequentially or coordinated in one patch before implementation.
- T034-T036 can proceed after T030-T033 because documentation files are independent.
- T040 can proceed after or alongside the T039 series because it touches `test_conformance_docs.c` instead of `test_traceability_docs.c`.

## Implementation Strategy

1. Complete Setup and Foundational guards first.
2. Deliver MVP documentation cleanup (US1) and stale-claim guards (US2).
3. Implement the scoped UADP subscriber (US3) using RED tests from T025-T029.
4. Add negative-path evidence checks (US4) without expanding supported standard surface.
5. Run full validation and record resource impact.

## Requirement Coverage

| Requirement | Covering Tasks |
|---|---|
| FR-001 current user/implementor documentation | T003, T010a, T010b, T011, T012, T014b, T015, T016, T018 |
| FR-002 numeric values are current or snapshot-labeled | T013, T020, T037a, T037b, T047 |
| FR-003 unsupported profile-compliant/CTT claims rejected | T003, T023, T024 |
| FR-004 StatusCode numeric/name guards | T019, T022, T024 |
| FR-005 NodeId and section-reference guards | T004, T005, T015, T023, T024 |
| FR-006 scoped UADP subscriber decodes publisher subset | T025, T030, T031a, T031b, T032a, T032b, T038 |
| FR-007 caller-provided storage and no heap | T030, T031a, T031b, T032a, T032b, T037a, T037b |
| FR-008 deterministic decoder failure statuses | T026, T027, T028, T029a, T029b, T029c, T029d, T033 |
| FR-009 PubSub subscriber documentation and exclusions | T012, T014a, T016, T034, T035, T036 |
| FR-010 subscription/DataChange/status polish | T039a, T039b, T039c, T040, T041a, T041b, T041c, T042, T043 |
| FR-011 traceability for changed standard-affecting files | T001, T006a, T006b, T007, T008, T017, T021, T036, T038, T042 |
| FR-012 final validation | T044, T045, T046, T047, T048 |
| SC-001 zero known stale public support statements | T003, T009, T010a, T010b, T011, T012, T014a, T014b, T015, T016, T018 |
| SC-002 stale claim/status/NodeId/section tests fail | T003, T004, T005, T019, T020, T021, T022, T023, T024 |
| SC-003 publisher fixture decodes successfully | T025, T030, T031a, T031b, T032a, T032b, T038 |
| SC-004 malformed/unsupported UADP tests cover required failures | T026, T027, T028, T029a, T029b, T029c, T029d, T033, T038 |
| SC-005 negative-path StatusCode evidence | T039a, T039b, T039c, T040, T041a, T041b, T041c, T042, T043 |
| SC-006 no heap and size budget recorded | T030, T031a, T031b, T032a, T032b, T037a, T037b, T047 |
| SC-007 traceability checks map changed files | T006a, T006b, T007, T008, T017, T021, T036, T038, T042, T045 |

## Notes

- Tasks marked `RESOURCE-RISK` require extra review for flash, RAM, stack, and throughput impact.
- Protocol tasks cite exact OPC UA sections for re-reference during implementation.
- Unsupported PubSub features must fail explicitly and remain documented as out of scope.
