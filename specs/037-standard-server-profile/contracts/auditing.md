# Contract: Auditing

**Feature**: 037-standard-server-profile | **OPC Reference**: OPC-10000-5 §6.5

## Audit Event Types

Four mandatory audit event types implemented:

1. **AuditOpenSecureChannelEventType** (ns=0;i=2060) — raised on OpenSecureChannel
2. **AuditCreateSessionEventType** (ns=0;i=2071) — raised on CreateSession
3. **AuditActivateSessionEventType** (ns=0;i=2075) — raised on ActivateSession
4. **AuditWriteUpdateEventType** (ns=0;i=2097) — raised on Write

## Event Trigger Points

```
OpenSecureChannel → mu_raise_audit_open_channel(channel_id, client_cert, status)
CreateSession     → mu_raise_audit_create_session(session_id, client_desc, status)
ActivateSession   → mu_raise_audit_activate_session(session_id, user_token, status)
Write (per-node)  → mu_raise_audit_write_update(node_id, old_value, new_value, status)
```

## API

```c
// Raise an audit event. If auditing is disabled (compile-time or runtime), this is a no-op.
void mu_raise_audit_event(mu_server_t *server, const mu_audit_event_t *event);

typedef struct {
    mu_node_id_t   event_type_id;  // e.g., ns=0;i=2060
    mu_date_time_t timestamp;       // ActionTimeStamp
    bool           status;          // true = success
    const char    *server_id;
    const char    *client_audit_entry_id;
    // Type-specific fields (union by event type)
    union {
        struct { const char *secure_channel_id; } open_channel;
        struct { mu_node_id_t session_id; } create_session;
        struct { mu_node_id_t session_id; const char *user_name; } activate_session;
        struct { mu_node_id_t node_id; mu_variant_t old_value; mu_variant_t new_value; } write_update;
    } specific;
} mu_audit_event_t;
```

## Delivery

Audit events are delivered through the same Publish mechanism as data-change and
event notifications. They appear as EventFieldList entries in PublishResponse
Notifications for MonitoredItems that subscribe to events on the Server Object.

The Server Object (ns=0;i=2253) has its EventNotifier attribute set to
SubscribeToEvents (1), enabling event subscriptions targeting it.

## Test Contract

1. Open SecureChannel → verify AuditOpenSecureChannelEventType event with correct channel_id.
2. Create session → verify AuditCreateSessionEventType event.
3. Activate session → verify AuditActivateSessionEventType event.
4. Write to a Variable → verify AuditWriteUpdateEventType event with node_id and new value.
5. Create event MonitoredItem on Server Object → verify all 4 audit types received.
6. Disable auditing (runtime flag) → no audit events raised.

## Gating

`MUC_OPCUA_AUDITING` CMake flag. Runtime disable via server config `auditing_enabled = false`.
