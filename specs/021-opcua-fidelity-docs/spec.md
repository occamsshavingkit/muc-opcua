# Feature Specification: OPC UA Fidelity Docs and Next Work

**Feature Branch**: `021-opcua-fidelity-docs`
**Created**: 2026-06-30
**Status**: Draft
**Input**: User description: "Do the five recommended next OPC UA work areas: close FreeRTOS + lwIP getting-started documentation, fix conformance citation drift, finish aggregate subscription fidelity, harden MonitoredItem/Subscription negative paths, and clean up profile/conformance claims."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Bring Up FreeRTOS + lwIP (Priority: P1)

An embedded integrator wants to wire `micro-opcua` into a FreeRTOS + lwIP target without using the in-tree Pico skeleton as a complete network stack.

**Why this priority**: This closes the only currently open GitHub issue and gives embedded users the shortest path from host example to real firmware integration while preserving the no-heap library claim.

**Independent Test**: A reader can follow the getting-started documentation and identify which adapters, static buffers, build options, task loop, and RAM categories are owned by the application versus the library.

**Acceptance Scenarios**:

1. **Given** a user has a FreeRTOS + lwIP firmware project, **When** they read the getting-started guide, **Then** they see how to use `MICRO_OPCUA_PLATFORM=external`, caller-owned storage, nonblocking lwIP socket callbacks, FreeRTOS tick time, entropy, and a polling task.
2. **Given** a user is budgeting embedded RAM, **When** they read the FreeRTOS + lwIP section, **Then** they can distinguish library storage and transport buffers from FreeRTOS heap, lwIP heap, and task stack.

---

### User Story 2 - Trust Conformance Citations (Priority: P1)

A maintainer reviewing conformance docs wants service rows and profile claims to cite the correct OPC UA standard sections before implementation tasks depend on those citations.

**Why this priority**: Incorrect citations make future implementation tasks harder to verify and violate the project constitution's spec-fidelity requirement.

**Independent Test**: The conformance service matrix contains corrected section references for Method Call and NodeManagement and remains free of unsupported profile-compliance or CTT-verified claims.

**Acceptance Scenarios**:

1. **Given** a maintainer reads the service matrix, **When** they inspect Method Call, **Then** the row cites OPC-10000-4 section 5.12.2.
2. **Given** a maintainer reads the service matrix, **When** they inspect AddNodes/DeleteNodes/AddReferences/DeleteReferences, **Then** the row cites OPC-10000-4 section 5.8.
3. **Given** conformance docs mention profiles or CTT, **When** the conformance-doc tests run, **Then** unsupported affirmative profile-compliant and CTT-verified claims are rejected.

---

### User Story 3 - Finish Aggregate Filter Fidelity (Priority: P2)

A client using AggregateFilter for Average, Minimum, and Maximum wants supported aggregate filters to behave predictably and unsupported aggregate selections to fail with explicit per-item status.

**Why this priority**: Aggregate filters are the active protocol feature area, and the implementation must stay scoped to verified Part 13 behavior rather than implying full aggregate support.

**Independent Test**: Aggregate filter tests verify supported Average/Minimum/Maximum behavior and representative unsupported, malformed, or disallowed AggregateFilter cases with cited OPC UA StatusCodes.

**Acceptance Scenarios**:

1. **Given** a monitored item is created with a supported Average, Minimum, or Maximum AggregateFilter, **When** publish cycles process samples, **Then** the returned values match the documented scoped aggregate calculation.
2. **Given** a monitored item is created with an unsupported aggregate function NodeId, **When** the server validates the filter, **Then** the per-item result is `Bad_MonitoredItemFilterUnsupported`.
3. **Given** a malformed AggregateFilter ExtensionObject is received, **When** the request is decoded, **Then** decoding fails without out-of-bounds reads or silent success.

---

### User Story 4 - Harden MonitoredItem and Subscription Negative Paths (Priority: P2)

A client or fuzzer sends invalid MonitoredItem and Subscription service inputs and expects deterministic OPC UA failure responses without heap use or state corruption.

**Why this priority**: Subscriptions are stateful and resource-sensitive; negative-path holes are likely to become interoperability or memory-safety failures on microcontrollers.

**Independent Test**: Focused unit and integration tests cover invalid IDs, invalid queue/timing parameters, invalid publish acknowledgements, unsupported TransferSubscriptions, and capacity exhaustion with cited StatusCodes.

**Acceptance Scenarios**:

1. **Given** a client references an unknown monitored item or subscription, **When** the relevant service executes, **Then** the response returns the cited invalid-ID StatusCode.
2. **Given** a client exceeds configured subscription, monitored-item, publish request, queue, or trigger-link capacity, **When** the service executes, **Then** the server returns the cited resource StatusCode without increasing storage.
3. **Given** a client calls unsupported TransferSubscriptions, **When** the service is dispatched, **Then** the server returns `Bad_ServiceUnsupported`.

---

### User Story 5 - Keep Profile Claims Honest (Priority: P3)

A maintainer preparing release notes wants profile, conformance-unit, and CTT language to remain exact, scoped, and evidence-backed.

**Why this priority**: The project is useful only when users can tell what is implemented, what is merely profile-targeting, and what still requires external CTT evidence.

**Independent Test**: Profile/conformance docs consistently label claims as profile-targeting unless external evidence exists and map supported services, encodings, transport, and selected conformance units to exact OPC UA sections.

**Acceptance Scenarios**:

1. **Given** a profile doc names Nano, Micro, or Embedded 2017, **When** a reader checks the status, **Then** it says profile-targeting and names the missing external CTT evidence.
2. **Given** a service or facet is optional or unsupported, **When** a reader checks conformance docs, **Then** the docs describe the compile-time option or expected unsupported behavior.
3. **Given** future implementation tasks are generated, **When** a coder reads each behavior-changing task, **Then** it includes the OPC UA section being implemented or verified.

### Edge Cases

- The FreeRTOS + lwIP documentation must not imply the core library depends on FreeRTOS, lwIP, Pico SDK, or heap allocation.
- Documentation must not claim full working Pico W support in-tree; the current Pico adapter remains a skeleton unless a future feature wires a real TCP/IP stack.
- Aggregate work must not imply full OPC-10000-13 aggregate coverage beyond the scoped Average, Minimum, and Maximum behavior.
- Unsupported services and filters must fail with cited OPC UA StatusCodes instead of silent success.
- Any change that increases flash, RAM, stack, static tables, transport buffers, or crypto dependencies must record the impact.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The getting-started guide MUST include a FreeRTOS + lwIP integration section that covers external platform builds, required adapters, static server storage, static receive/send buffers, nonblocking polling, and RAM accounting.
- **FR-002**: The FreeRTOS + lwIP guidance MUST distinguish library-owned behavior from platform-owned FreeRTOS/lwIP resources and MUST not claim the core library uses heap allocation.
- **FR-003**: The conformance service matrix MUST correct known citation drift for Method Call and NodeManagement rows.
- **FR-004**: Profile and conformance documentation MUST continue to reject unsupported affirmative profile-compliant or CTT-verified claims unless external CTT evidence is named.
- **FR-005**: AggregateFilter work MUST remain scoped to Average, Minimum, and Maximum and MUST explicitly document unsupported aggregate functions and malformed filter behavior.
- **FR-006**: MonitoredItem and Subscription negative-path tests MUST cover invalid identifiers, capacity limits, unsupported TransferSubscriptions, invalid acknowledgements, and malformed filters before behavior changes are made.
- **FR-007**: Every behavior-changing task generated for this feature MUST cite the exact OPC UA section it implements or verifies.
- **FR-008**: Any task that risks increasing flash, RAM, stack, static tables, transport buffers, or throughput cost MUST be marked as resource-risk in `tasks.md`.
- **FR-009**: Documentation-only changes MAY omit RED tests only when they are validated by existing documentation/conformance checks or link/format review.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target status remains profile-targeting under OPC-10000-7 sections 4.2 and 4.3; no profile-compliant or CTT-verified claim is introduced without external evidence.
- **OPC-002**: FreeRTOS + lwIP transport guidance is scoped to OPC UA TCP over `opc.tcp` with OPC UA Binary: OPC-10000-6 section 5.2, section 6.7.2, section 7.1.2.2, section 7.1.2.3, section 7.1.2.4, and section 7.2.
- **OPC-003**: Method Call conformance rows cite OPC-10000-4 section 5.12.2.
- **OPC-004**: NodeManagement conformance rows cite OPC-10000-4 section 5.8.
- **OPC-005**: AggregateFilter behavior cites OPC-10000-4 section 7.22.4 and the scoped Average, Minimum, and Maximum references in OPC-10000-13 sections 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, and 5.4.3.11.
- **OPC-006**: MonitoredItem and Subscription negative paths cite OPC-10000-4 sections 5.13.2, 5.13.3, 5.13.4, 5.13.5, 5.13.6, 5.14.2, 5.14.3, 5.14.4, 5.14.5, 5.14.6, 5.14.7, and 5.14.8 as applicable.
- **OPC-007**: StatusCode names and numeric values continue to be checked against OPC-10000-4 section 7.38.2 and OPC-10000-6 section 7.1.5.

### Scope Boundaries *(mandatory)*

- **In Scope**: FreeRTOS + lwIP getting-started docs, conformance citation cleanup, scoped aggregate-filter follow-up planning, subscription negative-path hardening plan/tasks, and profile/conformance claim cleanup.
- **Out of Scope**: Implementing a full in-tree Pico W FreeRTOS/lwIP example, adding a new transport, changing profile-targeting wording to profile-compliant or CTT-verified status without external CTT evidence, implementing full OPC-10000-13 aggregate coverage, or adding TransferSubscriptions support.
- **Compatibility Claim**: After this feature ships, the project may claim improved profile-targeting documentation and scoped behavior coverage only; it may not claim profile-compliant or CTT-verified status.
- **Application Headroom Goal**: Documentation and citation cleanup must not increase code size or runtime RAM; future aggregate/subscription behavior changes must stay within the plan's size budget and record measured impact.

### Key Entities *(include if feature involves data)*

- **Integration Guidance**: User-facing documentation describing build options, adapter responsibilities, memory ownership, task loop, and RAM accounting.
- **Conformance Citation**: A documentation row or task reference mapping implemented or unsupported behavior to an OPC UA part and section.
- **Aggregate Filter Case**: A supported, unsupported, malformed, or disallowed filter configuration and its expected value/status outcome.
- **Subscription Negative Path**: A resource, identifier, state, timing, acknowledgement, or unsupported-service input and its expected StatusCode.
- **Profile Claim**: Documentation wording about profile-targeting, profile-compliant, or CTT-verified status and the evidence required to support it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The getting-started guide contains a FreeRTOS + lwIP section covering at least external platform selection, TCP adapter callbacks, FreeRTOS time, entropy, static storage, server task loop, and RAM accounting.
- **SC-002**: Documentation/conformance tests pass after citation and claim wording changes.
- **SC-003**: Every generated behavior-changing task includes an OPC UA section reference.
- **SC-004**: Every task with possible flash/RAM/throughput impact is marked resource-risk in `tasks.md`.
- **SC-005**: Aggregate and subscription follow-up work has independently testable tasks that name RED tests before implementation.
- **SC-006**: The feature introduces no new affirmative profile-compliant or CTT-verified claim without external evidence.
- **SC-007**: Documentation-only slices add zero runtime flash and RAM by construction; any protocol slice records measured size deltas before completion.

## Assumptions

- The current open GitHub issue is #222, requesting FreeRTOS + lwIP integration documentation.
- The FreeRTOS + lwIP content belongs in `docs/getting-started.md` with links to the deeper `docs/integration-guide.md` rather than a new platform page for this first slice.
- The current in-tree Pico adapter is a skeleton and must be documented as such.
- The OPC UA reference MCP server may be unavailable; citations are still required and must be checked against existing repository traceability and official OPC UA section numbering before implementation claims.
- External CTT evidence is not available for this feature.
