# Feature Specification: Implement Deferred Features (D1-D4)

**Feature Branch**: `045-implement-deferred-features`  
**Created**: 2026-07-06  
**Status**: Draft  
**Input**: User description: "implement D1,2,3,4 from the TODO file."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Complex Type Binary Encode/Decode (Priority: P1)

An embedded developer using the library needs to serialize and deserialize custom data
structures over the OPC UA binary protocol so that complex data types defined in the
address space can be transmitted between client and server.

The current registration API (`mu_register_structure_type`, `mu_register_enumeration_type`)
works, but `src/encoding/binary_complex.c` is a dead stub — no encode or decode functions
exist. This means any application-defined structure types cannot be serialized for wire
transmission.

**Why this priority**: P1 — unblocks the most impactful missing feature. Complex types
are required for any non-trivial OPC UA application that defines custom data structures.
Without encode/decode, the registration API is useless at runtime.

**Independent Test**: `ctest -R test_complex_types` — round-trip encode→decode→deep-equal
for ≥3 structure types (scalar fields, optional fields, nested structures, arrays).

**Acceptance Scenarios**:

1. **Given** a registered structure type with required scalar fields, **When** a value
   of that type is binary-encoded then binary-decoded, **Then** all field values are
   preserved through the round-trip.
2. **Given** a registered structure type with optional fields and an EncodingMask,
   **When** only a subset of optional fields are set in the value then encoded then
   decoded, **Then** only the fields indicated by the EncodingMask are present in the
   decoded value, and unset fields are absent.
3. **Given** a registered structure type containing a nested structure field,
   **When** a value with the nested structure populated is encoded then decoded,
   **Then** all levels of nesting are preserved with correct field values.
4. **Given** a registered structure type with an array field of known element count,
   **When** a value with the array populated is encoded then decoded, **Then** the
   array length and all element values are preserved.
5. **Given** a registered enumeration type, **When** an enumeration value is encoded
   then decoded, **Then** the decoded value matches the original enumeration value.
6. **Given** malformed binary input, **When** the decoder encounters truncated data
   or invalid field data, **Then** an appropriate error StatusCode is returned
   (Bad_DecodingError).

---

### User Story 2 - Audit Event Callback Dispatch (Priority: P2)

An application developer needs to receive notifications when security-relevant events
occur in the OPC UA server (e.g., session creation, secure channel opening) so they can
log, forward, or react to these events.

Currently `mu_raise_audit_event` is a no-op that discards both arguments. There is no
`mu_audit_callback_t` typedef and no callback registration API.

**Why this priority**: P2 — audit events are required for security-conscious deployments
and are mandated by OPC-10000-5 §6.5 for profiles that include auditing. The event
types and struct are already defined; only the dispatch mechanism is missing.

**Independent Test**: `ctest -R test_audit_events` — callback receives event with
correct fields, multiple callbacks fire, no-crash on NULL input.

**Acceptance Scenarios**:

1. **Given** a registered audit callback and a server with auditing enabled,
   **When** a security event occurs (e.g., CreateSession), **Then** the callback
   is invoked with an event structure containing the correct ActionTimeStamp,
   ServerId, ClientAuditEntryId, and event-type-specific fields.
2. **Given** multiple registered audit callbacks, **When** a security event occurs,
   **Then** all registered callbacks are invoked in registration order.
3. **Given** no registered audit callbacks, **When** a security event occurs,
   **Then** `mu_raise_audit_event` returns gracefully without side effects.
4. **Given** NULL server or event pointer, **When** `mu_raise_audit_event` is called,
   **Then** the function returns immediately without crashing.
5. **Given** a registered callback and a server with auditing disabled,
   **When** a security event occurs, **Then** the callback is NOT invoked.

---

### User Story 3 - Remaining OPC-10000-13 Aggregate Functions (Priority: P3)

An application developer using the standard server profile needs access to the full set
of OPC UA aggregate functions defined in OPC-10000-13 for data processing in
subscriptions, not just the 3 currently implemented (Average, Minimum, Maximum).

**Why this priority**: P3 — the 3 existing aggregates cover basic use cases.
The additional 39 aggregate functions needed for OPC-10000-13 compliance are required
for full aggregate support but are individually lower priority. Implementation is
gated behind `MUC_OPCUA_AGGREGATE_FULL` so it can be toggled off for size-constrained
builds.

**Independent Test**: `ctest -R test_aggregate_full` — each implemented aggregate
function has ≥1 test with known-input/expected-output vectors.

**Acceptance Scenarios**:

1. **Given** a monitored item configured with aggregate type Count and a series
   of data samples, **When** the processing interval elapses, **Then** the published
   aggregate value equals the number of samples received.
2. **Given** a monitored item configured with aggregate type DurationGood and a
   series of status-coded samples, **When** the processing interval elapses,
   **Then** the published aggregate reflects the total duration of samples with
   Good status.
3. **Given** a monitored item configured with aggregate type Range and numeric
   samples, **When** the processing interval elapses, **Then** the published
   aggregate equals (maximum - minimum) of the samples.
4. **Given** a monitored item configured with aggregate type PercentBad and mixed
   status samples, **When** the processing interval elapses, **Then** the published
   aggregate equals the percentage of samples with Bad status.
5. **Given** a monitored item configured with any aggregate type, **When** zero
   samples arrive within the processing interval, **Then** the published aggregate
   returns Bad_NoData.
6. **Given** a monitored item configured with any aggregate type, **When** all
   incoming samples are non-numeric, **Then** the published aggregate returns
   Bad_NoData.

---

### User Story 4 - Binary Size Measurement Tooling (Priority: P4)

A developer evaluating the library for a resource-constrained target needs accurate,
actionable binary size measurements that reflect actual flash usage after linker
garbage collection, not inflated archive-level estimates.

Currently `scripts/measure_size.sh` measures `.a` archive sizes, which overstate
actual flash usage by ~1.4 KB due to object-file overhead that `--gc-sections`
removes at final link.

**Why this priority**: P4 — improves development workflow and accuracy of size
tracking. Not a runtime feature, but critical for honest evaluation and regression
detection.

**Independent Test**: Run `scripts/measure_size.sh` and verify the output reports
linked-ELF sizes (not just archive sizes) with `.text`, `.data`, `.bss` breakdowns
per profile.

**Acceptance Scenarios**:

1. **Given** all build profiles (nano, micro, embedded, standard), **When**
   `scripts/measure_size.sh` is run, **Then** the output includes linked-ELF
   sizes per profile with `.text`/`.data`/`.bss` breakdowns.
2. **Given** the standard profile, **When** a CLF binary is linked with
   `--gc-sections`, **Then** the measured size is ≤ the archive size for the
   same profile (reflecting dead-code elimination).
3. **Given** any measurement run, **When** the script completes, **Then** a
   machine-readable report (CSV or JSON) is written alongside the human-readable
   output for automated tracking.
4. **Given** `MUC_OPCUA_LTO=ON`, **When** `measure_size.sh` is run, **Then**
   LTO is applied at the final link step and the size difference is reported.

---

### Edge Cases

- What happens when a structure type is registered with zero fields? Encode produces
  an empty byte stream; decode produces an empty value.
- What happens when an optional field's EncodingMask bit is set but the corresponding
  field value pointer is NULL? Decoder reports Bad_DecodingError.
- What happens when an array field has length 0? Encoder produces a zero-length array
  encoding; decoder reconstructs an empty array.
- What happens when a callback modifies the server state during audit dispatch?
  Callbacks receive a `const mu_audit_event_t *` and must not modify server state
  directly.
- What happens when an aggregate function receives samples with mixed data types
  (e.g., INT32 and DOUBLE)? Only numeric samples of compatible types are included;
  incompatible types are skipped.
- What happens when the cross-compiler toolchain is not installed?
  `measure_size.sh` reports profiles for which the compiler is available and skips
  unavailable targets with a clear message.
- How does the feature preserve application flash/RAM headroom? All new aggregate
  functions gate on `MUC_OPCUA_AGGREGATE_FULL` (OFF by default). Complex type
  encode/decode gates on `MUC_OPCUA_COMPLEX_TYPES`. Audit callbacks gate on
  `MUC_OPCUA_AUDITING`.

## Requirements *(mandatory)*

### Functional Requirements

**D1 — Complex Types:**
- **FR-001**: System MUST provide `mu_binary_encode_struct` that serializes a
  `mu_structure_definition_t` value into the OPC UA Binary encoding format.
- **FR-002**: System MUST provide `mu_binary_decode_struct` that deserializes
  OPC UA Binary encoded data back into a `mu_structure_definition_t` value.
- **FR-003**: Encoder MUST handle required scalar fields (Boolean, SByte, Byte,
  Int16, UInt16, Int32, UInt32, Int64, UInt64, Float, Double, String,
  DateTime, Guid, ByteString, NodeId, ExpandedNodeId, QualifiedName,
  LocalizedText, StatusCode).
- **FR-004**: Encoder MUST handle optional fields via EncodingMask — fields
  marked optional are only encoded when the corresponding EncodingMask bit is set.
- **FR-005**: Encoder MUST recursively handle nested structures to arbitrary depth.
- **FR-006**: Encoder MUST handle array fields with element count prefix.
- **FR-007**: Decoder MUST validate input data against the structure definition
  and return Bad_DecodingError on truncation, type mismatch, or invalid data.
- **FR-008**: System MUST provide `mu_binary_encode_enum` and
  `mu_binary_decode_enum` for enumeration type round-trips.
- **FR-009**: All complex type functions MUST gate on `MUC_OPCUA_COMPLEX_TYPES`.

**D2 — Audit Events:**
- **FR-010**: System MUST define `mu_audit_callback_t` typedef — a function pointer
  type taking `(struct mu_server *, const mu_audit_event_t *)`.
- **FR-011**: System MUST provide `mu_server_set_audit_callback()` to register a
  single audit callback on a server instance.
- **FR-012**: System MUST provide `mu_server_add_audit_callback()` to register
  an additional audit callback (supporting multiple callbacks).
- **FR-013**: `mu_raise_audit_event()` MUST dispatch to all registered callbacks
  in registration order when `auditing_enabled` is true.
- **FR-014**: `mu_raise_audit_event()` MUST return without calling any callbacks
  when `auditing_enabled` is false.
- **FR-015**: `mu_raise_audit_event()` MUST return immediately without side effects
  when passed a NULL server or NULL event pointer.
- **FR-016**: Audit event structs passed to callbacks MUST include populated
  ActionTimeStamp (time of event), ServerId, and ClientAuditEntryId fields.
- **FR-017**: All audit functions MUST gate on `MUC_OPCUA_AUDITING`.

**D3 — Aggregate Functions:**
- **FR-018**: System MUST implement the following aggregate functions from
  OPC-10000-13 §5.4.3, gated on `MUC_OPCUA_AGGREGATE_FULL`: Count, Delta,
  DeltaBounds, DurationGood, DurationBad, DurationInStateZero, End,
  Interpolative, Maximum2, Minimum2,
  PercentGood, PercentBad, Range, Start, TimeAverage, TimeAverage2, Total,
  Total2, WorstQuality, WorstQuality2, AnnotationCount.
- **FR-019**: Each aggregate function MUST accept numeric OPC UA built-in types
  (SByte, Byte, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float, Double)
  and produce the correct aggregate output type per OPC-10000-13.
- **FR-020**: Duration-aggregate functions (DurationGood, DurationBad,
  DurationInStateZero) MUST process StatusCode-coded samples and compute
  aggregate durations in milliseconds.
- **FR-021**: Each aggregate function MUST handle zero-sample processing intervals
  by returning Bad_NoData.
- **FR-022**: Each aggregate function MUST skip non-numeric samples without
  incrementing sample count.
- **FR-023**: Each aggregate function MUST support `processing_interval` gating —
  publish only when the interval has elapsed, then reset state.
- **FR-024**: The existing 3 aggregate functions (Average, Minimum, Maximum)
  MUST remain functional and unchanged.

**D4 — Binary Size Measurement:**
- **FR-025**: `scripts/measure_size.sh` MUST produce linked-ELF size measurements
  (not just `.a` archive measurements) for profiles where a cross-compiler is
  available.
- **FR-026**: The script MUST report `.text`, `.data`, `.bss` breakdown per profile
  using `arm-none-eabi-size` or equivalent.
- **FR-027**: The script MUST produce a machine-readable output file (JSON or CSV)
  alongside human-readable terminal output.
- **FR-028**: The script MUST report `MUC_OPCUA_LTO=ON` vs `OFF` size comparison
  when LTO is available.
- **FR-029**: The script MUST skip unavailable cross-compiler targets with a clear
  diagnostic message rather than failing.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA profile/conformance-unit assumption is the Standard
  Server Profile (OPC-10000-7) for aggregate functions, with individual features
  gated on their respective profile flags.
- **OPC-002**: Implemented services/features:
  - Complex type binary encoding per OPC-10000-6 §5.2 (Binary Encoding) and
    OPC-10000-3 §5.6.4 (StructureDefinition) and §5.6.5 (EnumDefinition).
  - Audit event dispatch per OPC-10000-5 §6.5.2 (EventType) and
    OPC-10000-5 §6.5.3 (AuditEventType).
  - Aggregate functions per OPC-10000-13 §4.2.2 (AggregateFunctionType),
    §5.4.3 (AggregateFilter), and §5.4.4 (AggregateConfiguration).
- **OPC-003**: Unsupported aggregate types (when `MUC_OPCUA_AGGREGATE_FULL` is OFF)
  return Bad_MonitoredItemFilterUnsupported per OPC-10000-4 §7.22.
- **OPC-004**: Wire encoding for complex types follows OPC-10000-6 §5.2.2.12
  (Structure) and §5.2.2.13 (Enumeration) Binary encoding rules.
- **OPC-005**: Conformance status is experimental for all features (tests added,
  no CTT verification). Audit and complex types are profile-gated. Aggregate
  functions target the AggregateServerBase conformance unit when
  `MUC_OPCUA_AGGREGATE_FULL` is enabled.

### Scope Boundaries *(mandatory)*

- **In Scope**:
  - D1: `mu_binary_encode_struct`, `mu_binary_decode_struct`, `mu_binary_encode_enum`,
    `mu_binary_decode_enum` in `src/encoding/binary_complex.c`
  - D2: `mu_audit_callback_t` typedef, callback registration API, dispatch from
    `mu_raise_audit_event`
  - D3: 21 new primary aggregate functions (excluding bounding variants; MaximumActualTime/MinimumActualTime already implemented via existing Maximum/Minimum), updated
    filter reader to accept new types when `MUC_OPCUA_AGGREGATE_FULL` is ON
  - D4: Updated `measure_size.sh` with linked-ELF output, JSON report, LTO comparison,
    toolchain-availability checks
  - Behavioral tests for all new code (D1: round-trip encode/decode, D2: callback
    dispatch, D3: per-function input/output vectors)
- **Out of Scope**:
  - Converting existing no-op registration functions into real implementations
    (they correctly return `MU_STATUS_GOOD` for type-system registration)
  - Bounding variant aggregate functions (BoundingValues, EndBound, StartBound,
    etc.) — deferred to follow-up
  - CTT compliance verification for any feature
  - Runtime audit event emission from service handlers (only the dispatch
    mechanism is in scope; hooking audit calls into service handlers is deferred)
  - Publish/subscribe integration for complex types (only binary encode/decode
    is in scope)
  - mDNS discovery (F1 from TODO.md)
- **Compatibility Claim**: Standard Server Profile with AggregateServerBase
  conformance unit when `MUC_OPCUA_AGGREGATE_FULL` is enabled; existing profiles
  (nano, micro, embedded, standard) continue to pass all tests with new features
  gated off.
- **Application Headroom Goal**: Zero flash/RAM impact on existing profiles when
  `MUC_OPCUA_AGGREGATE_FULL`, `MUC_OPCUA_COMPLEX_TYPES`, and `MUC_OPCUA_AUDITING`
  are OFF. When enabled, aggregate functions add ~2-4 KB flash; complex types
  add ~1-2 KB flash; audit callbacks add ~0.5 KB flash.

### Key Entities *(include if feature involves data)*

- **mu_structure_definition_t**: Describes a custom data type with fields
  (name, type, rank, is_optional). Used by encoder/decoder to drive serialization.
- **mu_enum_definition_t**: Describes an enumeration type with named integer values.
- **mu_audit_callback_t**: Function pointer type `void (*)(struct mu_server *,
  const mu_audit_event_t *)` for receiving audit notifications.
- **mu_audit_event_t**: Audit event structure with discriminator (event_type),
  union of event-specific payloads, ActionTimeStamp, ServerId, and
  ClientAuditEntryId.
- **Aggregate accumulator**: Per-monitored-item state tracking sample values,
  timestamps, status codes, and processing interval progress. Extended to
  support new aggregate types.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Round-trip encode→decode for all ≥8 OPC UA scalar types preserves
  original values with zero bit errors.
- **SC-002**: ≥6 new structure type test cases (scalar fields, optional fields,
  nested structures, arrays, empty structures, malformed input) pass on full and
  standard profiles.
- **SC-003**: Audit callback dispatch correctly invokes ≥2 registered callbacks
  in registration order when a security event occurs, verified by test.
- **SC-004**: ≥21 primary aggregate functions each have ≥1 test case with
  known-input/expected-output verification.
- **SC-005**: Binary size measurement produces linked-executable size measurements
  for ≥3 configuration profiles (micro, embedded, standard) with code/data/BSS
  breakdown and structured machine-readable output.
- **SC-006**: All 4 build profiles (nano, micro, embedded, standard) pass
  automated testing with zero regressions.
- **SC-007**: Existing tests for the 3 implemented aggregate functions, audit
  NULL-safety, and complex type registration continue to pass unchanged.

## Assumptions

- The cross-compiler toolchain (`arm-none-eabi-gcc`) is available in the CI
  environment for binary size measurement (D4).
- The 3 existing aggregate functions (Average, Minimum, Maximum) will not be
  refactored — new functions follow the same pattern and are added alongside.
- Audit event emission calls (`mu_raise_audit_event` invocations) will be added
  to service handlers in a follow-up spec; this spec only implements the dispatch
  mechanism and tests it directly.
- Bounding variant aggregate functions (e.g., StartBound, EndBound) are out of
  scope due to their dependency on the primary function implementations.
- The existing `mu_audit_event_t` struct with 4 union members is sufficient for
  initial callback dispatch; additional event types may be added later.
- Complex type encode/decode uses the type-system information stored during
  `mu_register_structure_type` / `mu_register_enumeration_type` registration.
- The user has a `grep`/`awk`/`sed` capable shell for running `measure_size.sh`.
