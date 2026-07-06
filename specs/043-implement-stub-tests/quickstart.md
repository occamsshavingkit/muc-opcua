# Quickstart: Implement Placeholder Stub Tests

**Feature**: 043-implement-stub-tests

## Verify Current State

```bash
# All 5 stub tests should have STUB markers
grep -rn "STUB:" tests/unit/test_aggregate_full.c tests/unit/test_audit_events.c \
  tests/unit/test_complex_types.c tests/integration/test_minimal_server_flow.c \
  tests/integration/test_discovery_endpoint_no_session.c
```

## Build and Baseline

```bash
cd build && cmake .. -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PROFILE=full
cmake --build . -j$(nproc)
ctest --output-on-failure
# Expected: 124 tests pass. Stub tests pass via TEST_PASS_MESSAGE placeholders.
```

## Implementation Order

1. **T001**: `test_minimal_server_flow.c` — simplest integration test. Copy the
   mock transport pattern from `test_server_handshake.c`, implement the full
   lifecycle. Verify with `ctest -R test_minimal_server_flow`.

2. **T002**: `test_discovery_endpoint_no_session.c` — similar to T001 but tests
   GetEndpoints and FindServers without a session. Use existing
   `test_discovery_endpoint.c` as reference.

3. **T003**: `test_aggregate_full.c` — write edge-case tests for the 3 existing
   aggregate functions (Average, Min, Max). Use the existing
   `test_aggregate.c` as reference. Include
   `src/core/server_internal.h` for direct function access.

4. **T004**: `test_audit_events.c` — write minimal tests for the current no-op
   implementation. Test that `mu_raise_audit_event` doesn't crash with valid
   inputs and NULL inputs.

5. **T005**: `test_complex_types.c` — update stub documentation to reflect
   what's tested (registration API, type definitions) vs. what's deferred
   (encode/decode round-trips pending encoder implementation).

## Verify Completion

```bash
# No STUB markers remain
grep -rn "STUB:" tests/unit/test_aggregate_full.c tests/unit/test_audit_events.c \
  tests/unit/test_complex_types.c tests/integration/test_minimal_server_flow.c \
  tests/integration/test_discovery_endpoint_no_session.c
# Expected: no output

# All tests pass on full profile
cd build && ctest --output-on-failure
# Expected: 124+ tests pass (new tests may add a few)

# All profiles pass
# micro: cd build_micro && cmake .. -DMUC_OPCUA_PROFILE=micro ...
# standard: cd build_std && cmake .. -DMUC_OPCUA_PROFILE=standard ...
```
