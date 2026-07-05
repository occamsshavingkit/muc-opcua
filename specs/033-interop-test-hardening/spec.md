# Feature Specification: Interop Test Hardening

**Feature Branch**: `033-interop-test-hardening`  
**Created**: 2026-07-04  
**Status**: Draft  
**Input**: "Audit all encoder and interop tests for complete field coverage. Add write interop test. Ensure no silent failures — every binary response reader must verify position == expected_length."

## User Scenarios & Testing

### User Story 1 — Audit every response encoder test for complete field coverage (P1)

Every `*_encode` unit test must read every mandatory field from the encoded response
buffer. The WriteResponse diagnosticInfos bug was caused by an encoder missing a
mandatory field AND a test that stopped reading before reaching that field. No more
silent gaps.

**Independent Test**: Every response encoder test has `reader->position == expected`
at the end, or reads all fields explicitly. No test reads only the first N fields and
stops.

**Acceptance Scenarios**:
1. Each `*_response_encode` test reads all fields through to the end of the encoded buffer.
2. No `*_response_encode` test returns early after reading only a subset of fields.

---

### User Story 2 — Add write interop test against real OPC UA client (P1)

The interop smoke test has zero write coverage. The missing diagnosticInfos went
undetected because no real client ever exercised the Write path end-to-end. Add a
write test using a Python OPC UA client library.

**Independent Test**: Interop test issues a WriteRequest, server responds with a
well-formed WriteResponse that the client can decode without error.

**Acceptance Scenarios**:
1. Interop test connects to muc-opcua server, sends write request for a variable, client receives valid WriteResponse.
2. Client decodes the response without errors (diagnosticInfos present).

---

### User Story 3 — Verify interop tests cover all supported services (P2)

The interop smoke test should exercise every service set the server claims to support:
Read, Write, Browse, Subscribe, Publish, CreateSession, ActivateSession. Missing
coverage is a regression risk.

**Independent Test**: Each service set has at least one interop test that sends a
real request and validates the response.

**Acceptance Scenarios**:
1. Write interop test exists and passes.
2. Read, Browse, Subscribe, Publish, Session interop tests exist and pass.

---

### Edge Cases

- **Malformed response handling**: Interop tests must detect when server returns
  truncated or malformed responses (client decoder throws exception).
- **Empty/null arrays**: Tests must verify null/empty arrays are encoded correctly
  (Int32 0 or -1), not silently omitted.

## Requirements

### Functional Requirements

- **FR-001**: Every `*_response_encode` unit test MUST read all mandatory fields of the encoded response through to completion.
- **FR-002**: Every `*_response_encode` unit test MUST verify `reader->position == w.position` (or equivalent) after reading all fields.
- **FR-003**: An interop test for Write service MUST exist that sends a WriteRequest and validates the WriteResponse.
- **FR-004**: The interop write test MUST use a real OPC UA client library (asyncua or opcua-asyncio) capable of decoding WriteResponse.
- **FR-005**: All existing interop tests MUST continue to pass.
- **FR-006**: All 108 existing unit and integration tests MUST continue to pass.

### Scope Boundaries

- **In Scope**: Write diagnosticInfos already fixed. Audit all response encoder tests. Add write interop test. Verify existing interop coverage.
- **Out of Scope**: Fixing encoder bugs beyond diagnosticInfos (report any found). Performance benchmarking. New service implementations.

## Success Criteria

- **SC-001**: Every `*_response_encode` test reads all fields to end-of-buffer.
- **SC-002**: Write interop test passes against a running muc-opcua server.
- **SC-003**: 108/108 tests pass with zero failures.
- **SC-004**: No encoder test has a "reads only first N fields" pattern.

## Assumptions

- `asyncua` or `opcua-asyncio` is available as a Python dependency for interop tests.
- All response encoder bugs beyond diagnosticInfos have already been fixed in prior PRs.
