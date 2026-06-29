# Feature Specification: Advanced Alarms & Conditions

**Feature Branch**: `016-advanced-alarms-and-conditions`  
**Created**: 2026-06-29  
**Status**: Draft  
**Input**: User description: "Expand the basic event system to support Acknowledgeable Conditions, Active/Inactive states, and Dialogs."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Acknowledgeable Condition State Transitions (Priority: P1)

As an OPC UA client operator, I want to receive notifications when an alarm enters an Active state, and I want to be able to acknowledge it so that other operators know the issue is being handled.

**Why this priority**: State transitions (Active/Inactive and Acknowledged/Unacknowledged) form the core of industrial alarm management.

**Independent Test**: Can be independently tested by generating an active alarm on the server, having a client subscribe to events, and calling the `Acknowledge` method to observe the state transition.

**Acceptance Scenarios**:

1. **Given** an alarm is inactive, **When** a process condition causes it to become active, **Then** an event is fired with `ActiveState=True` and `AckedState=False`.
2. **Given** an unacknowledged alarm, **When** a client calls the `Acknowledge` method with a valid condition ID, **Then** the alarm state transitions to `AckedState=True` and an event is published reflecting this change.

---

### User Story 2 - Dialog Conditions (Priority: P2)

As a server process, I want to present a Dialog Condition to clients when user input (e.g., confirmation) is required to proceed, so that critical actions are explicitly approved.

**Why this priority**: Dialogs allow the server to pause and request operator feedback, expanding the capabilities of the system beyond simple notification.

**Independent Test**: Can be tested by triggering a Dialog Condition on the server, verifying the client receives it, and having the client call `Respond` to answer the dialog.

**Acceptance Scenarios**:

1. **Given** a server requires confirmation, **When** it activates a Dialog Condition, **Then** the condition state becomes Active and a Dialog Event is published.
2. **Given** an active Dialog Condition, **When** a client calls the `Respond` method with a valid response, **Then** the Dialog Condition becomes Inactive and processing continues.

---

### Edge Cases

- What happens when a client tries to `Acknowledge` an already acknowledged condition?
- How does the system handle an invalid or expired `ConditionId` when `Acknowledge` or `Respond` is called, and which OPC UA StatusCode is returned?
- What happens when the server runs out of memory for keeping track of active condition states?
- How does the feature preserve application flash/RAM headroom on the target microcontroller class? (Must use statically allocated condition instances).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST support `AcknowledgeableConditionType` and its state variables (`AckedState`, `ConfirmedState`).
- **FR-002**: System MUST support `AlarmConditionType` and its state variables (`ActiveState`).
- **FR-003**: System MUST expose the `Acknowledge` and `Confirm` methods for Acknowledgeable Conditions.
- **FR-004**: System MUST support `DialogConditionType`.
- **FR-005**: System MUST expose the `Respond` method for Dialog Conditions.
- **FR-006**: System MUST statically allocate the memory for all conditions and state trackers to adhere to the zero-heap policy.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA conformance is Part 9 (Alarms and Conditions).
- **OPC-002**: Implemented features include `AcknowledgeableConditionType` (Part 9, 5.5), `AlarmConditionType` (Part 9, 5.8), and `DialogConditionType` (Part 9, 5.7).
- **OPC-003**: Unsupported features or malformed method calls return standard Part 4 status codes (e.g., `Bad_ConditionAlreadyAcked`, `Bad_ConditionInvalid`).
- **OPC-004**: Method Calls for Acknowledge and Respond are encoded as standard `Call` service requests (Part 4, 5.8).
- **OPC-005**: SecurityPolicy and conformance status are profile-targeting until CTT evidence exists.

### Scope Boundaries *(mandatory)*

- **In Scope**: `AcknowledgeableConditionType`, `AlarmConditionType` (ActiveState, AckedState), `DialogConditionType`, Method execution for `Acknowledge` and `Respond`.
- **Out of Scope**: Exclusive/Non-Exclusive Limit Alarms, Condition refresh for clients that connect after an alarm is active (unless trivially implemented).
- **Compatibility Claim**: Alarms & Conditions Facet subset.
- **Application Headroom Goal**: Negligible RAM overhead per condition (e.g., < 32 bytes per condition for state tracking), preserving >90% headroom on standard target.

### Key Entities *(include if feature involves data)*

- **Condition State**: Tracks `ActiveState`, `AckedState`, `ConfirmedState`, and `Retain` for a specific condition instance.
- **Dialog Context**: Tracks expected responses and valid option masks for a specific dialog.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Client can successfully Acknowledge an active condition and receive the state change event.
- **SC-002**: Client can successfully Respond to a Dialog Condition and the server registers the response.
- **SC-003**: A maximum of 10 conditions can be statically tracked without exceeding 1KB of RAM overhead.
- **SC-004**: All invalid method calls (e.g., acknowledging an invalid condition) return correct `Bad_*` status codes.
- **SC-005**: Default example remains below agreed flash and RAM limits with documented headroom.

## Assumptions

- Clients support OPC UA Method Calls and Event Subscriptions.
- `AcknowledgeableConditionType` state transitions will be handled explicitly by server application logic using API helpers, rather than a hidden background thread.
- Node IDs for methods like `Acknowledge` are shared among instances of the same condition type, or properly instantiated per instance as per Part 9. (Assuming standard Part 9 modeling where methods belong to the condition type).
