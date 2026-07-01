# Feature Specification: Conformance Docs and PubSub Subscriber

**Feature Branch**: `023-conformance-docs-subscriber`
**Created**: 2026-06-30
**Status**: Draft
**Input**: User description: "Start items 2 through 5: conformance/evidence cleanup, stronger in-repo conformance substitute checks, scoped PubSub UADP/UDP subscriber support, and profile-relevant subscription/DataChange/status polish. Examine user and implementor documentation for ease of use and remove or guard stale numeric claims and stale OPC UA behavior/status documentation."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Use Current Documentation Confidently (Priority: P1)

An embedded implementor can start from the README, getting started guide, API reference, and integration guide and understand the current profile support, memory ownership model, PubSub scope, and unsupported areas without finding stale "not implemented" or outdated numeric claims.

**Why this priority**: Users choose whether this library fits their device and OPC UA needs from these documents. Stale support claims, stale sizes, or stale status-code examples create immediate integration risk.

**Independent Test**: Review and automated documentation checks can prove the public documentation no longer contains known stale support claims, stale profile-size statements, stale status-code examples, or outdated references to already implemented History, NodeManagement, and aggregate-subscription work.

**Acceptance Scenarios**:

1. **Given** a new implementor reads the README and getting started documentation, **When** they look for current profile support and resource requirements, **Then** they see current profile-targeting language, how to reproduce size data, and no stale "remaining Micro item" or "not implemented yet" statements for implemented features.
2. **Given** a maintainer reads the API and integration references, **When** they follow PubSub, NodeManagement, History, Query, and aggregate sections, **Then** each section matches the current supported/unsupported behavior and points to conformance or traceability evidence.
3. **Given** a document includes measured sizes, NodeIds, StatusCodes, or benchmark values, **When** the value is not generated live, **Then** the document labels it as a measured snapshot and points to the command or evidence file used to reproduce it.

---

### User Story 2 - Catch Stale Claims Before Release (Priority: P1)

A maintainer can rely on in-repo checks as the project's practical CTT substitute until external CTT is available: stale conformance claims, stale numeric constants, unsupported profile language, missing traceability rows, and outdated size evidence are detected by tests or CI-facing validation.

**Why this priority**: The project cannot depend on CTT soon, so in-repo evidence must prevent regressions before new standard surface is added.

**Independent Test**: The documentation/conformance test suite fails when known stale phrases, unsupported CTT/profile-compliance claims, stale StatusCode values, stale NodeId values, or missing traceability evidence are introduced.

**Acceptance Scenarios**:

1. **Given** a contributor changes documentation to claim profile compliance or CTT verification without evidence, **When** the conformance documentation tests run, **Then** the tests fail and identify the unsupported claim.
2. **Given** a contributor reintroduces stale aggregate function NodeIds, stale StatusCode values, or stale Query/NodeManagement section references, **When** the conformance and traceability tests run, **Then** they fail with a precise message.
3. **Given** the CI validation set runs, **When** the conformance documentation and traceability checks pass, **Then** maintainers can see which in-repo checks cover claims that would otherwise depend on external CTT.

---

### User Story 3 - Receive Scoped UADP NetworkMessages (Priority: P2)

An embedded application that already publishes scoped UADP/UDP messages from this library can also decode that same bounded UADP subset from UDP payload bytes using caller-provided storage, without enabling broad PubSub configuration, discovery, security, JSON, broker, chunked, or dynamic metadata features.

**Why this priority**: PubSub publisher support exists; a matching subscriber decoder improves practical interoperability while preserving the minimal, embedded-first surface.

**Independent Test**: A UADP NetworkMessage produced by the current publisher fixture decodes into the expected publisher id, dataset writer id, and scalar field values; unsupported UADP layouts and malformed inputs fail deterministically without heap allocation.

**Acceptance Scenarios**:

1. **Given** a scoped UADP/UDP payload produced by the existing publisher, **When** the subscriber decoder is given caller-provided output slots, **Then** it returns `Good` and fills those slots with the expected scalar Variant values.
2. **Given** a malformed, truncated, or length-inconsistent UADP payload, **When** the subscriber decoder processes it, **Then** it returns `Bad_DecodingError` and leaves no partially trusted output beyond the reported decoded count.
3. **Given** a valid UADP payload that uses unsupported PubSub features such as security headers, chunk messages, multiple DataSetMessages, delta frames, event messages, RawData-only metadata-dependent fields, JSON, or broker mappings, **When** the scoped subscriber decoder processes it, **Then** it returns `Bad_NotSupported` or the documented unsupported status without claiming broader PubSub support.

---

### User Story 4 - Preserve Profile-Relevant Negative Paths (Priority: P3)

An interoperability maintainer can exercise selected subscription, DataChange filter, StatusCode, and unsupported-feature negative paths and see deterministic, cited results that match the implemented profile-targeting scope.

**Why this priority**: The next standard surface should not reopen earlier fidelity defects around filters, aggregate scope, bad IDs, invalid acknowledgements, or unsupported service behavior.

**Independent Test**: Targeted negative-path tests for selected Subscription/MonitoredItem/DataChange/status behavior pass with exact cited StatusCodes and conformance documentation records the evidence.

**Acceptance Scenarios**:

1. **Given** an invalid Subscription or MonitoredItem identifier, invalid Publish acknowledgement, unsupported DataChange/deadband option, unsupported AggregateFunction, or malformed filter ExtensionObject, **When** the server handles the request, **Then** it returns the documented StatusCode and does not corrupt bounded subscription state.
2. **Given** a supported profile-targeting behavior already exists, **When** new tests are added for negative paths, **Then** any implementation fix is narrowly scoped and does not add broad Query, History, alarm, or PubSub configuration surface.
3. **Given** profile, conformance, and traceability documents are updated, **When** a maintainer audits the feature, **Then** each changed behavior cites its OPC UA section and points to the test that verifies it.

### Edge Cases

- UADP message is shorter than the declared header, payload-header, DataSetMessage size, or field count.
- UADP message declares more fields than the caller-provided output capacity.
- UADP message contains a DataSetMessage size larger than the remaining payload.
- UADP message uses UADP security headers, chunk NetworkMessages, discovery probe/announcement messages, multiple DataSetMessages, event messages, delta frames, or unsupported field encodings.
- PubSub UDP payload is larger than the configured decoder input length or would require heap allocation.
- Public documentation contains measured profile-size or benchmark numbers that are stale or not reproducible from an identified command.
- Documentation claims CTT verification, profile compliance, or unsupported OPC UA services without matching evidence.
- Subscription negative-path tests cover errors without changing successful behavior or widening the supported standard surface.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Public user and implementor documentation MUST describe current support for Nano, Micro, Embedded profile-targeting, History, NodeManagement, Query, aggregate subscriptions, and PubSub without stale "not implemented" or stale "remaining item" statements.
- **FR-002**: Documentation that includes profile sizes, storage sizes, StatusCodes, NodeIds, benchmark numbers, or profile claims MUST either match current evidence or label the value as a dated snapshot with a reproduction command.
- **FR-003**: In-repo documentation/conformance checks MUST reject unsupported `profile-compliant` or `CTT-verified` claims unless the repository contains explicit evidence for that claim.
- **FR-004**: In-repo checks MUST guard public StatusCode numeric values and names against stale values for all StatusCodes referenced by this feature.
- **FR-005**: In-repo checks MUST guard standard NodeIds and section references used by aggregate, Query, NodeManagement, and PubSub documentation against known stale values or stale section labels.
- **FR-006**: The scoped PubSub subscriber MUST decode the UADP NetworkMessage subset produced by the current publisher: UADP/UDP transport payload, publisher id, single DataSetMessage payload header, DataSetWriterId, Data Key Frame DataSetMessage, and bounded scalar Variant fields.
- **FR-007**: The scoped PubSub subscriber MUST use caller-provided output storage and MUST NOT allocate heap memory or add mandatory server hot-path storage.
- **FR-008**: The scoped PubSub subscriber MUST reject malformed input with `Bad_DecodingError`, caller/configuration errors with `Bad_InvalidArgument`, output capacity exhaustion with `Bad_TooManyOperations`, and unsupported PubSub layouts with `Bad_NotSupported`.
- **FR-009**: PubSub subscriber documentation MUST clearly state the supported subset and explicitly list excluded PubSub features: PubSub configuration model, PubSub security, discovery/meta-data announcements, JSON mapping, MQTT/broker mappings, Ethernet transport, chunked NetworkMessages, multi-DataSetMessage payloads, delta frames, event messages, RawData-only decoding without metadata, and CTT conformance claims.
- **FR-010**: Selected subscription/DataChange/status polish MUST be implemented test-first and MUST preserve existing successful behavior while adding or correcting only cited negative-path behavior.
- **FR-011**: Traceability documentation MUST map every standard-affecting implementation and test file changed by this feature to exact OPC UA sections.
- **FR-012**: Final validation MUST run conformance documentation checks, traceability checks, affected protocol tests, host tests for changed areas, and size/resource measurements relevant to any code changes.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Profile and claim language is governed by OPC-10000-7 §4.2 Conformance Units and Conformance Groups and §4.3 Profiles; this feature remains profile-targeting/experimental for scoped PubSub subscriber behavior unless external evidence is added.
- **OPC-002**: PubSub subscriber message reception scope cites OPC-10000-14 §5.4.2.1 Subscriber general behavior and §5.4.2.2 message reception.
- **OPC-003**: Broker-less UADP over UDP scope cites OPC-10000-14 §5.4.6.2.2 and §7.3.2.1.
- **OPC-004**: UADP NetworkMessage decoding cites OPC-10000-14 §7.2.4.1, §7.2.4.2, and §7.2.4.4.2.
- **OPC-005**: UADP payload and DataSetMessage decoding cites OPC-10000-14 §7.2.4.5.2, §7.2.4.5.3, §7.2.4.5.4, and §7.2.4.5.5.
- **OPC-006**: Scalar Variant field decoding cites OPC-10000-6 §5.2.2.16; unsupported RawData-only and metadata-dependent decoding is excluded by this feature.
- **OPC-007**: StatusCode names and values used by this feature cite OPC-10000-4 §7.38.2.
- **OPC-008**: Subscription and MonitoredItem negative-path polish cites OPC-10000-4 §5.13, §5.14, §7.17.2, §7.22.1, §7.22.2, §7.22.4, and §7.38.2 as applicable to the touched behavior.
- **OPC-009**: Aggregate scope remains limited to Average, Minimum, and Maximum citations already used by the project: OPC-10000-13 §4.2.2.4/§5.4.3.5, §4.2.2.9/§5.4.3.10, and §4.2.2.10/§5.4.3.11.
- **OPC-010**: Query and NodeManagement documentation citations remain OPC-10000-4 Appendix B §B.2.3/§B.2.4 for Query and OPC-10000-4 §5.8 for NodeManagement.

### Scope Boundaries *(mandatory)*

- **In Scope**: Public documentation cleanup; in-repo stale-claim and stale-number checks; CI-facing conformance/traceability evidence; a caller-storage scoped UADP/UDP subscriber decoder compatible with the existing publisher subset; targeted subscription/DataChange/status negative-path tests and narrow fixes.
- **Out of Scope**: External CTT certification; profile-compliant claims; broad PubSub configuration model; PubSub security; PubSub metadata discovery; JSON, MQTT, broker, Ethernet, or DTLS mappings; full Part 14 subscriber implementation; full Part 13 aggregate semantics; new History/Query feature breadth; heap-backed dynamic PubSub metadata.
- **Compatibility Claim**: After this feature ships, the project may claim a tested, experimental scoped UADP/UDP publisher/subscriber subset over OPC UA Binary-compatible Variant fields, plus current profile-targeting server documentation. It may not claim PubSub profile compliance or CTT verification.
- **Application Headroom Goal**: Documentation-only and check-only work adds no runtime cost. PubSub subscriber code should target no mandatory static RAM, no heap, caller-provided output buffers, and less than 2 KiB additional Arm Cortex-M0+ `.text` unless measurements justify otherwise.

### Key Entities *(include if feature involves data)*

- **Documentation Evidence Row**: A support or conformance claim with its current status, cited OPC UA section, evidence file, reproduction command, and snapshot date if numeric.
- **Stale Numeric Guard**: A testable assertion for a StatusCode, NodeId, profile size, benchmark number, or OPC UA section reference that must remain current or explicitly snapshot-labeled.
- **Scoped UADP Subscriber Message**: A decoded UADP NetworkMessage containing publisher id, dataset writer id, scalar field values, and decoded count.
- **Unsupported PubSub Layout**: A valid or malformed PubSub payload whose required feature is outside the scoped subscriber and must fail deterministically without widening the implementation claim.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Public README, getting started, API, integration, conformance, and traceability documentation contain zero known stale support statements for implemented History, NodeManagement, Query, aggregate, subscription, and PubSub publisher behavior.
- **SC-002**: Documentation/conformance tests fail when unsupported CTT/profile-compliance claims, known stale StatusCode values, known stale aggregate NodeIds, or known stale section references are introduced.
- **SC-003**: At least one existing publisher-generated UADP fixture decodes successfully through the new subscriber path with exact expected publisher id, dataset writer id, and scalar Variant values.
- **SC-004**: Malformed and unsupported UADP subscriber tests cover truncation, size mismatch, output-capacity exhaustion, unsupported NetworkMessage type/layout, unsupported DataSetMessage type, and unsupported field encoding.
- **SC-005**: Targeted subscription/DataChange/status negative-path tests pass with exact cited StatusCodes and no change to existing successful-path tests.
- **SC-006**: Runtime code changes add no heap use and do not exceed the planned flash/static-RAM budget, with measured results recorded in the size ledger or feature traceability document.
- **SC-007**: Traceability checks map every standard-affecting changed implementation and test file to OPC UA section citations before implementation is considered complete.

## Assumptions

- Existing profile claims remain profile-targeting until external CTT evidence is available.
- The PubSub subscriber first targets the UADP subset already produced by `mu_encode_uadp_network_message`; broader Part 14 features require a separate feature plan.
- Documentation numeric values should prefer reproducible commands and evidence links over hard-coded prose values when possible.
- Any selected subscription/DataChange/status polish will be limited to negative paths whose expected behavior can be cited and tested without expanding the supported profile surface.
