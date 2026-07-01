# Research: Conformance Docs and PubSub Subscriber

## Decision: Treat documentation as conformance evidence, not marketing copy

**Decision**: Public documentation must remove stale "not implemented" and "remaining Micro item" statements, label numeric values as measured snapshots or reproducible outputs, and route detailed claims through conformance/traceability pages.

**Rationale**: The project is relying on in-repo tests and interop until CTT is available. Documentation is part of the conformance surface because users decide whether to integrate the library from these claims.

**Alternatives considered**: Leaving size and support statements as prose was rejected because stale prose already survived prior implementation passes.

## Decision: Extend existing conformance-doc checks

**Decision**: Add stale-claim/stale-number checks to `tests/unit/test_conformance_docs.c` and traceability checks to `tests/unit/test_traceability_docs.c` instead of creating a new standalone linter.

**Rationale**: These tests already parse documentation, enforce profile-targeting/CTT honesty, and validate StatusCode names and values. Extending them keeps the CI signal concentrated and avoids a second parser with inconsistent exemptions.

**Alternatives considered**: A shell-only grep gate was rejected because it would duplicate existing C test helpers and make precise StatusCode/NodeId checks harder to maintain.

## Decision: Scope PubSub subscriber to the existing publisher wire format

**Decision**: Implement a public decode function for the UADP NetworkMessage subset already emitted by `mu_encode_uadp_network_message`: UInt32 PublisherId, UADP PayloadHeader with one DataSetWriterId, one sized Data Key Frame DataSetMessage, and scalar OPC UA Binary Variant fields.

**Rationale**: MCP reference lookups identified the relevant Part 14 sections: OPC-10000-14 §7.2.4.4.2 NetworkMessage layout, §7.2.4.5.2 DataSet payload header, §7.2.4.5.3 DataSet payload, §7.2.4.5.4 DataSetMessage header, §7.2.4.5.5 Data Key Frame DataSetMessage, and §7.3.2.1 UDP transport. OPC-10000-6 §5.2.2.16 covers the scalar Variant encoding reused by the project. A matching decoder gives practical interoperability without broadening PubSub claims.

**Alternatives considered**: Implementing a full Subscriber configuration model, DataSetMetaData handling, chunked NetworkMessages, PubSub security, or a UDP receive loop was rejected for this feature because it adds RAM/flash surface and conformance claims beyond the current evidence.

## Decision: Use explicit decoder statuses

**Decision**: Return `Bad_InvalidArgument` for null pointers or invalid caller output configuration, `Bad_DecodingError` for malformed/truncated bytes, `Bad_TooManyOperations` when the caller output capacity is insufficient or the payload exceeds `MU_PUBSUB_MAX_FIELDS`, and `Bad_NotSupported` for valid but unsupported UADP layouts.

**Rationale**: OPC-10000-4 §7.38.2 provides the StatusCode names and values. This separates caller misuse, malformed wire data, bounded-resource limits, and unsupported feature requests.

**Alternatives considered**: Returning `Bad_DecodingError` for every failure was rejected because unsupported but well-formed UADP layouts should not be treated as malformed.

## Decision: Negative-path polish stays targeted

**Decision**: Add or update only the subscription/DataChange/status tests that close clear evidence gaps found during implementation, then apply minimal fixes if those tests expose behavior drift.

**Rationale**: OPC-10000-4 §5.13, §5.14, §7.17.2, §7.22.1, §7.22.2, §7.22.4, and §7.38.2 govern the relevant negative paths. The goal is fidelity to the implemented subset, not a broad feature expansion.

**Alternatives considered**: Expanding Query, History, alarms, or full aggregate behavior was rejected because the user asked for in-repo conformance substitute work and selected next steps, not a broad standard-surface expansion.

## Decision: Size/resource evidence is required for code-affecting tasks

**Decision**: Mark PubSub decoder and any subscription/status behavior tasks as ROM/RAM/throughput risk and record the measured size impact before completion.

**Rationale**: The constitution requires flash/RAM discipline for every feature. The planned decoder should add no static RAM and less than 2 KiB Arm Cortex-M0+ `.text`, but this must be measured.

**Alternatives considered**: Deferring size measurement until a later optimization pass was rejected because decoder size risk is visible at implementation time.
