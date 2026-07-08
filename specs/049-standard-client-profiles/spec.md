# Feature Specification: Standard UA Client 2025 Profile

**Feature Branch**: `049-standard-client-profile`  
**Created**: 2026-07-07  
**Status**: Draft  
**Input**: "Implement the rest of the client profiles from Part 7 of the OPC-UA standard"

## User Scenarios & Testing

### Existing Baseline: Minimum UA Client Profile

The Minimum UA Client Profile (`/Client/Minimum`, UACore 1.04) is already implemented
via the nano client (spec 047). It provides: TCP connection, secure channel,
session management, discovery (GetEndpoints, FindServers), browse, and read.

### Current Scope: Standard UA Client 2025 Profile

The Standard UA Client 2025 Profile (`/Client/Standard2025`, UACore 1.05) supersedes
the Standard UA Client 2022 Profile. It aggregates the Core 2022 Client Facet plus
additional facets for subscriptions, write, method calls, events, and data access.
Per OPC-10000-7 §4.8, a Standard UA Client shall support local, subnet, and global
discovery, maintain a single session, and fall back to Read when a server does
not support subscriptions.

The specific facets composing the Standard UA Client 2025 Profile are defined in
the profile table in OPC-10000-7 §6.6 (not available via the reference search tool).
Based on the registry URI listing and Part 7 §4.8, the known required capabilities
include:

- Core 2022 Client Facet (existing baseline)
- DataChange Subscriber — subscribe to data changes on server variables
- Attribute Write — write values to server nodes
- Method Call — invoke methods on server objects
- Base Event Processing — receive and process event notifications
- Data Access — handle DA-specific variable metadata

### User Story 1 — Subscribe to Data Changes

An operator using the OPC UA client needs to monitor server variables for value
changes without polling. The client creates a subscription, adds monitored items
with data change filters, and receives notifications when values change.

**Independent Test**: Client creates a subscription on a server with a monitored
item, the server value changes, and the client receives a data change notification
containing the new value and status code within one publishing interval.

**Acceptance Scenarios**:

1. **Given** a connected and activated client, **When**
   `mu_client_create_subscription` is called with a publishing interval, **Then**
   a subscription is created and the client receives Publish responses.
2. **Given** an active subscription, **When** `mu_client_create_monitored_item`
   is called for a server variable with a data change filter, **Then** data
   change notifications are received when the value changes.
3. **Given** a monitored item with configured queue size, **When** the value
   changes faster than the client can consume, **Then** the queue stores samples
   according to `discard_oldest` policy.
4. **Given** received notifications, **When** the client sends the next Publish
   request with the acknowledged sequence number, **Then** the server stops
   retransmitting that notification.

### User Story 2 — Write Values

An application needs to set values on remote server nodes — for example,
configuration parameters or control setpoints.

**Independent Test**: Client writes a known value to a server variable via the
Write service, then reads it back and confirms the value was applied.

**Acceptance Scenarios**:

1. **Given** a connected client, **When** `mu_client_write` is called with a
   valid node ID, attribute ID, and value, **Then** the server applies the
   write and returns `Good`.
2. **Given** a write to a non-existent node, **When** the client sends the
   request, **Then** the server returns `Bad_NodeIdUnknown`.
3. **Given** a write with an incorrect data type, **When** the client sends
   the request, **Then** the server returns `Bad_TypeMismatch`.

### User Story 3 — Call Methods

An application needs to invoke methods on server objects — for example,
`GetMonitoredItems`, `ResendData`, or companion-spec-defined methods.

**Independent Test**: Client calls a method on the server with known input
arguments and verifies the output arguments match expectations.

**Acceptance Scenarios**:

1. **Given** a connected client, **When** `mu_client_call` is called with a
   method node ID, object node ID, and input arguments, **Then** the server
   executes the method and returns output arguments.
2. **Given** a call to a non-existent method, **When** the client sends the
   request, **Then** the server returns `Bad_NothingToDo`.

### User Story 4 — Receive Events

An application needs to be notified of server events — alarms, audit events,
or system events — via subscriptions with event filters.

**Independent Test**: Client subscribes to events on a server object, an event
is raised server-side, and the client receives an event notification containing
the selected fields.

**Acceptance Scenarios**:

1. **Given** a connected client with a subscription, **When**
   `mu_client_create_monitored_item` is called with an event filter containing
   a `SelectClause`, **Then** event notifications matching the filter are
   received with only the selected fields present.
2. **Given** an event subscription with a `WhereClause`, **When** events that
   do not match the filter occur on the server, **Then** those events are not
   delivered to the client.
3. **Given** a `BaseEventType` notification, **When** the client processes it,
   **Then** the event fields (time, severity, message, source node) are
   extracted and available to the application.

### Edge Cases

- Client disconnect during active subscription: all subscriptions are implicitly
  deleted when the session closes. Client must re-create on reconnect.
- Server does not support subscriptions: client must fall back to Read,
  per Standard UA Client Profile requirements (OPC-10000-7 §4.8).
- Publish request timeout: client should retry — parked Publish requests are
  normal in the OPC UA protocol.
- Method output arguments exceed receive buffer: return
  `Bad_EncodingLimitsExceeded`.
- Server returns `Bad_UserAccessDenied` for Write/Call: client propagates the
  status code to the application.
- Event notification with an unsupported `SelectClause` field: the field is
  omitted from the parsed result.

## Requirements

### Functional Requirements

- **FR-001**: Client MUST provide `mu_client_create_subscription` to create a
  subscription via `CreateSubscription` request (OPC-10000-4 §5.14.2).
- **FR-002**: Client MUST provide `mu_client_delete_subscription` to delete a
  subscription via `DeleteSubscriptions` request (§5.14.8).
- **FR-003**: Client MUST provide `mu_client_create_monitored_item` to create
  a monitored item via `CreateMonitoredItems`, supporting the `DataChangeFilter`
  and `EventFilter` filter types (§5.13.2).
- **FR-004**: Client MUST provide `mu_client_delete_monitored_item` to delete
  a monitored item (§5.13.6).
- **FR-005**: Client MUST park `Publish` requests and process incoming
  `PublishResponse` messages containing `DataChangeNotification` (§5.14.5)
  and `EventNotificationList` (§5.14.5.2).
- **FR-006**: Client MUST acknowledge received sequence numbers in subsequent
  `Publish` requests.
- **FR-007**: Client MUST buffer notifications per monitored item queue
  configuration (`queue_size`, `discard_oldest`) (§5.13.2.2).
- **FR-008**: Client MUST provide `mu_client_write` to write values via the
  `Write` service, accepting `WriteValue` items and returning per-item status
  codes (§5.11.4).
- **FR-009**: Client MUST provide `mu_client_call` to invoke methods via the
  `Call` service, accepting input arguments and returning output arguments
  with per-method status codes (§5.12.2).
- **FR-010**: Client MUST process `EventNotificationList` and extract event
  field values according to the active `SelectClause` (§5.13.2).
- **FR-011**: All new client features MUST gate on `MUC_OPCUA_CLIENT_PROFILE`
  with at minimum `nano` (existing) and `standard` (new) levels.
- **FR-012**: The nano client profile (`MUC_OPCUA_CLIENT_PROFILE=nano`) MUST
  continue to build and pass all existing tests unchanged when the standard
  profile is not enabled.

### OPC UA Normative Scope

- **OPC-001**: Targets Standard UA Client 2025 Profile (`/Client/Standard2025`,
  UACore 1.05) per OPC-10000-7 §4.8.
- **OPC-002**: Core 2022 Client Facet (`/Client/Core2022`) — existing baseline
  (nano client).
- **OPC-003**: Subscription and monitored item services per OPC-10000-4 §5.13–5.14.
- **OPC-004**: Write service per OPC-10000-4 §5.11.4.
- **OPC-005**: Method Call service per OPC-10000-4 §5.12.2.
- **OPC-006**: Event and notification encoding per OPC-10000-6 §5.2.2.17
  (DataValue), §5.2.2.15 (ExtensionObject), OPC-10000-5 §6.4.2 (EventEventType).
- **OPC-007**: Conformance status experimental — no CTT verification included.

### Scope Boundaries

- **In Scope**: Client subscription management, data change notifications, event
  notifications, write service, method call service. All gated on
  `MUC_OPCUA_CLIENT_PROFILE=standard`.
- **Out of Scope**: Client-side Historical Access (separate facet), Alarms and
  Conditions client, Redundancy, PubSub Subscriber, Reverse Connect, Complex
  Types registration on the client, Query.
- **Compatibility**: Nano profile (`MUC_OPCUA_CLIENT_PROFILE=nano`) continues
  to build and pass unchanged.

## Success Criteria

- **SC-001**: A standard-profile test client creates a subscription, adds a
  monitored item with data change filter, receives ≥5 successive notifications,
  and verifies values match server state.
- **SC-002**: A test client writes a value to a server variable, reads it back,
  and confirms the value matches.
- **SC-003**: A test client calls a method and verifies the output arguments.
- **SC-004**: A test client subscribes to events with a `SelectClause` and
  verifies selected fields are present in received event notifications.
- **SC-005**: The standard client profile builds without errors and all existing
  server-side tests continue to pass.
- **SC-006**: The nano client profile builds without errors and all existing
  nano client tests continue to pass.

## Key Entities

- **mu_client_subscription_t**: Client-side subscription state (id, publishing
  interval, monitored item list, publish request management, ack tracking).
- **mu_client_monitored_item_t**: Client-side monitored item state (id, attribute,
  sampling interval, queue configuration, filter type, notification buffer).
- **mu_client_publish_manager_t**: Client-side Publish request lifecycle (parked
  requests, notification dispatch, sequence number tracking).
