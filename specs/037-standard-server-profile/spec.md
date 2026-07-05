# Feature Specification: Implement Standard 2017 UA Server Profile

**Feature Branch**: `037-standard-server-profile`  
**Created**: 2026-07-05  
**Status**: Draft  
**Source**: Gap analysis against OPC-10000-7 §6.6 (Standard 2017 UA Server Profile, profile ID 1663)  
**Profile URI**: `http://opcfoundation.org/UA-Profile/Server/StandardUA2017`

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Data Access Server Facet (Priority: P1)

An OPC UA Client reads an AnalogItem Variable from the server and retrieves
engineering metadata such as EURange and EngineeringUnits. The client creates a
MonitoredItem with a percent deadband filter and receives only data changes that
exceed the configured percentage of the EURange span. The client browses the
address space and sees AnalogItemType nodes with InstrumentRange,
EngineeringUnits, and EURange Properties.

**Why this priority**: Data Access is the core value proposition that
distinguishes the Standard profile from Embedded. Without it, clients cannot
interpret analog data semantically or filter efficiently by percent-of-range.

**Independent Test**: Can be tested by creating an AnalogItem variable with
EURange and EngineeringUnits properties, creating a MonitoredItem with percent
deadband filter, changing the value by amounts above and below the deadband
threshold, and verifying only qualifying changes are published.

**Acceptance Scenarios**:

1. **Given** an AnalogItem with EURange [0,100] and EURange, EngineeringUnits
   Properties, **When** a client reads the variable's EURange Property,
   **Then** the correct range structure is returned.
2. **Given** a deadband filter with deadbandType=Percent and value=10 on
   EURange [0,100], **When** value changes by 5 units, **Then** no
   notification is sent. **When** value changes by 15 units, **Then** a
   notification is sent.
3. **Given** a deadband filter with deadbandType=Percent, **When** no EURange
   is defined on the variable, **Then** Bad_DeadbandFilterInvalid is returned
   (per OPC-10000-4 §7.22.2).

---

### User Story 2 — Method Server Facet (Priority: P1)

An OPC UA Client discovers custom Methods on Objects in the server address space
and invokes them via the Call service. The server dispatches to
application-registered callbacks and returns output arguments. Methods beyond the
two built-in Base Info methods (GetMonitoredItems, ResendData) are supported.

**Why this priority**: Methods are required by the Standard profile and every
companion specification (DI, ADI, PLCopen, etc). Without arbitrary method
support, the server cannot be used as a companion-spec platform.

**Independent Test**: Can be tested by registering a custom method callback,
calling it via the Call service with input arguments, and verifying the correct
output arguments and status code are returned.

**Acceptance Scenarios**:

1. **Given** a custom Method registered on an Object, **When** a client calls
   the Method via Call, **Then** the registered callback executes and returns
   output arguments with Good status.
2. **Given** a Method call to an unregistered MethodId, **When** the Call
   service processes it, **Then** Bad_MethodInvalid is returned.
3. **Given** a Method callback that sets an error status, **When** the Call
   service processes it, **Then** the error status and diagnostic info are
   returned in the CallResponse.

---

### User Story 3 — Standard Event Subscription + Address Space Notifier (Priority: P2)

An OPC UA Client subscribes to Events with an EventFilter specifying a
where-clause (e.g. Severity > 500) and select-clauses for specific BaseEventType
fields (SourceNode, SourceName, LocalTime). Events that match the filter are
delivered; events that do not match are suppressed. The client can also specify
which nodes it monitors for events by setting the EventNotifier attribute on the
MonitoredItem.

**Why this priority**: Event filtering and notifier scoping are fundamental to
real-world event use and required by the Standard profile. Without where-clause
evaluation, all events would flood the client.

**Independent Test**: Can be tested by creating an Event MonitoredItem with an
EventFilter containing a where-clause (e.g., Severity >= 500), triggering events
of different severity levels, and verifying only matching events are published.

**Acceptance Scenarios**:

1. **Given** an EventFilter select-clause requesting EventId, EventType, Time,
   Message, Severity, SourceNode, **When** an event is triggered, **Then** all
   seven requested fields appear in the EventFieldList.
2. **Given** an EventFilter where-clause requiring Severity >= 500, **When** an
   event with Severity=300 is triggered, **Then** the event is NOT delivered to
   this MonitoredItem.
3. **Given** a Server Object with EventNotifier attribute set, **When** a
   MonitoredItem Targets this node for events, **Then** events from the Server
   Object are delivered. **When** a different node with no EventNotifier
   triggers an event, **Then** it is NOT delivered.

---

### User Story 4 — Auditing (Priority: P2)

An administrator configures the server to emit audit events. When a client opens
a SecureChannel, creates a Session, or activates a Session, the server raises
corresponding audit events (AuditOpenSecureChannelEventType,
AuditCreateSessionEventType, AuditActivateSessionEventType). When a client writes
to a variable, an AuditWriteUpdateEventType is raised.

**Why this priority**: Auditing is a security-essential Standard profile
requirement. Industrial deployments require audit trails for compliance (IEC
62443, NIST 800-82).

**Independent Test**: Can be tested by performing a secure channel open,
session create, and session activate, then subscribing to the Server Object's
Events and verifying audit event types are received with correct
ClientAuditEntryId correlation.

**Acceptance Scenarios**:

1. **Given** auditing enabled, **When** a client opens a SecureChannel,
   **Then** an AuditOpenSecureChannelEventType event is raised with the
   requesting client's application URI.
2. **Given** auditing enabled, **When** a client creates a session,
   **Then** an AuditCreateSessionEventType event is raised.
3. **Given** auditing enabled, **When** a client writes to a variable,
   **Then** an AuditWriteUpdateEventType event is raised with the NodeId
   and old/new values.

---

### User Story 5 — ComplexType Server Facet (Priority: P2)

A server developer defines a custom Structure DataType with named fields and a
custom Enumeration DataType with named values. The server exposes these types in
the address space under Types/DataTypes/BaseDataType/Structure/. Clients can
discover the type definitions via Browse and read their DataTypeDefinition
attributes.

**Why this priority**: Complex types are required by the Standard profile and
virtually every companion spec. Without them, the server cannot model
structured data beyond built-in types.

**Independent Test**: Can be tested by defining a custom structure, exposing a
Variable of that type, browsing the DataType hierarchy to find it, and
verifying the DataTypeDefinition attribute contains the correct StructureField
descriptions.

**Acceptance Scenarios**:

1. **Given** a custom structure DataType registered, **When** a client browses
   Types/DataTypes/BaseDataType/Structure/, **Then** the custom type appears
   with HasSubtype reference from Structure.
2. **Given** a custom structure DataType, **When** the client reads its
   DataTypeDefinition attribute, **Then** the StructureDefinition with correct
   field names and types is returned.

---

### User Story 6 — Server Diagnostics (Priority: P3)

A monitoring client reads the ServerDiagnostics object to retrieve server health
metrics: cumulative session count, rejected session count, current subscription
count, and session-level diagnostics. The server updates these counters in
real-time as sessions are created/closed and subscriptions are
created/modified/deleted.

**Why this priority**: Diagnostics are required by the Standard profile's Base
Server Behavior Facet and are essential for operational monitoring, but can be
added late without blocking other facets.

**Independent Test**: Can be tested by reading the ServerDiagnosticsSummary
Variable, creating and closing sessions, creating and deleting subscriptions,
then verifying the cumulative counters have incremented.

**Acceptance Scenarios**:

1. **Given** an initial SessionCount of N, **When** a new client session is
   created, **Then** ServerDiagnostics.CumulatedSessionCount increments and
   SessionCount reflects current active sessions.
2. **Given** an active subscription, **When** it is deleted, **Then**
   ServerDiagnostics.CurrentSubscriptionCount decrements.

---

### User Story 7 — Remaining Facets and Hardening (Priority: P3)

Includes: Durable Subscriptions (TransferSubscriptions), full aggregate function
set (beyond Average/Min/Max), Security Time Synchronization, Base Server
Behavior objects (Namespaces, ServerRedundancy), and Reverse Connect transport.

**Why this priority**: These complete the Standard profile surface but are the
least critical for functional completeness.

**Independent Test**: Each sub-feature is independently testable: e.g.,
TransferSubscriptions by creating a subscription in session A and transferring
it to session B; aggregate Count/Range by testing with the corresponding
filter.

**Acceptance Scenarios**:

1. **Given** a subscription in session A, **When** TransferSubscriptions is
   called from session B, **Then** session B receives the subscription's
   notifications.
2. **Given** a MonitoredItem with AggregateFilter using Count, **When** the
   variable value changes multiple times, **Then** the aggregate count is
   published.
3. **Given** a server with Durable Subscriptions, **When** a client reconnects
   after disconnect, **Then** the subscription resumes without loss.
4. **Given** a server built with Reverse Connect, **When** configured with a
   client endpoint, **Then** the server initiates connection to the client.

---

### Edge Cases

- **Percent deadband without EURange**: If EURange Property is absent on an
  AnalogItem, return Bad_DeadbandFilterInvalid (OPC-10000-4 §7.22.2).
- **where-clause with no matching events**: Publish should still send keep-alive
  notifications; the MonitoredItem queue should remain at zero.
- **Audit event overflow**: If audit event queue is full, oldest events drop;
  server logs a warning via the diagnostics adapter.
- **DataTypeDefinition on non-DataType nodes**: Read of DataTypeDefinition on
  non-DataType nodes returns Bad_AttributeIdInvalid.
- **TransferSubscriptions with invalid SubscriptionId**: Return
  Bad_SubscriptionIdInvalid.
- **Aggregate filter with unsupported aggregate function**: Return
  Bad_MonitoredItemFilterUnsupported for functions beyond the implemented set.
- **Reverse Connect timeout**: If the target client is unreachable, server
  retries with backoff up to a configurable maximum.
- **Diagnostics reset**: Reading ServerDiagnosticsSummary with a Reset request
  zeroes cumulative counters without affecting current/active counts.
- **Feature gating**: Each facet is gated behind its own CMake flag; standard
  profile build enables all required flags. Nano/micro/embedded profiles remain
  unchanged.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Server MUST support percent deadband filtering for MonitoredItems
  per OPC-10000-4 §7.22.2.
- **FR-002**: Server MUST expose EURange and EngineeringUnits Properties on
  AnalogItemType Variable nodes.
- **FR-003**: Server MUST support InstrumentRange Property on AnalogItemType nodes.
- **FR-004**: Server MUST allow application-registered Method callbacks to be
  invoked via the Call service (OPC-10000-4 §5.12.2.2).
- **FR-005**: Server MUST evaluate EventFilter where-clause (ContentFilter with
  SimpleAttributeOperand elements) and suppress non-matching events.
- **FR-006**: Server MUST support select-clause extraction for all BaseEventType
  fields (EventId, EventType, SourceNode, SourceName, Time, ReceiveTime,
  LocalTime, Message, Severity) per OPC-10000-5 §6.4.
- **FR-007**: Server MUST support per-node EventNotifier attribute on address
  space nodes, gating event delivery to MonitoredItems by source node.
- **FR-008**: Server MUST raise AuditOpenSecureChannelEventType on
  OpenSecureChannel requests (OPC-10000-5 §6.5.3).
- **FR-009**: Server MUST raise AuditCreateSessionEventType on CreateSession
  requests (OPC-10000-5 §6.5.4).
- **FR-010**: Server MUST raise AuditActivateSessionEventType on
  ActivateSession requests (OPC-10000-5 §6.5.5).
- **FR-011**: Server MUST raise AuditWriteUpdateEventType on Write requests
  (OPC-10000-5 §6.5.8).
- **FR-012**: Server MUST support custom Structure DataTypes with
  StructureDefinition and StructureField definitions per OPC-10000-3 §5.6.4.
- **FR-013**: Server MUST support custom Enumeration DataTypes with
  EnumDefinition and EnumField definitions per OPC-10000-3 §5.6.5.
- **FR-014**: Server MUST expose ServerDiagnosticsSummary object with
  CumulatedSessionCount, CumulatedSubscriptionCount, CurrentSessionCount,
  CurrentSubscriptionCount, SecurityRejectedSessionCount, RejectedSessionCount,
  SessionTimeoutCount per OPC-10000-5 §6.3.3.
- **FR-015**: Server MUST expose ServerDiagnostics.SessionsDiagnosticsSummary
  with per-session diagnostic arrays per OPC-10000-5 §6.3.5.
- **FR-016**: Server MUST expose ServerRedundancy object with RedundancySupport
  and ServiceLevel per OPC-10000-5 §6.3.11.
- **FR-017**: Server MUST expose Namespaces object under Server for namespace
  metadata discovery per OPC-10000-5 §6.2.10.
- **FR-018**: Server MUST support TransferSubscriptions service per
  OPC-10000-4 §5.14.7 for subscription mobility between sessions.
- **FR-019**: Server MUST support full aggregate function set: Count, Range,
  TimeAverage, DurationGood, DurationBad, PercentGood, PercentBad, WorstQuality,
  Total, MinimumActualTime, MaximumActualTime, AnnotationCount, Delta,
  DeltaBounds, Start, End, DurationInStateZero per OPC-10000-13.
- **FR-020**: Server MUST support at least 5 SecurityPolicies (None,
  Basic256Sha256, Aes128-Sha256-RsaOaep, Aes256-Sha256-RsaPss,
  Basic256) per the SecurityPolicy profile requirements.
- **FR-021**: Server MUST support Reverse Connect (server-initiated TCP
  connections to clients) per OPC-10000-6 §7.5.
- **FR-022**: Server MUST support Time Synchronization via the
  TimestampsToReturn mechanism per OPC-10000-4 §A.2.
- **FR-023**: All existing 108 tests MUST pass with zero regressions.
- **FR-024**: Each new facet SHALL be gated behind its own CMake option; the
  "standard" profile target SHALL enable all required facets.
- **FR-025**: ASAN/UBSan MUST pass zero issues for all new code paths.
- **FR-026**: Static data (`.data`, `.bss`, heap) MUST remain at zero bytes
  across all profiles (nano, micro, embedded, standard) when built with the
  minimal configuration (no runtime session/subscription/event state).

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target profile is Standard 2017 UA Server Profile
  (`http://opcfoundation.org/UA-Profile/Server/StandardUA2017`, OPC-10000-7
  §6.6, profile ID 1663).
- **OPC-002**: Data Access Server Facet per OPC-10000-7 §6.6.8 (AnalogItemType,
  EURange, EngineeringUnits, percent deadband).
- **OPC-003**: Method Server Facet per OPC-10000-7 §6.6.25 (arbitrary Method
  Call via OPC-10000-4 §5.12.2.2).
- **OPC-004**: Standard Event Subscription Server Facet per OPC-10000-7 §6.6.23
  (EventFilter with where-clause per OPC-10000-4 §7.22.3; BaseEventType fields
  per OPC-10000-5 §6.4).
- **OPC-005**: Address Space Event Notifier Server Facet per OPC-10000-7
  §6.6.174 (EventNotifier attribute on nodes, OPC-10000-3 §5.4.6).
- **OPC-006**: ComplexType Server Facet per OPC-10000-7 (StructureDefinition
  OPC-10000-3 §5.6.4, EnumDefinition OPC-10000-3 §5.6.5).
- **OPC-007**: Auditing Server Facet per OPC-10000-7 §6.6.57 (audit event types
  per OPC-10000-5 §6.5).
- **OPC-008**: Base Server Behavior Facet per OPC-10000-7 §6.6.2
  (ServerDiagnostics OPC-10000-5 §6.3, ServerRedundancy OPC-10000-5 §6.3.11).
- **OPC-009**: Client Redundancy Facet per OPC-10000-7 §6.6.38
  (TransferSubscriptions OPC-10000-4 §5.14.7).
- **OPC-010**: Historical Access Server Facet per OPC-10000-7 §6.6.49
  (HistoryRead raw, modified, at-time per OPC-10000-11).
- **OPC-011**: Aggregate Server Facet per OPC-10000-7 §6.6.72 (OPC-10000-13
  aggregate functions).
- **OPC-012**: Rediscovery Server Facet per OPC-10000-7 §6.6.73 (ModelChange
  events, ServerRedundancy ServiceLevel).
- **OPC-013**: Reverse Connect Facet per OPC-10000-7 §6.6.69.2 (server-initiated
  connections per OPC-10000-6 §7.5).
- **OPC-014**: Unsupported services/features MUST return Bad_ServiceUnsupported
  (OPC-10000-4 §7.38.2) for services behind disabled CMake gates.
- **OPC-015**: Wire encoding per OPC-10000-6 §5.2 (UA Binary), transport per
  OPC-10000-6 §7.2 (OPC UA TCP), security model per OPC-10000-2.
- **OPC-016**: Conformance status is "profile-targeting"; CTT-verified status
  withheld until CTT run passes.

### Scope Boundaries *(mandatory)*

- **In Scope**:
  - Data Access Server Facet (percent deadband, EURange, EngineeringUnits,
    InstrumentRange, AnalogItemType)
  - Method Server Facet (arbitrary custom methods via Call service)
  - Standard Event Subscription (full EventFilter where-clause, all
    BaseEventType fields)
  - Address Space Event Notifier (per-node EventNotifier attribute, source-node
    event filtering)
  - Auditing (security-relevant audit event types)
  - ComplexType Server Facet (custom Structure and Enumeration types)
  - Server Diagnostics (ServerDiagnosticsSummary, SessionsDiagnostics)
  - Base Server Behavior (ServerRedundancy, Namespaces node)
  - Durable Subscriptions (TransferSubscriptions service)
  - Full aggregate function set (Count through DurationInStateZero)
  - Reverse Connect transport
  - Security Time Synchronization
  - Profile gating: all new facets behind individual CMake flags; new
    `standard` profile target; existing profiles unchanged
  - ServerProfileArray updated to advertise Standard 2017 UA Server Profile URI

- **Out of Scope**:
  - CTT certification (external verification step)
  - Companion specification implementation (DI, ADI, PLCopen, etc.)
  - GDS (Global Discovery Server)
  - PubSub extensions
  - File Transfer (Part 20)
  - Alarms and Conditions beyond existing basic event support
  - XML or JSON encoding (UA Binary only)
  - HTTPS or WebSocket transport (UA TCP only)
  - Redundancy beyond ServerRedundancy object (no failover clustering)
  - ServerConfiguration node (vendor-specific configuration model)

- **Targeting Claim**: Standard 2017 UA Server Profile may be targeted as
  "profile-targeting" after this feature ships. External CTT verification
  is required before promoting to "profile-compliant" or "CTT-verified".

- **Application Headroom Goal**: Standard profile binary size should not exceed
  80 KB `.text` on ARM Cortex-M0+ (Thumb). Existing profiles (nano, micro,
  embedded) must not regress in size beyond 2%.

### Key Entities *(include if feature involves data)*

- **EventNotifier attribute**: Byte-valued attribute on address space nodes
  (Object and View types), indicating event subscription capability per
  OPC-10000-3 §5.4.6.
- **Audit Event**: Subtype of BaseEventType with additional fields:
  ActionTimeStamp, Status, ServerId, ClientAuditEntryId, ClientUserId. Specialized
  subtypes: AuditChannelEventType, AuditSessionEventType,
  AuditUpdateMethodEventType, etc.
- **StructureDefinition**: DataType node attribute containing defaultEncodingId,
  baseDataType, structureType, and an array of StructureField definitions
  (name, description, dataType, valueRank, arrayDimensions, maxStringLength,
  isOptional).
- **EnumDefinition**: DataType node attribute containing an array of EnumField
  definitions (value, displayName, description, name).
- **ServerDiagnosticsSummary**: Variable of ServerDiagnosticsSummaryDataType
  with cumulative server health counters.
- **SessionsDiagnosticsSummary**: Variable of SessionsDiagnosticsSummaryDataType
  with per-session diagnostics arrays.
- **ServerRedundancy**: Object of ServerRedundancyType with RedundancySupport
  (enum: None, Cold, Warm, Hot, Transparent, HotAndMirrored) and ServiceLevel
  (Byte).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A client can create a MonitoredItem with percent deadband filter
  on an AnalogItem variable with EURange, and only value changes exceeding the
  deadband threshold generate notifications.
- **SC-002**: A client can call any Method registered by the application via the
  Call service and receive correct output arguments.
- **SC-003**: A client subscribing to events with a where-clause (e.g., Severity
  >= 500) receives only matching events and no non-matching events.
- **SC-004**: A client subscribing to events for a specific source node receives
  events only from that node and not from other event-emitting nodes.
- **SC-005**: Audit events are raised for OpenSecureChannel, CreateSession,
  ActivateSession, and Write service calls.
- **SC-006**: A custom Structure DataType is discoverable via Browse of the
  DataType hierarchy and its StructureDefinition is readable.
- **SC-007**: ServerDiagnosticsSummary counters increment correctly as
  sessions, subscriptions are created, timed out, and rejected.
- **SC-008**: TransferSubscriptions moves an active subscription from one
  session to another without data loss.
- **SC-009**: All 42 standard aggregate functions from OPC-10000-13 that are
  applicable to a running time series are supported.
- **SC-010**: All existing 108 tests pass unchanged (zero regressions).
- **SC-011**: ASAN/UBSan reports zero issues on the full test suite.
- **SC-012**: All existing profiles (nano, micro, embedded) compile and pass
  tests with no size regression exceeding 2%.
- **SC-013**: The "standard" CMake profile target builds successfully with all
  newly implemented facets enabled.
- **SC-014**: New interop tests can demonstrate the full Standard 2017 UA Server
  Profile feature surface against at least one OPC UA client implementation.

## Assumptions

- Existing address space, encoding, session, transport, and security
  infrastructure is stable and correct; new facets build on this foundation.
- The `MUC_OPCUA_EMBEDDED_PROFILE` flag currently controls capacity minima;
  `MUC_OPCUA_STANDARD_PROFILE` will raise minima further per the Standard
  profile's capacity expectations (e.g., MU_MAX_MONITORED_ITEMS >= 1000).
- Custom method callbacks are registered by the application at initialization
  time before the server starts accepting client connections.
- Custom Structure and Enumeration types are defined at compile time as static
  data; runtime type creation is out of scope.
- Event where-clause evaluation is limited to SimpleAttributeOperand elements
  with basic comparison operators (EqualTo, GreaterThan, LessThan,
  GreaterThanOrEqual, LessThanOrEqual); full ContentFilter with
  ElementOperand/AND/OR is deferred.
- Audit events use the existing event delivery pipeline (same Publish
  mechanism); no separate audit event channel or persistent audit log.
- Reverse Connect requires the target client to already be listening; the server
  does not implement client-side discovery/registration protocols.
- Security Time Synchronization uses the existing time adapter and
  TimestampsToReturn mechanism; no new network protocol is required.
- Durable subscriptions persist subscription state in-memory across session
  lifecycle; no persistent storage is required.
- Aggregate functions beyond Average/Min/Max are implemented as stateless
  stream aggregators; stateful aggregators leveraging processing intervals are
  out of scope.
- ServerProfileArray will include `http://opcfoundation.org/UA-Profile/Server/StandardUA2017`
  plus applicable facet URIs when the standard profile target is built.
