# Feature Specification: Historical Access (HA)

**Feature Branch**: `017-historical-access`  
**Created**: 2026-06-29  
**Status**: Draft  
**Input**: User description: "Implement Historical Access (HA). Implement HistoryRead (Raw and Modified). Implement HistoryUpdate (Insert/Replace/Delete). Provide a platform-agnostic persistence adapter interface (e.g., to write to SD card, flash, or host filesystem)."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read Historical Data (Priority: P1)

As an OPC UA client, I want to query a history of a variable's past values so I can display trends and perform analysis.

**Why this priority**: Reading history is the most common and critical feature of the HA specification. Without read capabilities, there is no historical access.

**Independent Test**: Can be tested by pre-populating historical data via the backend adapter, then having a client call `HistoryRead` and verifying the data points.

**Acceptance Scenarios**:

1. **Given** a variable node with `HistoryRead` enabled, **When** a client calls `HistoryRead` for Raw data with a valid time range, **Then** the server returns the historical data values within the range.
2. **Given** a variable node with `HistoryRead` enabled, **When** a client calls `HistoryRead` for Modified data, **Then** the server returns the historical modified data values with their update history.

---

### User Story 2 - Update Historical Data (Priority: P2)

As an OPC UA client, I want to insert, replace, or delete historical data points so I can correct erroneous readings or backfill data.

**Why this priority**: Secondary to reading, updating history allows clients to manage the historical store dynamically.

**Independent Test**: Can be tested by having a client call `HistoryUpdate` to insert data, and verifying the data is present in the store via `HistoryRead`.

**Acceptance Scenarios**:

1. **Given** a variable node with `HistoryUpdate` enabled, **When** a client calls `HistoryUpdate` to Insert a data value, **Then** the value is stored and becomes accessible via `HistoryRead`.
2. **Given** an existing data point, **When** a client calls `HistoryUpdate` to Replace it, **Then** the new value overwrites the old, and the modification is logged (for Modified read).
3. **Given** an existing data point, **When** a client calls `HistoryUpdate` to Delete it, **Then** it is removed from the store and no longer returned by `HistoryRead`.

---

### User Story 3 - Platform-Agnostic Persistence (Priority: P3)

As an embedded developer using the server, I want a platform-agnostic persistence adapter interface so I can store historical data in a backend suitable for my hardware (e.g., SD card, flash memory, host filesystem).

**Why this priority**: Required for embedded devices where memory is strictly constrained and non-volatile storage is heterogeneous.

**Independent Test**: Can be tested by implementing an in-memory or filesystem mock adapter and verifying `HistoryRead` and `HistoryUpdate` work correctly through the abstraction layer.

**Acceptance Scenarios**:

1. **Given** the server is configured with a custom history persistence adapter, **When** history operations occur, **Then** the server routes the storage reads and writes through the adapter callbacks without assuming filesystem specifics.

---

### Edge Cases

- What happens when a `HistoryRead` requests more data than fits in a single `MessageChunk`? (Should use continuation points).
- How does the system handle a `HistoryUpdate` when the persistence backend is full? Returns `Bad_OutOfMemory` or a specific HA error.
- What happens when malformed timestamps or invalid update types are sent?
- How does the feature preserve application flash/RAM headroom on the target microcontroller class? The adapter interface must remain minimal, and history operations should not allocate heap memory (using chunked iteration instead).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST support the `HistoryRead` service for `ReadRawModifiedDetails` (Raw and Modified).
- **FR-002**: System MUST support the `HistoryUpdate` service for `UpdateDataDetails` (Insert, Replace, Update, DeleteRawModified).
- **FR-003**: System MUST define a `mu_history_adapter_t` struct with function pointers for reading, inserting, replacing, and deleting historical records.
- **FR-004**: `HistoryRead` MUST handle pagination using ContinuationPoints if the result exceeds `max_message_size`.
- **FR-005**: All History operations MUST adhere strictly to the zero-heap constraints of the project.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA role is the History Server Facet (OPC 10000-7).
- **OPC-002**: Implemented services are `HistoryRead` (OPC 10000-4, 5.10.3) and `HistoryUpdate` (OPC 10000-4, 5.10.4).
- **OPC-003**: Unsupported HistoryReadDetails (like Event, Processed, AtTime) return `Bad_HistoryOperationUnsupported`.
- **OPC-004**: Wire encoding uses Binary encoding.

### Scope Boundaries *(mandatory)*

- **In Scope**: `HistoryRead` (Raw and Modified), `HistoryUpdate` (Insert/Replace/Delete), persistence adapter definition.
- **Out of Scope**: `HistoryRead` for Events, Processed (Aggregates - this is Feature 018), and AtTime. Complex annotation updates are out of scope.
- **Compatibility Claim**: History Server Facet (partial/embedded variant).
- **Application Headroom Goal**: History adapter struct and service dispatch should add minimal static memory overhead (<1KB RAM). History buffers will be constrained by the global maximum message size.

### Key Entities *(include if feature involves data)*

- **Historical Data Point**: A tuple of (NodeId, SourceTimestamp, ServerTimestamp, StatusCode, Value).
- **Continuation Point**: An opaque identifier used to paginate large historical queries without keeping state on the heap.
- **Persistence Adapter (`mu_history_adapter_t`)**: Interface providing callbacks for storage operations.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Client can successfully query 1000 historical data points using `HistoryRead` and continuation points.
- **SC-002**: Client can successfully insert, replace, and delete data points using `HistoryUpdate`.
- **SC-003**: A mock platform adapter is provided in the test suite that stores data in an in-memory circular buffer.
- **SC-004**: Memory profiling confirms no dynamic heap allocations are made during history queries or updates.
- **SC-005**: Unsupported historical operations return correct `Bad_*` status codes.

## Assumptions

- Clients support continuation points for large historical reads.
- The underlying persistence layer (implemented by the user) is responsible for handling storage failures or media degradation.
- Historical data points are stored in a format determined by the platform adapter; the server only passes variants and timestamps to the adapter.
