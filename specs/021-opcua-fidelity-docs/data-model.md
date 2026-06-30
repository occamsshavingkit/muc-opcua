# Data Model: OPC UA Fidelity Docs and Next Work

## Integration Guidance

Represents a user-facing documentation slice for embedded bring-up.

- **Fields**: target platform, build mode, required adapters, static storage, receive buffer, send buffer, polling loop, RAM accounting categories, limitations.
- **Validation rules**: Must distinguish core library memory from platform heap/task/network resources. Must not claim a complete in-tree FreeRTOS + lwIP platform adapter.

## Conformance Citation

Represents a mapping from a supported, optional, or unsupported behavior to an OPC UA part and section.

- **Fields**: behavior name, OPC UA section, support status, expected StatusCode where applicable, evidence files.
- **Validation rules**: Behavior-changing tasks must cite the same section or a narrower subsection. Citation rows must not use stale sections.

## Aggregate Filter Case

Represents a supported or rejected AggregateFilter scenario.

- **Fields**: AggregateFunction NodeId, filter parameters, processing interval, sample set, expected value or StatusCode.
- **Validation rules**: Supported cases are limited to Average, Minimum, and Maximum. Unsupported NodeIds return `Bad_MonitoredItemFilterUnsupported`.

## Subscription Negative Path

Represents a malformed, invalid-state, invalid-ID, resource-exhaustion, or unsupported-service scenario.

- **Fields**: service name, input shape, target state, configured capacity, expected StatusCode, storage impact.
- **Validation rules**: Must be independently testable. Must not allocate heap. Must leave server state consistent after rejection.

## Profile Claim

Represents documentation language about profile/conformance status.

- **Fields**: profile name, status label, evidence, missing external evidence, allowed claim wording.
- **Validation rules**: Profile-compliant and CTT-verified wording is allowed only when the same line names external evidence or clearly states the claim is not being made.
