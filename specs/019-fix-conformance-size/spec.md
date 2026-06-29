# Feature Specification: OPC UA Conformance and Size Repairs

**Feature Branch**: `019-fix-conformance-size`  
**Created**: 2026-06-29  
**Status**: Draft  
**Input**: User description: "Fix OPC UA conformance fidelity defects and embedded size/performance regressions found in the audit: correct StatusCode and NodeId constants, make AggregateFilter and GetEndpoints behavior standard-traceable, improve traceability docs, reduce RAM/ROM hotspots without weakening required protocol correctness, and add tests/measurements for all wire-visible changes."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Correct Wire Identifiers (Priority: P1)

As an OPC UA client or integrator, I need the library to emit and accept the standard StatusCode values, standard ExtensionObject encoding NodeIds, and standard AggregateFunction NodeIds so interoperable clients do not observe implementation-private meanings on the wire.

**Why this priority**: Incorrect numeric identifiers make otherwise functional services non-conformant and can cause real clients to reject responses or request unsupported behavior by accident.

**Independent Test**: Can be tested by validating the library's public constants and aggregate filter decoding against the official OPC Foundation NodeIds and StatusCode tables, without requiring other feature work.

**Acceptance Scenarios**:

1. **Given** an unsupported monitored-item filter, **When** the server returns a StatusCode, **Then** the value is `Bad_MonitoredItemFilterUnsupported` `0x80440000`.
2. **Given** a client sends an `AggregateFilter` ExtensionObject using `AggregateFilter_Encoding_DefaultBinary`, **When** the server decodes the filter, **Then** the accepted encoding NodeId is namespace 0 numeric `730`.
3. **Given** a client requests Average, Minimum, or Maximum aggregate functions, **When** the server validates the aggregate type, **Then** namespace 0 numeric NodeIds `2342`, `2346`, and `2347` are recognized respectively.

---

### User Story 2 - Standard-Traceable Service Behavior (Priority: P1)

As an interoperability tester, I need aggregate monitored-item filters and discovery endpoint filtering to either follow the cited OPC UA sections or fail with the cited StatusCode so conformance claims are auditable.

**Why this priority**: The project constitution requires every implemented protocol behavior and unsupported scope to map to exact OPC UA normative sections.

**Independent Test**: Can be tested by exercising `CreateMonitoredItems` with supported and unsupported aggregate filters and `GetEndpoints` with matching and non-matching transport profile filters.

**Acceptance Scenarios**:

1. **Given** a `CreateMonitoredItems` request with an unsupported aggregate function, **When** the server processes the filter, **Then** the operation result is the cited monitored-item filter StatusCode and no partial aggregate state is installed.
2. **Given** a `GetEndpoints` request with a transport profile URI that does not match the server endpoint, **When** the server responds, **Then** the endpoint array is empty and the service result remains successful.
3. **Given** a `GetEndpoints` request with the server's transport profile URI, **When** the server responds, **Then** the matching endpoint is returned.

---

### User Story 3 - Smaller and Faster Embedded Defaults (Priority: P2)

As an embedded firmware author, I need measurable reductions or configuration controls for the largest ROM, RAM, and stack hotspots so enabling conformance fixes does not consume unnecessary microcontroller headroom.

**Why this priority**: The library targets constrained devices; conformance fixes must not hide growth in flash, static RAM, or stack depth.

**Independent Test**: Can be tested by building the nano, micro, embedded, and full profiles, recording archive text size and stack-budget results, and confirming non-protocol behavior still passes.

**Acceptance Scenarios**:

1. **Given** the nano and embedded profiles, **When** size measurement runs, **Then** reported flash size does not regress beyond the documented budget.
2. **Given** the stack-usage gate, **When** compiler output omits a static helper because it was inlined or optimized away, **Then** the gate reports only concrete emitted frames and still fails for frames that exceed the configured budget.
3. **Given** aggregate subscriptions are enabled, **When** non-aggregate monitored items publish, **Then** they do not incur avoidable aggregate interval floating-point work.

---

### User Story 4 - Complete Traceability and Honest Claims (Priority: P2)

As a reviewer, I need the spec, plan, tasks, code comments, tests, and traceability docs to cite exact OPC UA parts and sections for each wire-visible behavior so implementation can be re-audited later.

**Why this priority**: The constitution rejects uncited protocol behavior and overstated conformance claims.

**Independent Test**: Can be tested by scanning generated task lists, traceability docs, and conformance docs for missing citations, stale AggregateFilter references, and profile-compliance claims not backed by OPC-10000-7 research.

**Acceptance Scenarios**:

1. **Given** this feature's tasks, **When** a task changes protocol behavior, **Then** the task cites the exact OPC UA section it implements or verifies.
2. **Given** the aggregate subscription documentation, **When** it references AggregateFilter, **Then** it cites OPC-10000-4 §7.22.4 and OPC-10000-13 aggregate behavior rather than stale section numbers.
3. **Given** conformance documentation, **When** it describes support status, **Then** the claim is limited to experimental or profile-targeting unless CTT evidence exists.

### Edge Cases

- Requests that use stale aggregate NodeIds previously accepted by the library must no longer be treated as standard AggregateFunction requests.
- Unsupported aggregate functions, invalid aggregate filter encodings, and invalid aggregate processing intervals must return cited monitored-item filter StatusCodes without installing partial state.
- `GetEndpoints` must handle an empty `profileUris` array as no transport-profile filter and a non-empty unmatched array as zero matching endpoints.
- Size and stack gates must distinguish real emitted frames from compiler-optimized static helpers while still detecting oversized emitted frames.
- Documentation-only traceability repairs must not change compatibility claims beyond the evidence available in this repository.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The library MUST define and test all public StatusCode constants touched by this feature against the official OPC Foundation StatusCode values.
- **FR-002**: The library MUST define and test `AggregateFilter_Encoding_DefaultBinary` as namespace 0 numeric NodeId `730`.
- **FR-003**: The library MUST define and test `AggregateFunction_Average`, `AggregateFunction_Minimum`, and `AggregateFunction_Maximum` as namespace 0 numeric NodeIds `2342`, `2346`, and `2347`.
- **FR-004**: The aggregate filter decoder MUST accept only the standard aggregate filter encoding and supported aggregate function NodeIds for this feature's scoped aggregate support.
- **FR-005**: The server MUST return cited monitored-item filter StatusCodes for unsupported or invalid aggregate filters without changing monitored-item state.
- **FR-006**: `GetEndpoints` MUST apply the requested transport-profile URI filter and return only endpoints that support at least one requested profile URI.
- **FR-007**: The stack-budget validation MUST not fail solely because an optimized build omits a static helper symbol, and MUST continue to fail on emitted frames that exceed the configured stack limit.
- **FR-008**: The implementation MUST reduce, provide profile configuration for, or explicitly document a standards-constrained non-change rationale for at least one measured RAM hotspot and at least one measured ROM or soft-float hotspot found in the audit.
- **FR-009**: Size measurements for nano, micro, embedded, and full profiles MUST be recorded after implementation and compared to the pre-change audit baseline.
- **FR-010**: Traceability and conformance documentation MUST replace stale OPC UA references and remove or qualify unsupported profile-compliant claims.
- **FR-011**: All protocol tasks and tests for this feature MUST cite exact OPC UA part and section numbers.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target role is embedded OPC UA Server over OPC UA TCP with OPC UA Binary, with conformance status limited to profile-targeting unless independent CTT evidence is added; profile terminology follows OPC-10000-7 §4.3.
- **OPC-002**: StatusCode names and values are governed by OPC-10000-4 §7.38.2 Common StatusCodes and the OPC Foundation `StatusCode.csv` table for numeric values.
- **OPC-003**: NodeIds for `AggregateFilter_Encoding_DefaultBinary` and AggregateFunction objects are governed by the OPC Foundation `NodeIds.csv` table; `AggregateFilter` behavior is scoped by OPC-10000-4 §7.22.4.
- **OPC-004**: MonitoredItem filter validation and monitored-item creation StatusCodes are governed by OPC-10000-4 §5.13.2.4, §5.13.3.4, §7.22.1, and §7.22.4.
- **OPC-005**: Average aggregate output and calculation constraints are governed by OPC-10000-13 §5.4.2.3 and §5.4.3.5; Minimum and Maximum aggregate behavior is governed by OPC-10000-13 aggregate function sections for those functions.
- **OPC-006**: `GetEndpoints` request filtering is governed by OPC-10000-4 §5.5.4.2, including `profileUris`.
- **OPC-007**: OPC UA Binary ExtensionObject and NodeId encoding expectations are governed by OPC-10000-6 §5.2.2.9 and §5.2.2.15.

### Scope Boundaries *(mandatory)*

- **In Scope**: StatusCode constant fidelity, aggregate filter wire identifiers and scoped validation, GetEndpoints transport-profile filtering, stack-budget gate correction, measured embedded size/performance improvements, and traceability documentation.
- **Out of Scope**: Full OPC UA aggregate history semantics, CTT certification, new transports, XML/JSON encodings, new security policies, broad service-dispatch refactoring unrelated to measured hotspots, and dynamic heap allocation.
- **Compatibility Claim**: After this feature, the project may claim profile-targeting support for the repaired scoped services and constants over OPC UA TCP Binary; it may not claim profile-compliant or CTT-verified status without external evidence.
- **Application Headroom Goal**: No more than 2 percent flash regression in nano or embedded profiles, documented static RAM impact in embedded defaults, and documented reductions, controls, or standards-constrained non-change rationale for the largest audited RAM and stack hotspots.

### Key Entities *(include if feature involves data)*

- **StatusCode Constant Set**: Public numeric constants that map StatusCode names to OPC UA wire values.
- **Aggregate Filter Contract**: The accepted ExtensionObject encoding NodeId, aggregate function NodeIds, interval, and scoped configuration handling for monitored items.
- **Endpoint Filter Contract**: The `GetEndpoints` request parameters that determine whether a server endpoint is included in the response.
- **Size Measurement Baseline**: The pre-change and post-change profile measurements used to judge ROM, RAM, and stack impact.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All StatusCode and aggregate NodeId tests introduced by this feature pass with the official OPC Foundation numeric values.
- **SC-002**: `GetEndpoints` tests demonstrate both matched and unmatched transport-profile filtering behavior.
- **SC-003**: Aggregate filter tests demonstrate standard NodeIds are accepted and stale or unsupported aggregate identifiers are rejected with cited StatusCodes.
- **SC-004**: The stack-budget test suite passes or reports only real budget violations, not missing optimized-away helper symbols.
- **SC-005**: Post-change size measurement records nano, micro, embedded, and full profile flash sizes and documents any delta greater than 1 percent.
- **SC-006**: At least one RAM hotspot and at least one ROM or soft-float hotspot have a measured reduction, a default configuration reduction, or an explicitly documented non-change rationale.
- **SC-007**: Traceability docs and this feature's tasks contain exact OPC UA section references for every protocol behavior changed by this feature.

## Assumptions

- The authoritative numeric identifier sources for this feature are the OPC Foundation UA NodeSet `UA-1.05.07-2026-04-15` `NodeIds.csv` and `StatusCode.csv` files.
- Existing host tests, integration tests, and CMake profiles remain the primary verification path.
- Existing aggregate behavior is a scoped subscription aggregate implementation, not a full historical aggregate implementation.
- SecurityPolicy behavior is not changed by this feature except for documentation honesty if existing claims are overstated.
