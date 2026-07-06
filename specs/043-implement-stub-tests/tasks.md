# Tasks: Implement Placeholder Stub Tests

**Feature**: 043-implement-stub-tests
**Input**: Design documents from `specs/043-implement-stub-tests/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US4, US5)
- Include exact file paths in descriptions
- Include OPC UA part/section references

## Phase 1: Setup

**Purpose**: Verify baseline and identify all STUB markers

- [ ] T001 Verify baseline in `build/` — `ctest --output-on-failure` all pass on full profile
- [ ] T002 [P] Verify baseline in `build_micro/` — `ctest --output-on-failure` all pass
- [ ] T003 [P] Verify baseline in `build_standard/` — `ctest --output-on-failure` all pass
- [ ] T004 [P] Catalog all `/* STUB:` markers with: `grep -rn "STUB:" tests/unit/test_aggregate_full.c tests/unit/test_audit_events.c tests/unit/test_complex_types.c tests/integration/test_minimal_server_flow.c tests/integration/test_discovery_endpoint_no_session.c`

---

## Phase 2: User Story 4 — Minimal Server Flow Integration Test (Priority: P3)

**Goal**: Replace `tests/integration/test_minimal_server_flow.c` stub with real
full-lifecycle integration test (OPC-10000-6 §7.1 / OPC-10000-4 §5.6).

**Independent Test**: `ctest -R test_minimal_server_flow` — full lifecycle
completes with expected StatusCodes.

### Implementation for User Story 4

- [ ] T005 [US4] Implement shared mock transport helpers in `tests/support/mock_transport.h`: `mock_t` struct with inbound queue and `last_write` capture, `mock_accept`, `mock_read`, `mock_write`, `mock_close`, `mock_shutdown` callbacks, `enqueue()` helper — copy pattern from `tests/integration/test_server_handshake.c` (OPC-10000-6 §7.1.2 OPC UA TCP transport, §7.1.2.3 Hello, §7.1.2.4 Acknowledge)
- [ ] T006 [US4] Implement wire-format helper `build_msg()` and `parse_response()` in `tests/support/mock_transport.h`: encode MSG framing (magic `MSGF` + size + channel_id + security header + body) and decode ResponseHeader + service type from server output (OPC-10000-6 §6.7.2 MessageChunk structure, §7.1.2 OPC UA TCP)
- [ ] T007 [US4] Implement HEL→ACK handshake test in `tests/integration/test_minimal_server_flow.c`: enqueue HELLO chunk (4-byte magic `HELF` + version + buffer sizes + endpoint URL) → `mu_server_poll` → verify ACK response with correct buffer sizes (OPC-10000-6 §7.1.2.3 Hello, §7.1.2.4 Acknowledge)
- [ ] T008 [US4] Implement OpenSecureChannel test in `tests/integration/test_minimal_server_flow.c`: enqueue OPN chunk → `mu_server_poll` → verify OpenSecureChannelResponse has valid SecureChannelId, SecurityToken, and token lifetime → read channel ID with `read_opn_channel_id()` → patch channel IDs into queued MSG/CLO chunks with `patch_msg_channel_ids()` (OPC-10000-4 §5.5.2 OpenSecureChannel, OPC-10000-6 §6.7.2 MessageChunk, §7.1.2.4 OPN chunk framing)
- [ ] T009 [US4] Implement CreateSession test in `tests/integration/test_minimal_server_flow.c`: build CreateSessionRequest body (client description, nonce, cert, timeout) → enqueue as MSG → `mu_server_poll` → verify CreateSessionResponse contains valid SessionId, nonce, and revisedLifetime (OPC-10000-4 §5.6.2 CreateSession)
- [ ] T010 [US4] Implement ActivateSession test in `tests/integration/test_minimal_server_flow.c`: build ActivateSessionRequest body (session auth token, client signature, client nonce) → enqueue as MSG → `mu_server_poll` → verify ActivateSessionResponse is Good and server nonce is returned (OPC-10000-4 §5.7.2 ActivateSession)
- [ ] T011 [US4] Implement Read test in `tests/integration/test_minimal_server_flow.c`: build ReadRequest body (read ServerStatus node) → enqueue as MSG → `mu_server_poll` → verify ReadResponse contains ServerStatus DataValue with a recognized value (OPC-10000-4 §5.11.2 Read)
- [ ] T012 [US4] Implement CloseSession test in `tests/integration/test_minimal_server_flow.c`: build CloseSessionRequest body (delete subscriptions=false) → enqueue as MSG → `mu_server_poll` → verify CloseSessionResponse is Good (OPC-10000-4 §5.6.3.2 CloseSession)
- [ ] T013 [US4] Remove `TEST_IGNORE_MESSAGE("Implement minimal server flow integration test")` from `tests/integration/test_minimal_server_flow.c` — all tests now exercise real code paths
- [ ] T014 [US4] Build and test: `ctest -R test_minimal_server_flow` passes on full, micro, standard profiles

---

## Phase 3: User Story 5 — Discovery Without Session Integration Test (Priority: P3)

**Goal**: Replace `tests/integration/test_discovery_endpoint_no_session.c` stub
with real discovery integration test (OPC-10000-4 §5.5.4).

**Independent Test**: `ctest -R test_discovery_endpoint_no_session` — GetEndpoints
and FindServers return valid results without active session.

### Implementation for User Story 5

- [ ] T015 [US5] Include `tests/support/mock_transport.h` (from T005-T006) in `tests/integration/test_discovery_endpoint_no_session.c` — reuse shared mock transport helpers (OPC-10000-6 §7.1.2 OPC UA TCP transport)
- [ ] T016 [US5] Implement GetEndpoints without session test in `tests/integration/test_discovery_endpoint_no_session.c`: enqueue OPN → `mu_server_poll` → extract channel ID → patch channel IDs → build GetEndpointsRequest body → enqueue as MSG → `mu_server_poll` → verify GetEndpointsResponse contains endpoint descriptions with SecurityPolicy URI and TransportProfile URI (OPC-10000-4 §5.5.4.2 GetEndpoints)
- [ ] T017 [US5] Implement FindServers without session test in `tests/integration/test_discovery_endpoint_no_session.c`: build FindServersRequest body (no filter) → enqueue as MSG → `mu_server_poll` → verify FindServersResponse lists at least one server with correct ApplicationName and ApplicationURI (OPC-10000-4 §5.5.4.3 FindServers)
- [ ] T018 [US5] Implement FindServers filtered by ApplicationURI test in `tests/integration/test_discovery_endpoint_no_session.c`: build FindServersRequest body with `server_uris` filter matching server URI → enqueue as MSG → `mu_server_poll` → verify only matching servers returned (OPC-10000-4 §5.5.4.3 FindServers)
- [ ] T019 [US5] Implement `MU_DISCOVERY_MAX_ENDPOINTS` limit test in `tests/integration/test_discovery_endpoint_no_session.c`: configure server with N endpoints → verify GetEndpointsResponse count ≤ `MU_DISCOVERY_MAX_ENDPOINTS` (OPC-10000-4 §5.5.4 Discovery)
- [ ] T020 [US5] Remove `TEST_IGNORE_MESSAGE("Implement integration test for discovery without session")` from `tests/integration/test_discovery_endpoint_no_session.c`
- [ ] T021 [US5] Build and test: `ctest -R test_discovery_endpoint_no_session` passes on full, micro, standard profiles

---

## Phase 4: User Story 1 — Aggregate Function Edge-Case Tests (Priority: P1)

**Goal**: Replace `tests/unit/test_aggregate_full.c` stub with edge-case tests
for the 3 existing aggregate functions (Average, Minimum, Maximum). Direct API
calls via `src/core/server_internal.h`, not full pipeline (OPC-10000-13).

**Independent Test**: `ctest -R test_aggregate_full` — each aggregate function
tested with known-input/expected-output vectors.

### Implementation for User Story 1

- [ ] T022 [US1] Add `monitored_item_accumulate_aggregate` / `monitored_item_publish_aggregate` direct API test for Average aggregate in `tests/unit/test_aggregate_full.c`: populate `mu_monitored_item_t` with `aggregate_type=MU_ID_AGGREGATETYPE_AVERAGE`, accumulate known samples, publish, verify result is `sum/sample_count` as double (OPC-10000-13)
- [ ] T023 [US1] Add Minimum aggregate edge-case test in `tests/unit/test_aggregate_full.c`: accumulate sequence [5.0, 2.0, 3.0] — verify published minimum is 2.0 (OPC-10000-13)
- [ ] T024 [US1] Add Maximum aggregate edge-case test in `tests/unit/test_aggregate_full.c`: accumulate sequence [1.0, 4.0, 2.0] — verify published maximum is 4.0 (OPC-10000-13)
- [ ] T025 [US1] Add zero-sample fallback test in `tests/unit/test_aggregate_full.c`: call `monitored_item_publish_aggregate` with `sample_count=0` and `has_value=false` — verify `MU_STATUS_BAD_NODATA` published; with `has_value=true` — verify `last_value` published unchanged (OPC-10000-13)
- [ ] T026 [US1] Add non-numeric sample skip test in `tests/unit/test_aggregate_full.c`: accumulate a non-numeric variant (string type) — verify `sample_count` is NOT incremented (OPC-10000-13)
- [ ] T027 [US1] Add processing-interval boundary test in `tests/unit/test_aggregate_full.c`: set `processing_interval=100`, accumulate, attempt publish at t=50 — verify NOT published; advance to t=150 — verify IS published (OPC-10000-13)
- [ ] T028 [US1] Add integer type preservation test in `tests/unit/test_aggregate_full.c`: accumulate INT32 values, publish Minimum — verify published variant type is INT32 (not converted to DOUBLE) (OPC-10000-13)
- [ ] T029 [US1] Remove existing `TEST_PASS_MESSAGE` stubs (`test_unsupported_aggregate_returns_filter_unsupported`) and keep existing constant-check tests (`test_aggregate_type_average_is_2342`, etc.) (OPC-10000-13)
- [ ] T030 [US1] Remove `/* STUB: Aggregate function behavioral tests not yet implemented... */` comment block from `tests/unit/test_aggregate_full.c`
- [ ] T031 [US1] Build and test: `ctest -R test_aggregate_full` passes on full, standard profiles (skips via `#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD` on micro)

---

## Phase 5: User Story 2 — Audit Event Tests (Priority: P2)

**Goal**: Replace `tests/unit/test_audit_events.c` stub with tests for current
`mu_raise_audit_event` implementation (no-op with NULL-safety). Callback dispatch
tests deferred until callback mechanism is implemented (OPC-10000-5 §6.5).

**Independent Test**: `ctest -R test_audit_events` — all audit event paths tested.

### Implementation for User Story 2

- [ ] T032 [US2] Add valid-input no-crash test in `tests/unit/test_audit_events.c`: create a server instance, populate a valid `mu_audit_event_t` with `event_type=MU_AUDIT_EVENT_CREATE_SESSION`, call `mu_raise_audit_event(server, &event)`, verify no crash and server remains valid (OPC-10000-5 §6.5)
- [ ] T033 [US2] Add NULL-pointer safety test in `tests/unit/test_audit_events.c`: call `mu_raise_audit_event(NULL, NULL)` and verify no crash — function silently returns (OPC-10000-5 §6.5)
- [ ] T034 [US2] Add auditing_disabled config flag test in `tests/unit/test_audit_events.c`: set `config.auditing_enabled=false` on server, verify flag reads back correctly (OPC-10000-5 §6.5)
- [ ] T035 [US2] Add documentation comment in `tests/unit/test_audit_events.c` listing what is deferred: `mu_audit_callback_t` typedef, callback registration API, callback dispatch from `mu_raise_audit_event`, per-event-type handler routing — pending audit callback implementation
- [ ] T036 [US2] Remove `/* STUB: Audit event dispatch behavioral tests... */` comment block from `tests/unit/test_audit_events.c`
- [ ] T037 [US2] Build and test: `ctest -R test_audit_events` passes on full, standard profiles (skips via `#if MUC_OPCUA_AUDITING` on micro)

---

## Phase 6: User Story 3 — Complex Types Documentation Update (Deferred)

**Goal**: Update `tests/unit/test_complex_types.c` to accurately document what
is tested (registration API, type definitions) vs. what is deferred (encode/
decode round-trips pending encoder implementation). No behavioral test changes.

**Independent Test**: `ctest -R test_complex_types` — existing tests pass, STUB
comment replaced with deferred-work documentation.

### Implementation for User Story 3

- [ ] T038 [US3] Replace `/* STUB: Complex type round-trip encode/decode behavioral tests not yet implemented... */` comment in `tests/unit/test_complex_types.c` with documentation listing: (a) what is tested — registration API (`mu_register_structure_type`, `mu_register_enumeration_type`), type definition field validation (`mu_structure_field_t`, `mu_structure_definition_t`, `mu_enum_field_t`, `mu_enum_definition_t`); (b) what is deferred — encode/decode round-trips pending `mu_binary_encode_struct`/`mu_binary_decode_struct` implementation in `src/encoding/binary_complex.c` (OPC-10000-3 §5.6.4)
- [ ] T039 [US3] Verify existing tests pass: `ctest -R test_complex_types` on full, standard profiles (skips via `#if MUC_OPCUA_COMPLEX_TYPES` on micro)

---

## Phase 7: Polish & Verification

**Purpose**: Cross-cutting validation before completion

- [ ] T040 Run clang-format on all modified files: `tests/support/mock_transport.h`, `tests/unit/test_aggregate_full.c`, `tests/unit/test_audit_events.c`, `tests/unit/test_complex_types.c`, `tests/integration/test_minimal_server_flow.c`, `tests/integration/test_discovery_endpoint_no_session.c`
- [ ] T041 Run cppcheck — zero warnings on `src/` and `tests/`
- [ ] T042 Run `ctest --output-on-failure` on full profile — all tests pass, no `TEST_IGNORE_MESSAGE` placeholders in the 5 target files
- [ ] T043 Run `ctest --output-on-failure` on micro profile — all tests pass
- [ ] T044 Run `ctest --output-on-failure` on standard profile — all tests pass
- [ ] T045 Verify `grep -rn "STUB:"` returns zero matches across the 5 target test files (`tests/unit/test_aggregate_full.c`, `tests/unit/test_audit_events.c`, `tests/unit/test_complex_types.c`, `tests/integration/test_minimal_server_flow.c`, `tests/integration/test_discovery_endpoint_no_session.c`)
- [ ] T046 Update TODO.md — mark STUB1, STUB2, STUB5, STUB6, STUB7 as complete, update STUB3/STUB4/STUB8 status as feature-gated

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — run T001-T004 first
- **Phase 2 (US4)**: Depends on Phase 1. T005 must complete before T006-T014
- **Phase 3 (US5)**: Depends on T005-T006 (shared mock helpers). T015-T021 self-contained otherwise
- **Phase 4 (US1)**: Depends on Phase 1 — unit test, self-contained file
- **Phase 5 (US2)**: Depends on Phase 1 — unit test, self-contained file
- **Phase 6 (US3)**: Depends on Phase 1 — doc-only, self-contained file
- **Phase 7 (Polish)**: Depends on all prior phases

### Parallel Opportunities

- T002, T003, T004 can run in parallel after T001
- Phases 4, 5, 6 can run in parallel after Phase 1 (different files)
- Within US4: T007-T012 can run in parallel after T005-T006
- Within US1: T021-T028 can run in parallel (different test functions)
- Within US2: T032-T034 can run in parallel
- Within US5: T016-T019 can run in parallel after T015

### Suggested Execution Order

```
T001 → T002,T003,T004 (phase 1)
  ├── T005 → T006 → T007,T008,T009,T010,T011,T012 → T013 → T014 (phase 2, US4)
  ├── T015 → T016,T017,T018,T019 → T020 → T021 (phase 3, US5)
  ├── T022,T023,T024,T025,T026,T027,T028 → T029 → T030 → T031 (phase 4, US1)
  ├── T032,T033,T034 → T035 → T036 → T037 (phase 5, US2)
  └── T038 → T039 (phase 6, US3)
T040-T046 (phase 7, polish)
```

---

## Task Summary

| Phase | Story | Count | Description |
|-------|-------|-------|-------------|
| Setup | — | 4 | Baseline verification, stub catalog |
| US4 | P3 | 10 | Minimal server flow (mock helpers + lifecycle tests) |
| US5 | P3 | 7 | Discovery without session (reuses mock helpers) |
| US1 | P1 | 10 | Aggregate edge-case tests (3 functions) |
| US2 | P2 | 6 | Audit event tests (minimal scope) |
| US3 | P2 | 2 | Complex types doc update (deferred) |
| Polish | — | 7 | Cross-cutting validation |
| **Total** | | **46** | |

## Implementation Strategy

**MVP (Minimum Viable)**: US4 alone (T005-T014) — one integration test covering the
most critical OPC UA server path. Delivers a real, non-stub test. Requires shared
mock transport helpers (T005-T006) that US5 also reuses.

**Incremental**: Add US5 next (discovery), then US1 (aggregate edge-cases).
US2 and US3 are small follow-ups.

**Deferred explicitly**: Complex type encode/decode (US3, encoder doesn't exist),
audit callback dispatch (US2, API doesn't exist), 39 additional aggregate
functions (US1, `MUC_OPCUA_AGGREGATE_FULL` not implemented). All documented
in test files as deferred work.
