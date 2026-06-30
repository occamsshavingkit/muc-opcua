# Feature Specification: Optimize Hot Paths

**Feature Branch**: `022-optimize-hot-paths`
**Created**: 2026-06-30
**Status**: Draft
**Input**: User description: "Use optimize-hot-paths.md to specify benchmark-driven throughput, ROM, and RAM optimization work for the implemented OPC UA hot paths."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Verify Repeatable Optimization Evidence (Priority: P1)

As a maintainer, I need every optimization to be measured against a repeatable baseline so that claimed speed or resource improvements are trustworthy and reviewable.

**Why this priority**: Without a controlled baseline, performance changes cannot be distinguished from host noise and may hide regressions in embedded size or protocol behavior.

**Independent Test**: A reviewer can run the controlled benchmark process before and after a change and compare the reported throughput, ROM, RAM, scheduling, and host-fingerprint fields.

**Acceptance Scenarios**:

1. **Given** an optimization candidate and an existing baseline, **When** the controlled benchmark process is run, **Then** the result records the selected benchmark environment, workload matrix, resource measurements, and pass/fail comparison.
2. **Given** a benchmark run that is not using the required controlled environment, **When** results are reviewed for optimization evidence, **Then** the run is rejected as non-authoritative.

---

### User Story 2 - Improve Implemented OPC UA Hot Paths (Priority: P2)

As an embedded OPC UA integrator, I need the implemented Read, Write, Binary encoding, and OPC UA TCP framing paths to complete faster without increasing memory use so that constrained deployments can serve more requests within the same firmware budget.

**Why this priority**: The current baseline identifies these paths as the highest-value throughput targets while preserving the existing minimal server surface.

**Independent Test**: A targeted benchmark comparison shows improved normalized throughput for at least one hot-path workload while all resource and conformance gates remain passing.

**Acceptance Scenarios**:

1. **Given** the current benchmark baseline, **When** a hot-path optimization is applied, **Then** at least one targeted workload improves measurably without causing any resource gate failure.
2. **Given** a batch Read or Write workload, **When** optimization evidence is collected, **Then** per-operation results and StatusCodes remain equivalent to the pre-optimization behavior.

---

### User Story 3 - Preserve OPC UA Fidelity While Optimizing (Priority: P3)

As a conformance reviewer, I need every behavior-sensitive optimization to preserve the applicable OPC UA requirements so that faster code does not change wire encoding, service results, unsupported-feature behavior, or compatibility claims.

**Why this priority**: Optimizations that alter observable protocol behavior can pass simple throughput tests while breaking interoperability or conformance expectations.

**Independent Test**: A reviewer can trace each behavior-sensitive change to the relevant OPC UA section and verify existing positive and negative protocol tests still pass.

**Acceptance Scenarios**:

1. **Given** malformed Binary encoding or invalid service input, **When** the optimized path processes the input, **Then** it returns the same cited OPC UA StatusCode as the baseline behavior.
2. **Given** an unsupported OPC UA service or feature remains unsupported, **When** a client requests it after optimization, **Then** the service remains explicitly rejected rather than partially implemented or silently accepted.

---

### Edge Cases

- Benchmark results are produced on a host or scheduling setup that does not match the controlled baseline.
- A change improves a single workload but regresses another implemented workload in the same protocol area.
- A change lowers ROM or RAM but reduces throughput beyond the allowed optimization tolerance.
- Malformed Binary encoding, invalid NodeIds, invalid array/string lengths, and invalid MessageChunk boundaries must continue to produce the same failure behavior.
- Unsupported services, transports, encodings, security modes, or profile claims must remain explicitly out of scope.
- Benchmark comparisons must account for resource measurements as well as normalized throughput.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST preserve all currently implemented OPC UA wire-visible behavior for optimized paths, including encoded bytes, service-level results, operation-level StatusCodes, and unsupported-feature responses.
- **FR-002**: The system MUST require controlled before-and-after benchmark evidence for any claimed optimization.
- **FR-003**: The system MUST record enough benchmark context to determine whether an optimization result came from the approved repeatable environment.
- **FR-004**: The system MUST compare optimized results against the pre-optimization baseline for both normalized throughput and embedded resource use.
- **FR-005**: The system MUST reject an optimization as incomplete if it improves throughput but exceeds the allowed ROM, RAM, heap, stack, or correctness constraints.
- **FR-006**: The system MUST preserve negative-path behavior for malformed transport, Binary encoding, Read, and Write inputs.
- **FR-007**: The system MUST preserve existing conformance claims; optimization alone MUST NOT broaden supported profiles, services, encodings, transports, or security policies.
- **FR-008**: The system MUST document the before-and-after result for each optimized hot-path area so reviewers can determine whether the change is a net improvement.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA role/profile/conformance-unit assumptions remain unchanged from the currently documented server profile claims; no new conformance unit is claimed by this optimization work. Profile and conformance-unit terminology remains governed by OPC-10000-7 section 4.2.
- **OPC-002**: Optimized behavior may cover only already-implemented functionality in OPC-10000-6 section 5.2 Binary encoding, OPC-10000-6 section 5.2.2.4 String, OPC-10000-6 section 5.2.2.7 ByteString, OPC-10000-6 section 5.2.2.9 NodeId, OPC-10000-6 section 5.2.5 Arrays, OPC-10000-6 section 6.7.2 MessageChunk structure, OPC-10000-6 section 6.7.2.2 Message Header, OPC-10000-6 section 6.7.2.4 Sequence Header, OPC-10000-6 section 6.7.3 MessageChunks and error handling, OPC-10000-6 section 7.1.2.2 Message Header, OPC-10000-4 section 5.11.2 Read Service, and OPC-10000-4 section 5.11.4 Write Service.
- **OPC-003**: Unsupported services and invalid operation inputs must continue to return the currently cited StatusCodes according to OPC-10000-4 section 7.38.2 and any applicable service-specific StatusCode sections.
- **OPC-004**: Wire encoding and transport behavior must remain consistent with OPC-10000-6 section 5.2 Binary encoding, OPC-10000-6 section 6.7.2 MessageChunk structure, OPC-10000-6 section 6.7.3 MessageChunks and error handling, OPC-10000-6 section 6.7.4 Establishing a SecureChannel, and OPC-10000-6 section 7.2 OPC UA TCP.
- **OPC-005**: Write-related audit behavior and observable result semantics must remain consistent with OPC-10000-4 section 6.5.8 and OPC-10000-4 section 5.11.4; this optimization does not change security policy or conformance status.

### Scope Boundaries *(mandatory)*

- **In Scope**: Throughput and resource optimization for already-implemented OPC UA Binary encoding, MessageChunk framing, Read, Write, and response-framing behavior; benchmark evidence and resource regression gates for those paths.
- **Out of Scope**: New OPC UA services, new transports, alternate encodings, new security policies, new conformance claims, new heap-dependent behavior, public interface expansion for unrelated features, and changes to unsupported-service behavior.
- **Compatibility Claim**: After this feature ships, the project may claim only the same OPC UA Binary over opc.tcp services, profiles, and conformance status it could claim before the optimization, with improved measured performance where evidence supports it.
- **Application Headroom Goal**: Every optimized profile must remain within the current resource gates: no more than 3% ROM growth, no more than 5% static RAM growth, no new heap use, and no undocumented stack growth.

### Key Entities *(include if feature involves data)*

- **Benchmark Baseline**: The authoritative pre-optimization measurement set containing workload throughput, resource usage, host fingerprint, scheduling context, and comparison thresholds.
- **Benchmark Result**: A post-change measurement set that can be compared with the baseline for normalized throughput and resource impact.
- **Hot-Path Workload**: A repeatable exercise of an implemented protocol path such as single-item Read, batched Read, single-item Write, batched Write, malformed input handling, or response framing.
- **Resource Budget**: The allowed ROM, RAM, heap, stack, and throughput limits used to decide whether an optimization is acceptable.
- **Protocol Behavior Contract**: The set of OPC UA-visible results, encodings, StatusCodes, and compatibility claims that must remain unchanged while internal performance improves.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: At least one targeted hot-path workload improves normalized throughput by 5% or more compared with the pre-optimization controlled baseline.
- **SC-002**: No targeted hot-path workload regresses by more than 5% normalized throughput compared with the pre-optimization controlled baseline unless the change is explicitly accepted as a size-first optimization.
- **SC-003**: All profiles remain within the resource gates: ROM no more than 3% above baseline and static RAM no more than 5% above baseline.
- **SC-004**: No optimized path introduces heap allocation, broadens the public compatibility claim, or changes unsupported-feature behavior.
- **SC-005**: All affected positive and negative OPC UA behavior checks continue to pass, including malformed transport, Binary encoding, Read, and Write cases.
- **SC-006**: Two consecutive controlled benchmark comparisons on the approved host produce the same pass/fail outcome and include matching scheduling and host-fingerprint fields.

## Assumptions

- The optimization baseline is the current controlled benchmark data generated on 2026-06-30 unless a fresh pre-change baseline is captured before implementation begins.
- The approved repeatable environment is the known lab host with isolated benchmark CPU, realtime scheduling, performance governor, and recorded host fingerprint.
- The optimization work targets already-implemented server behavior only; any broader OPC UA feature work requires a separate specification.
- Resource gates are evaluated per profile because a change that is acceptable in one profile may be unacceptable in a smaller profile.
- A size-first optimization may be accepted with lower throughput only if it is explicitly documented and still passes conformance and resource gates.
