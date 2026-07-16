<!-- markdownlint-disable MD013 -->

# Data Model: Server-Emitted, Client-Observable AuditEvents

**Feature**: 074-server-auditing-events | **Date**: 2026-07-16

No new runtime data structures beyond a small field-mapping. The model is the
mapping from the existing `mu_audit_event_t` to an OPC UA Event delivered through
the pipeline, plus the EventNotifier attribute.

## Entity: AuditEvent (mapping `mu_audit_event_t` → EventFieldList)

Common fields (every emitted AuditEvent), grounded OPC-10000-5 §5.4 + §6.4.3:

| Event field | Source | Notes |
| --- | --- | --- |
| EventId | server-generated (unique ByteString) | reuse existing event-id generation |
| EventType | subtype NodeId | 2060 / 2071 / 2075 / 2100 |
| SourceNode | Server Object `i=2253` | the notifier |
| SourceName | "Server" | |
| Time / ReceiveTime | `action_timestamp` (time adapter) | |
| Severity | 1 (default) | audit severity |
| Message | subtype description | LocalizedText |
| ActionTimeStamp | `action_timestamp` | AuditEventType |
| Status | `status` (bool) | false on failed action (FR-006) |
| ServerId | `server_id` | |
| ClientAuditEntryId | `client_audit_entry_id` | |
| ClientUserId | `client_user_id` | |

Subtype-specific:

| Variant | EventType | Extra fields (from union) |
| --- | --- | --- |
| OPEN_SECURE_CHANNEL | 2060 | SecureChannelId (`open_channel.secure_channel_id`) |
| CREATE_SESSION | 2071 | SessionId (`create_session.session_id`) |
| ACTIVATE_SESSION | 2075 | SessionId + UserIdentityToken/user_name (`activate_session`) |
| WRITE_UPDATE | 2100 | AttributeId, OldValue, NewValue (`write_update`) |

## Entity: EventNotifier attribute

| Property | Value |
| --- | --- |
| Attribute id | 12 (`MU_ATTRIBUTEID_EVENTNOTIFIER`) |
| Type | Byte (bit field) |
| Server Object (`i=2253`) value | `0x01` (SubscribeToEvents, bit 0) |
| All other nodes | `0x00` (default; read returns 0) |
| Read behaviour | `read_attribute.c` returns the node's `event_notifier` byte instead of `Bad_AttributeIdInvalid` |

## Non-entity: host-callback path

`mu_audit_callback_t` / `mu_server_add_audit_callback` remain for integrators
but are NOT the conformance mechanism; the event pipeline is. `mu_raise_audit_event`
gains the pipeline-routing step in addition to callback dispatch.
