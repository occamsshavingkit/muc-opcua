# Research: Standard 2017 UA Server Profile Implementation

**Feature**: 037-standard-server-profile | **Phase**: 0 | **Date**: 2026-07-05

## Decision 1: Percent Deadband Implementation

**Decision**: Compute percent deadband relative to EURange span (High - Low) per OPC-10000-4 §7.22.2: "absolute change in a Numeric Variable that exceeds (deadbandValue / 100.0) * (high - low)". Require EURange Property on the Variable node; if absent, return Bad_DeadbandFilterInvalid.

**Rationale**: OPC-10000-4 §7.22.2 is explicit about the formula. Percent deadband without EURange is undefined; rejecting with Bad_DeadbandFilterInvalid matches the spec behavior expected by clients.

**Alternatives considered**:
- Use value range derived from DataType limits — rejected because spec mandates EURange, not DataType limits.
- Silently default to 0% deadband — rejected because it would silently change client-requested behavior.

## Decision 2: Method Dispatch Architecture

**Decision**: Extend the existing `dispatch_method.c` to support arbitrary MethodIds. Use a registration model: application calls `mu_server_register_method(NodeId method_id, mu_method_callback_t callback)` before server start. Dispatch looks up method_id in a static table indexed by NodeId. Unregistered methods return Bad_MethodInvalid.

**Rationale**: The existing code already has `MUC_OPCUA_CUSTOM_METHODS` and the callback registration API (`server.c:1069`). The extension adds a static lookup table (typically 8-16 entries for embedded), keeping the hot path O(1). This avoids heap allocation in the dispatch path.

**Alternatives considered**:
- Dynamic linked-list dispatch — rejected due to heap use and O(n) lookup.
- Address-space-based method resolution (traverse HasComponent references from Object Nodes) — rejected as too heavy for embedded; static table is simpler and faster.
- Per-object method tables — deferred; current scope only needs server-wide method registration.

## Decision 3: EventFilter Where-Clause Evaluation

**Decision**: Implement a recursive evaluator for ContentFilter elements with SimpleAttributeOperand selectors. Support comparison operators: Equal, GreaterThan, LessThan, GreaterThanOrEqual, LessThanOrEqual, NotEqual, Like. Support logical operators: And, Or, Not. Evaluate against event field values obtained from the event notification structure.

**Rationale**: OPC-10000-4 §7.22.3 defines the EventFilter with whereClause as a ContentFilter. Real-world clients (UA Expert, Kepware, WinCC) routinely use where-clauses for event subscription. Without where-clause evaluation, the server cannot filter events and clients receive noise.

**Alternatives considered**:
- Skip where-clause evaluation (current behavior) — rejected; breaks event filtering, a core Event Subscription requirement.
- Precompile where-clause to bytecode — rejected as over-engineered for embedded; interpretive evaluation is sufficient for typical filter complexity (depth < 4).
- Limit to SimpleAttributeOperand only (no ElementOperand nesting) — deferred to spec assumption; flat filters cover common use cases.

## Decision 4: Audit Event Types

**Decision**: Implement 4 audit event types: AuditOpenSecureChannelEventType (OPC-10000-5 §6.5.3), AuditCreateSessionEventType (§6.5.4), AuditActivateSessionEventType (§6.5.5), AuditWriteUpdateEventType (§6.5.8). Each extends BaseEventType with audit-specific fields (ActionTimeStamp, Status, ServerId, ClientAuditEntryId). Use existing event delivery pipeline (Publish mechanism).

**Rationale**: These 4 types cover the mandatory audit surface for the Standard profile. AuditOpenSecureChannelEventType covers secure channel lifecycle; CreateSession/ActivateSession cover session lifecycle; WriteUpdate covers data modification. The existing event pipeline avoids duplicating delivery infrastructure.

**Alternatives considered**:
- Separate audit event queue/channel — rejected; the Publish mechanism already delivers events to subscribed clients. A separate channel adds complexity without benefit.
- Full OPC-10000-5 §6.5 audit type set (12+ types) — deferred; the 4 mandatory types cover the Standard profile requirement. Additional types (AddNodes, DeleteNodes, etc.) can be added later.
- Persistent audit log — out of scope; auditing in this implementation means real-time event delivery, not persistent storage.

## Decision 5: ComplexType Architecture

**Decision**: Use compile-time static registration for custom Structure and Enumeration DataTypes. Application calls `mu_register_structure_type(NodeId type_id, mu_structure_def_t *def)` at init time. The type appears in the DataType hierarchy under BaseDataType/Structure/ (or /Enumeration/). The DataTypeDefinition attribute returns a StructureDefinition or EnumDefinition per OPC-10000-3 §5.6.4/5.6.5. Binary encoding uses the StructureField list to serialize/deserialize fields.

**Rationale**: Static registration matches the existing address-space model (static base_nodes.c). Runtime type creation (AddNodes for DataType) is out of scope per the spec. This keeps the type system bounded and predictable.

**Alternatives considered**:
- Runtime type creation via NodeManagement — rejected; would require full dynamic node management for DataType nodes, which is a separate feature scope.
- Code generation from NodeSet2 XML — rejected; adds build complexity. Static C registration is simpler for embedded.

## Decision 6: Diagnostics Architecture

**Decision**: Implement ServerDiagnosticsSummary as a struct of counters updated atomically on session/subscription lifecycle events. Expose as a Variable node under Server/ServerDiagnostics/. Implement SessionsDiagnosticsSummary as a fixed-size array of per-session diagnostic structs, indexed by session handle. Reading the diagnostics node calls a value source callback that snapshots the current counters.

**Rationale**: Counters are cheap (64 bytes total for summary). Atomic updates avoid locks. Snapshot-on-read avoids maintaining multiple copies. The value source callback pattern matches how ServerStatus.CurrentTime is already implemented.

**Alternatives considered**:
- Push-based updates to the Variable value — rejected; would add overhead on every session/subscription event for diagnostics that may never be read.
- Linked-list of session diagnostics — rejected; fixed array is simpler and bounded.

## Decision 7: Aggregate Functions

**Decision**: Implement 42 aggregate functions per OPC-10000-13 in a stateless streaming model. Each function maintains a small aggregate state struct (accumulator, count, min, max, etc.). On each processed value, the state is updated. On query, the aggregate is computed from the state. Supported functions: Average, Count, Delta, DeltaBounds, DurationGood, DurationBad, DurationInStateZero, End, Interpolative, Maximum, Maximum2, MaximumActualTime, Minimum, Minimum2, MinimumActualTime, PercentGood, PercentBad, Range, Start, TimeAverage, TimeAverage2, Total, Total2, WorstQuality, WorstQuality2, AnnotationCount, and their bounding variants.

**Rationale**: OPC-10000-13 defines these as the standard set. The existing Average/Min/Max implementation proves the streaming aggregate model works. Extending it to all 42 is straightforward (same accumulator pattern, different combinator functions).

**Alternatives considered**:
- Only implementing a subset commonly used in practice (~15 functions) — rejected; the Aggregate Server Facet requires all 42 for full conformance.
- Stateful processing intervals — deferred; stateless streaming is simpler and covers the most common use case (process-as-values-arrive).

## Decision 8: Durable Subscriptions / TransferSubscriptions

**Decision**: Implement TransferSubscriptions (OPC-10000-4 §5.14.7) to move subscription ownership between sessions within the same server. Subscription state stays in memory; the transfer updates the session_id reference. No persistent storage required. On transfer, any queued notifications are delivered to the new session.

**Rationale**: TransferSubscriptions is the core of the Client Redundancy Facet. The implementation is straightforward: update session_id, transfer queued notifications. In-memory state matches the existing subscription model.

**Alternatives considered**:
- Persistent subscription state (disk/flash) — rejected; adds storage dependency. In-memory transfer covers the Client Redundancy requirement.
- Full Durable Subscription with disconnect tolerance — deferred; TransferSubscriptions covers inter-session mobility; across-disconnect resilience requires persistent state which is out of scope.

## Decision 9: Reverse Connect

**Decision**: Implement server-initiated TCP connections to pre-configured client endpoints. When the Reverse Connect flag is enabled, the server attempts outbound connections to a configured list of URLs. On success, the server treats the connection identically to an inbound one (same SecureChannel/session establishment). Connection failures retry with configurable backoff.

**Rationale**: OPC-10000-6 §7.5 defines Reverse Connect for servers behind NAT/firewalls. The existing TCP connection infrastructure (tcp_connection.c) can be reused for outbound connections with minimal changes.

**Alternatives considered**:
- Always-on outbound connections — rejected; connection attempts should be gated behind configuration and only attempted when needed.
- Full LDS-ME (Local Discovery Server with Multicast Extension) — out of scope; Reverse Connect is independent of LDS.

## Decision 10: Security Time Synchronization

**Decision**: Honor the TimestampsToReturn parameter in Read and Write services to return SourceTimestamp and ServerTimestamp. Use the existing time adapter for ServerTimestamp. For SourceTimestamp, preserve the timestamp provided by the application when the value was set (Write service) or last sampled (read from source callback).

**Rationale**: OPC-10000-4 §A.2 governs time synchronization behavior. The existing TimestampsToReturn handling already returns timestamps; this decision formalizes the behavior and ensures both ServerTimestamp and SourceTimestamp are populated.

**Alternatives considered**:
- NTP-based time synchronization — out of scope; the OPC UA Security Time Synchronization is about timestamp fields, not clock synchronization protocols.
- Cryptographic timestamp validation — out of scope; requires SecurityPolicy timestamp verification which is not required by the Standard profile.

## Decision 11: Profile Architecture

**Decision**: Create a new `standard` CMake profile target that enables all newly implemented facets plus the full embedded baseline. Each facet gets its own CMake flag: `MUC_OPCUA_DATA_ACCESS`, `MUC_OPCUA_METHOD_SERVER`, `MUC_OPCUA_EVENT_FILTER_WHERE`, `MUC_OPCUA_AUDITING`, `MUC_OPCUA_COMPLEX_TYPES`, `MUC_OPCUA_SERVER_DIAGNOSTICS` (activate the dead flag), `MUC_OPCUA_REDUNDANCY`, `MUC_OPCUA_AGGREGATE_FULL`, `MUC_OPCUA_REVERSE_CONNECT`, `MUC_OPCUA_TIME_SYNC`. The `standard` profile preset also raises capacity minima: MU_MAX_SESSIONS >= 50, MU_MAX_SUBSCRIPTIONS >= 50, MU_MAX_MONITORED_ITEMS >= 1000.

**Rationale**: Individual flags let users compose exactly the facets they need without pulling in everything. The `standard` preset is a convenience target that enables all required facets for the Standard profile. This matches the existing `nano`/`micro`/`embedded`/`full`/`custom` pattern.

**Alternatives considered**:
- Single `MUC_OPCUA_STANDARD_PROFILE` flag that enables everything — rejected; it prevents users from selectively disabling facets they don't need.
- Add facets to the `full` profile — rejected; `full` is a kitchen-sink target. The `standard` profile maps to a specific OPC UA specification requirement.
