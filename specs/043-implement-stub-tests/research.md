# Research: Implement Placeholder Stub Tests

**Feature**: 043-implement-stub-tests | **Date**: 2026-07-06

## Decision 1: Aggregate Function Test Scope

**Decision**: `test_aggregate_full` can only test the 3 existing aggregate functions
(Average, Minimum, Maximum). The 39 additional OPC-10000-13 aggregates are not
implemented.

**Rationale**: 
- The directory `src/services/aggregates/` does not exist. Aggregate logic lives
  in `src/services/subscription_aggregate.c` (230 lines).
- Only 3 types are supported: MU_ID_AGGREGATETYPE_AVERAGE (2342),
  MU_ID_AGGREGATETYPE_MINIMUM (2346), MU_ID_AGGREGATETYPE_MAXIMUM (2347).
- The accumulator union only has `avg`, `min`, `max` members.
- The filter validation in `filter_reader.c` hard-rejects non-Average/Min/Max types.
- `test_aggregate.c` (existing, 13 tests) already covers these 3 functions
  through the full OPC UA pipeline (server → subscription → tick → aggregate).
- `MUC_OPCUA_AGGREGATE_FULL` flag exists but no code is gated behind it.

**Alternatives considered**:
- Implement 39 aggregate functions: out of scope, requires feature work not tests.
- Leave `test_aggregate_full` as stub: violates spec goal.
- Write targeted behavioral tests that exercise the 3 existing functions in ways
  that `test_aggregate.c` does not (direct function calls, edge cases, corner
  values): **chosen**.

**Approach for spec 043**: Write 10+ tests in `test_aggregate_full` that
exercise the 3 aggregate functions via the internal API (direct
`monitored_item_accumulate_aggregate` and `monitored_item_publish_aggregate`
calls), covering edge cases not tested by the existing full-pipeline test.

## Decision 2: Audit Event Test Scope

**Decision**: `test_audit_events` can only test the current implementation:
`mu_raise_audit_event` is a no-op that `(void)s` both arguments. No callback
dispatch to test.

**Rationale**:
- `mu_raise_audit_event` in `src/services/audit_events.c` casts both `server`
  and `event` to `(void)`. It does nothing.
- There is no `mu_audit_callback_t` typedef, no callback registration API, no
  callback storage in the server struct.
- The function is never called from anywhere in `src/`.
- The spec says "Out of Scope: Modifying the audit event API or implementation"
  — this is contradictory with testing callback dispatch.

**Alternatives considered**:
- Implement callback mechanism: violates "out of scope" constraint but is the
  only path to meaningful test coverage.
- Leave as stub: violates spec goal.
- Write minimal tests that verify the current behavior (no-op is correct when
  callback doesn't exist): **chosen**, but delivers minimal value.

**Approach for spec 043**: Write minimal tests verifying that
`mu_raise_audit_event` does not crash with valid inputs, does not crash with
NULL arguments, and that the auditing config flag is readable. Defer callback
dispatch tests until the API is implemented.

## Decision 3: Complex Type Test Scope

**Decision**: Complex type round-trip tests cannot be implemented. The encode/
decode functions (`mu_binary_encode_struct`, `mu_binary_decode_struct`) do not
exist.

**Rationale**:
- `src/encoding/binary_complex.c` is a 12-line dead stub: one unused
  `muc_opcua_complex_types_placeholder()` function gated by `MUC_OPCUA_COMPLEX_TYPES`.
- No `mu_binary_encode_struct` or `mu_binary_decode_struct` anywhere in the
  codebase.
- The registration API (`mu_register_structure_type`,
  `mu_register_enumeration_type`) is implemented and the existing stub already
  tests it.
- The spec assumption "The complex type encoder/decoder already supports
  round-trip" is incorrect.

**Alternatives considered**:
- Implement encoder/decoder: out of scope, major feature work.
- Leave as stub: violates spec goal.
- Write tests exercising what exists: the registration API and type definition
  field validation are already tested by the current stub. There is nothing
  additional to test without an encoder. **Defer until encoder exists.**

**Approach for spec 043**: Remove the round-trip test requirement from the spec.
Update `test_complex_types` to clearly document what the stub covers vs. what
is deferred pending encoder implementation.

## Decision 4: Feature Feature Gating

**Decision**: All new tests must gate on the same `MUC_OPCUA_*` feature flags as
the code they test. Tests compile in all profiles but skip at runtime when
features are disabled.

**Rationale**:
- Existing test pattern: `#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD` / `#else` with
  `TEST_PASS_MESSAGE`.
- This allows `test_aggregate_full` to compile in nano/micro profiles (where
  subscriptions are off) and run in standard/full profiles.
- `test_audit_events` gates on `MUC_OPCUA_AUDITING` (which requires
  `MUC_OPCUA_EVENTS`).
- Integration tests (`test_minimal_server_flow`, `test_discovery_endpoint_no_session`)
  compile in all profiles since they use the basic server API.
- `test_complex_types` gates on `MUC_OPCUA_COMPLEX_TYPES`.

## Decision 5: Integration Test Implementation

**Decision**: Both integration tests are fully implementable using existing
mock transport patterns from `test_server_handshake.c`.

**Rationale**:
- Mock TCP adapter pattern is well-established (queue-based `mock_t` with
  `enqueue()` helper).
- Full server lifecycle flow is documented in `test_server_handshake.c`:
  HEL → ACK → OPN → OpenSecureChannelResponse → MSG service chunks →
  CLO channel close.
- Discovery without session is simpler: skip HEL (inject OPN directly) or
  skip HEL+OPN (inject GetEndpoints/FindServers directly through mock).
- No new APIs or implementation changes needed. Tests exercise existing code.

## Decision 6: Spec Revision Required

**Decision**: The specification must be revised to reflect the actual state of
the codebase. Three of five user stories are not implementable as specified.

**Rationale**:
- US1 (aggregate): Only 3 of 42 functions exist. Scope reduced to deep-testing
  those 3.
- US2 (audit events): No callback mechanism. Scope reduced to no-op verification.
- US3 (complex types): No encoder exists. Deferred entirely.
- US4 (server flow): Fully implementable.
- US5 (discovery): Fully implementable.

**Follow-up**: The revised spec should clearly separate what spec 043 delivers
(integration tests + aggregate edge-case tests) from what requires feature
implementation first (full aggregate set, audit callbacks, complex type encoder).
