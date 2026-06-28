# Feature Specification: Optional Attribute Write Service

**Feature Branch**: `007-optional-write-service`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement optional Attribute Write service for Value only with scalar values and callback"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Basic Write for Scalar Values (Priority: P1)

Allows an external OPC UA client to modify a read-write variable's value on the server by sending a WriteRequest containing a single scalar value. The server validates the request and forwards the new value to an application-supplied write callback for validation and custom storage.

**Why this priority**: Core MVP requirement to enable control capabilities (sending setpoints, toggling states) for resource-constrained microcontrollers.

**Independent Test**: Can be tested independently by issuing a single-item WriteRequest for a known variable NodeId (e.g. dynamic state) and verifying the write callback is triggered with the correct value, returning a success result.

**Acceptance Scenarios**:

1. **Given** a server configured with a write callback, **When** a client writes a valid scalar Int32 to a variable NodeId, **Then** the write callback is called with the NodeId and the new Int32 value, and the client receives a success status code (`Good`).
2. **Given** a server configured with a write callback, **When** a client writes a value but the write callback returns `Bad_OutOfRange`, **Then** the client receives the `Bad_OutOfRange` status code in the WriteResponse.

---

### User Story 2 - Batch Write Support (Priority: P2)

Allows writing multiple variables in a single batch WriteRequest, returning independent status codes for each write item in the response.

**Why this priority**: Performance optimization to allow clients to update multiple parameters/setpoints in a single network round-trip.

**Independent Test**: Test by writing to a list of two nodes in a single request, verifying both callbacks/status results are returned in order.

**Acceptance Scenarios**:

1. **Given** a batch WriteRequest with two nodes, **When** the first write is valid and the second write is to an unknown NodeId, **Then** the first result returns `Good` and the second returns `Bad_NodeIdUnknown` in the results array.

---

### User Story 3 - Write Constraints and Error Handling (Priority: P3)

Ensures that the server correctly rejects out-of-scope write requests (non-Value attributes, index ranges, or disabled write service) with spec-mandated StatusCodes.

**Why this priority**: Secures the server and ensures conformance by preventing invalid operations.

**Independent Test**: Test by sending WriteRequests targeting other attribute IDs, array index ranges, or when the Write service flag is disabled, verifying correct error code propagation.

**Acceptance Scenarios**:

1. **Given** a client sending a WriteRequest, **When** the target attribute is `BrowseName` (3) instead of `Value` (13), **Then** the server returns `Bad_AttributeIdInvalid`.
2. **Given** a client sending a WriteRequest with `indexRange` specified, **When** the server does not support partial array writes, **Then** the server returns `Bad_WriteNotSupported`.
3. **Given** the server compiled with `MICRO_OPCUA_SERVICE_WRITE=OFF`, **When** any WriteRequest is received, **Then** the server returns `Bad_ServiceUnsupported`.

### Edge Cases

- **Malformed request buffer**: Handled by the message chunk decoder; if the array length of `nodesToWrite` is invalid, the chunk is rejected with `Bad_DecodingError`.
- **Empty `nodesToWrite` array**: Returns `Bad_NothingToDo` service-level error (OPC-10000-4 §5.11.4.3).
- **Type mismatch**: If the datatype of the written variant does not match the expected type of the node, the server returns `Bad_TypeMismatch`.
- **NULL callback**: If no write callback is registered in the config, all writes return `Bad_NotWritable`.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The server MUST support the Write service gated behind the CMake build flag `MICRO_OPCUA_SERVICE_WRITE` (default `OFF` for nano/micro, `ON` for new `embedded-write` profile).
- **FR-002**: The server MUST only accept writes targeting `AttributeId_Value` (13). Writes to other attributes MUST return `Bad_AttributeIdInvalid`.
- **FR-003**: The server MUST delegate the write operation to a caller-provided callback:
  `opcua_statuscode_t (*write_callback)(void *handle, const opcua_nodeid_t *node_id, const opcua_variant_t *value)`
  registered in `mu_server_config_t`.
- **FR-004**: If the write callback is `NULL`, the server MUST fail all writes with `Bad_NotWritable`.
- **FR-005**: The server MUST parse the incoming `DataValue` structure, but only process the `value` field. Source and server timestamps are ignored.
- **FR-006**: The server MUST reject write attempts containing a non-empty `indexRange` with `Bad_WriteNotSupported`.
- **FR-007**: The server MUST perform type checking: if the node has a defined datatype, the written variant type MUST match or be compatible, otherwise returning `Bad_TypeMismatch`.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target profile: targets the **Write Value Server Facet** (OPC-10000-7 §6.6.6) for scalar attributes only.
- **OPC-002**: Implemented service: **Write** (OPC-10000-4 §5.11.4).
- **OPC-003**: If `MICRO_OPCUA_SERVICE_WRITE=OFF`, the Write service returns `Bad_ServiceUnsupported` (OPC-10000-4 §5.11.4.3).
- **OPC-004**: Wire encoding: complies with the binary serialization of `WriteRequest` (node type encoding ID 675) and `WriteResponse` (node type encoding ID 678) in OPC-10000-6.

### Scope Boundaries *(mandatory)*

- **In Scope**:
  - `Write` service parsing and execution.
  - Scalar values for standard types (Boolean, Int32, UInt32, Float, String, DateTime).
  - Integration write callback in `mu_server_config_t`.
- **Out of Scope**:
  - Writing attributes other than `Value` (e.g. `DisplayName`, `BrowseName`).
  - Array writes, index ranges, or composite structure writes.
  - Mutable internal node storage managed by the library (values must be stored/managed by the application).
- **Compatibility Claim**: Adds the optional **Write Value Server Facet** to the existing profile capability list.
- **Application Headroom Goal**: The code addition should be less than **1.5 KB** of Flash, and add **0 bytes** of static RAM (BSS) since all state is transient in the dispatch path.

### Key Entities

- **WriteValue**: Encapsulates a NodeId, AttributeId, indexRange, and DataValue.
- **WriteCallback**: Opaque callback registered at server init to handle the update of the backing variable.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of valid WriteRequests trigger the application callback and update the target node.
- **SC-002**: 100% of out-of-scope write requests (invalid AttributeId, indexRange) fail with the exact spec-cited StatusCode.
- **SC-003**: The addition increases host BSS by exactly 0 bytes and ARM Flash by under 1.5 KB.
- **SC-004**: Interoperability test suites using `asyncua` and `.NET` reference clients successfully execute write/readback loops.

## Assumptions

- Backing values are kept by the application, and the existing `mu_value_source_t` callback mechanism is used to read them back.
- No database or persistent storage is managed internally by the core library.
