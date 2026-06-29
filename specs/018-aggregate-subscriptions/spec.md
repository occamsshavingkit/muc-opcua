# Feature Specification: Aggregate Subscriptions

**Feature Branch**: `018-aggregate-subscriptions`  
**Created**: 2026-06-29  
**Status**: Draft  
**Input**: User description: "Feature 018: Aggregate Subscriptions. Support for calculating averages, mins, maxes over time periods during publishing."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Average Aggregate Subscription (Priority: P1)

As an OPC UA client, I want to subscribe to a variable's average value calculated over a specified processing interval so that I can monitor a smoothed version of the data without high-frequency noise.

**Why this priority**: Averages are the most common aggregate type used to downsample and smooth sensor data in industrial monitoring.

**Independent Test**: Can be tested by creating a MonitoredItem with an AggregateFilter specifying the Average aggregate type and a 5-second processing interval, simulating variable value changes on the server, and verifying that the published value matches the average of the inputs over that interval.

**Acceptance Scenarios**:

1. **Given** a variable node, **When** a client creates a MonitoredItem with an `AggregateFilter` for `Average` and a 5000ms processing interval, **Then** the server publishes the average value of the variable every 5000ms.
2. **Given** a MonitoredItem with an `Average` aggregate filter, **When** the variable changes value multiple times during the interval, **Then** the value published at the end of the interval is the correct mathematical mean of those values.

---

### User Story 2 - Min/Max Aggregate Subscriptions (Priority: P2)

As an OPC UA client, I want to subscribe to a variable's minimum and maximum value calculated over a specified processing interval so that I can track the signal bounds.

**Why this priority**: Min/Max tracking is essential for detecting peaks, valleys, and bounds in engineering signals.

**Independent Test**: Can be tested by creating MonitoredItems with `AggregateFilter` for Min and Max, changing values on the server, and verifying that the minimum and maximum values are correctly reported at the end of each interval.

**Acceptance Scenarios**:

1. **Given** a MonitoredItem with a `Minimum` aggregate filter, **When** the variable changes through multiple values during the interval, **Then** the server publishes the lowest value recorded during that interval.
2. **Given** a MonitoredItem with a `Maximum` aggregate filter, **When** the variable changes through multiple values during the interval, **Then** the server publishes the highest value recorded during that interval.

---

### Edge Cases

- What happens if no value changes occur during the processing interval? The server should publish the last known value, or a Status of Good/NoData depending on the aggregate configuration.
- What happens if the variable has a non-numeric type? The server should return `Bad_FilterNotAllowed` during MonitoredItem creation.
- How does the system handle an unsupported aggregate type? The server returns `Bad_AggregateListMismatch` or `Bad_MonitoredItemFilterUnsupported`.
- How does the feature preserve application flash/RAM headroom on the target microcontroller class? All monitored item state (including running accumulation variables) must be stored within the existing pre-allocated MonitoredItem slot in the server, using no heap memory.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST support the `AggregateFilter` type inside the `CreateMonitoredItems` service.
- **FR-002**: System MUST calculate and publish the mathematical `Average` of a variable's values over the `processingInterval` when requested.
- **FR-003**: System MUST calculate and publish the `Minimum` of a variable's values over the `processingInterval` when requested.
- **FR-004**: System MUST calculate and publish the `Maximum` of a variable's values over the `processingInterval` when requested.
- **FR-005**: All aggregate calculations and state tracking MUST run without dynamic heap allocations, using statically bounded structures or running accumulators.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA scope is the scoped MonitoredItem `AggregateFilter` behavior defined by OPC-10000-4 §7.22.4 and standard AggregateFunction objects from OPC-10000-13.
- **OPC-002**: Implemented filters are `AggregateFilter` (OPC-10000-4 §7.22.4). Supported namespace 0 NodeIds are `Average` (`2342`, OPC-10000-13 §4.2.2.4), `Minimum` (`2346`, OPC-10000-13 §4.2.2.9), and `Maximum` (`2347`, OPC-10000-13 §4.2.2.10).
- **OPC-003**: Unsupported aggregate types or configurations return `Bad_MonitoredItemFilterUnsupported` or `Bad_FilterNotAllowed`.
- **OPC-004**: Wire encoding uses standard binary representations of `AggregateFilter` and `AggregateConfiguration`.

### Scope Boundaries *(mandatory)*

- **In Scope**: `AggregateFilter` support for `Average`, `Minimum`, and `Maximum` on numeric variables. Bounded running calculators stored within monitored item instances.
- **Out of Scope**: Historical aggregates (aggregates queried via HistoryRead - this is part of Feature 017 but is a separate enterprise profile). Complex aggregates like StandardDeviation, Variance, or custom configurations are out of scope.
- **Compatibility Claim**: Profile-targeting scoped MonitoredItem AggregateFilter support for Average, Minimum, and Maximum; not CTT-verified.
- **Application Headroom Goal**: The implementation must add <2KB of flash and 0 bytes of heap memory. MonitoredItem structure footprint growth must be limited to <32 bytes.

### Key Entities *(include if feature involves data)*

- **AggregateFilter**: MonitoredItem filter parameters defining the processing interval, aggregate type NodeId, and configuration.
- **MonitoredItem Aggregate State**: Statically allocated running variables (accumulator, count, min/max values, last update timestamp) associated with a monitored item instance to compute aggregates over time.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A client can subscribe to a variable's 5-second Average, Minimum, and Maximum and receive correct updates every 5 seconds.
- **SC-002**: Non-numeric or unsupported variables return `Bad_FilterNotAllowed` when an aggregate filter is applied.
- **SC-003**: System memory audits verify zero heap allocations are performed during publishing or aggregate calculations.
- **SC-004**: Unit tests confirm correct aggregate calculations for Average, Min, and Max over multiple sample periods.

## Assumptions

- The sampling interval of the monitored item is used to poll/sample values to include in the aggregate calculation.
- The client provides a valid numeric variable NodeId for aggregate subscriptions.
- If a client requests an unsupported sampling/processing interval combination, the server may round the interval to the nearest supported granularity.
