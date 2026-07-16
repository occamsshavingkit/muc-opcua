<!-- markdownlint-disable MD013 -->

# Research: Server-Emitted, Client-Observable AuditEvents

**Feature**: 074-server-auditing-events | **Date**: 2026-07-16
**Status**: Grounded against opc-ua-reference (OPC-10000-5 §6.4). One planning clarification (FR-008) resolved below.

## Decision 1 — AuditEvent subtype NodeIds and field sets (grounded)

`AuditEventType` = `i=2052` (OPC-10000-5 §6.4.3). Base fields inherited from
BaseEventType (§5.4): `EventId`, `EventType`, `SourceNode`, `SourceName`,
`Time`, `ReceiveTime`, `LocalTime`, `Message`, `Severity`. AuditEventType adds:
`ActionTimeStamp`, `Status`, `ServerId`, `ClientAuditEntryId`, `ClientUserId`.

The four already-modelled `mu_audit_event_t` variants map to these grounded subtypes:

| `mu_audit_event_t` variant | AuditEvent subtype | NodeId | Section | Subtype-specific fields |
| --- | --- | --- | --- | --- |
| OPEN_SECURE_CHANNEL | AuditOpenSecureChannelEventType | `i=2060` | 6.4.6 | SecureChannelId, ClientCertificate, RequestType, SecurityPolicyUri, SecurityMode, RequestedLifetime (we currently carry SecureChannelId) |
| CREATE_SESSION | AuditCreateSessionEventType | `i=2071` | 6.4.8 | SessionId, (SecureChannelId, ClientCertificate) |
| ACTIVATE_SESSION | AuditActivateSessionEventType | `i=2075` | 6.4.10 | SessionId (via SourceNode), UserIdentityToken, ClientSoftwareCertificates (we carry SessionId + user_name) |
| WRITE_UPDATE | AuditWriteUpdateEventType | `i=2100` | 6.4.25 | AttributeId, IndexRange, OldValue, NewValue (we carry node_id + old/new value) |

Hierarchy: `AuditOpenSecureChannelEventType` (2060) ⊂ `AuditChannelEventType`
(2059) ⊂ `AuditSecurityEventType` ⊂ `AuditEventType` (2052); the session types ⊂
`AuditSessionEventType`; WriteUpdate ⊂ `AuditUpdateEventType`.

**Decision**: emit each action as its grounded subtype `EventType` NodeId, with
the base + AuditEvent fields populated from `mu_audit_event_t` (already carries
`status`, `action_timestamp`, `server_id`, `client_audit_entry_id`,
`client_user_id`) plus the subtype-specific fields we already model. Additional
subtype fields (e.g. SecurityPolicyUri) MAY be filled if cheaply available;
absent optional fields are emitted Null (spec-permitted).

## Decision 2 — routing through the existing event pipeline

`mu_server_trigger_event()` (notification.c:330) already queues an event to every
MonitoredItem watching `attribute_id == 12` and the EventFilter SELECT/WHERE
machinery already applies (proven by `test_event_notifications`). **Decision**:
add an audit→event adapter that builds a `mu_event_notification_t` (EventType +
field values) from a `mu_audit_event_t` and calls `mu_server_trigger_event`. The
existing host-callback path in `mu_raise_audit_event` is retained (integrator
hook) but the conformance mechanism is the event pipeline. SourceNode = Server
Object `i=2253`.

## Decision 3 — Server EventNotifier attribute (reconciles CU 3194)

The Server Object `i=2253` must expose a readable `EventNotifier` attribute
(id 12, `MU_ATTRIBUTEID_EVENTNOTIFIER`) with the `SubscribeToEvents` bit (bit 0,
value `0x01`) set (OPC-10000-3 §5.4 EventNotifier; OPC-10000-5 ServerType).
`read_attribute.c` currently returns `Bad_AttributeIdInvalid` for it (:254);
no node sets `.event_notifier`.

**Decision**: set `.event_notifier = 0x01` on the Server Object in
`base_nodes.c` and add the `MU_ATTRIBUTEID_EVENTNOTIFIER` read case returning the
node's `event_notifier` byte (0 for non-notifier nodes). This closes
`opc_cu_3194` and lets a client discover the Server as an event source before
subscribing. Gated with the auditing/events surface so nano stays minimal.

## Decision 4 — gating

Emission + audit→event routing gated by `MUC_OPCUA_CU_AUDITING` (sets both
`MUC_OPCUA_CU_AUDITING` and `MUC_OPCUA_AUDITING`, src/CMakeLists.txt:383-384).
The EventNotifier attribute read case is tiny and always-safe (returns 0 when no
node sets it); setting the Server `.event_notifier` bit is gated on the
event/auditing surface so a no-events profile advertises no notifier. No new
Kconfig symbol needed — reuse `MUC_OPCUA_CU_AUDITING` (+ `MUC_OPCUA_CU_EVENTS`
for the delivery pipeline, already a dependency of event delivery).

## Decision 5 — FR-008 resolution: which granular Auditing CUs become claimable

After emit+observe of the 4 modelled actions, the claimable set is bounded by
which services actually emit an AuditEvent:

| CU | Name | Claimable after 074? | Reason |
| --- | --- | --- | --- |
| 2422 | Auditing Secure Communication | **Yes** | AuditOpenSecureChannelEventType emitted + observable on OPN (success/failure) |
| 3968 | Auditing Services | **Yes (audited subset)** | Session create/activate + write emit AuditEvents observable by a client |
| 3228 | Auditing Write | **Yes** | AuditWriteUpdateEventType emitted on attribute Write |
| 5213 | Auditing Connections | **Partial → defer** | OPN is audited, but there is no channel-close/connection-close audit type; claim only if CTT accepts OPN-open auditing |
| 3224 | Auditing NodeManagement | No | no AddNodes/DeleteNodes audit type or emit site |
| 3226 | Auditing History Services | No | no history-update audit type or emit site |
| 3230 | Auditing Method | No | no Call/method audit type or emit site |

**Decision**: this feature makes **2422, 3968, 3228** genuinely claimable (with
emit→observe E2E tests) and reconciles **3194** (EventNotifier). `5213` is
deferred pending a connection-close audit type; `3224/3226/3230` remain
unclaimed (no emit sites). `opc_cu_auditing` is re-annotated to reflect
emit+observe of the 4 modelled actions (not the full audit surface).

## Open questions

None blocking. Exact subtype-field completeness (e.g. whether to populate
SecurityPolicyUri on the channel audit) is an implementation refinement, not a
design unknown.

## Decision 6 — event-field-model extension (revised scope, post-implementation-probe)

**Finding (2026-07-16, during implementation)**: the event pipeline delivers only
5 base fields. `mu_event_notification_t` (types.h) carries `event_type`,
`event_id`, `time`, `message`, `severity`; the SELECT resolver
(notification.c:277-310) emits exactly those and Null for everything else
(SourceNode/SourceName/ReceiveTime/LocalTime and any AuditEvent field). The
`MU_EVENT_FIELD_*` enum already *reserves* SourceNode/SourceName/ReceiveTime/
LocalTime (parsed, resolved Null), so SELECT-path parsing extends cleanly, but
**no audit-specific field can currently be carried or resolved.**

**Decision (per user choice: full-field AuditEvents)**: extend the event field
model so AuditEvents carry and can SELECT their fields:

1. `mu_event_notification_t`: add an audit payload **only under
   `#if MUC_OPCUA_CU_AUDITING`** (non-auditing builds pay 0): `source_node`
   (nodeid), `status` (bool), `action_timestamp` (datetime), and a bounded
   subtype union (secure_channel_id, session_id, attribute_id, old/new scalar).
2. `MU_EVENT_FIELD_*`: add STATUS, ACTIONTIMESTAMP, SERVERID, CLIENTAUDITENTRYID,
   CLIENTUSERID, ATTRIBUTEID, OLDVALUE, NEWVALUE, SECURECHANNELID, SESSIONID.
3. notification.c SELECT resolver: add cases for the new enums.
4. filter_reader.c SELECT parsing: map AuditEvent BrowseName paths to the enums.

## Decision 7 — bounded capture (no-heap lifetime + RAM)

The notification is **queued** (`queue[MU_MAX_EVENT_QUEUE_SIZE=8]` per
subscription × `MU_INTERN_MAX_SUBSCRIPTIONS`) and delivered later in Publish, so
any pointed-to data (request strings, write value arrays) is gone by delivery.

**Decision**:

- **Strings** (server_id, client_user_id, client_audit_entry_id, secure_channel_id):
  bounded inline char buffers `[MU_AUDIT_STR_MAX]` copied at emit time; over-long
  values truncated (documented). Sized small (e.g. 32-64B).
- **Write old/new values**: capture **scalar** built-in values inline (small
  fixed union); array/structured values are emitted Null with a documented
  limitation (a scalar-write audit is the common CTT case). Avoids unbounded copy.
- **RAM cost**: the audit payload enlarges the queued notification. Cost =
  `delta * 8 * MU_INTERN_MAX_SUBSCRIPTIONS`, paid **only** on auditing-enabled
  profiles. Must be measured (SC-003) and may motivate a smaller audit-profile
  `MU_MAX_EVENT_QUEUE_SIZE` or subscription cap. **This is the dominant cost of
  the full-field path and the reason it is materially larger than base-only.**

## Decision 8 — WHERE over audit fields

`mu_where_clause_eval` reads `mu_event_fields_t` (a separate carrier). WHERE over
audit-specific fields is **out of scope** for the first cut (SELECT-only for
audit fields); WHERE continues to operate on the base fields. Documented; a
follow-up can extend `mu_event_fields_t` if CTT requires audit-field WHERE.
