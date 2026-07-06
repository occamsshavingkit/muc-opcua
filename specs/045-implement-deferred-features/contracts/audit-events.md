# API Contract: Audit Event Callback Dispatch

**Feature**: 045-implement-deferred-features (D2)
**OPC Ref**: OPC-10000-5 §6.5.2 (EventType), §6.5.3 (AuditEventType)

## Callback Type

```c
typedef void (*mu_audit_callback_t)(struct mu_server *server,
    const mu_audit_event_t *event, void *context);
```

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `server` | in | Server that raised the event |
| `event` | in | Audit event (read-only, populated with timestamp on dispatch) |
| `context` | in | Opaque pointer supplied at registration |

## Registration API

```c
void mu_server_set_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context);
```

Sets a single audit callback, clearing any previously registered callbacks.
Call with `callback == NULL` to clear all callbacks.

```c
opcua_statuscode_t mu_server_add_audit_callback(mu_server_t *server,
    mu_audit_callback_t callback, void *context);
```

Registers an additional audit callback (supports multiple). Returns:

| StatusCode | Condition |
|------------|-----------|
| `MU_STATUS_GOOD` | Callback registered successfully |
| `MU_STATUS_BAD_OUTOFMEMORY` | Maximum (4) callbacks already registered |
| `MU_STATUS_BAD_ARGUMENTSMISSING` | `server == NULL` or `callback == NULL` |

## Dispatch Contract

`mu_raise_audit_event(server, event)` behavior:

1. If `server == NULL` or `event == NULL` → return immediately (no crash)
2. If `server->config.auditing_enabled == false` → return immediately
3. Set `event->action_timestamp` to current server time
4. Iterate `server->audit_callbacks[0..count-1]` calling each callback:
   ```c
   callback(server, event, callback_context);
   ```
5. Callbacks are invoked in registration order (`set` then `add` calls)
6. Callbacks receive `const mu_audit_event_t *` — must not modify

## Server Storage

Added to `struct mu_server` in `src/core/server_internal.h`:

```c
#if MUC_OPCUA_AUDITING
#define MU_MAX_AUDIT_CALLBACKS 4
    struct {
        mu_audit_callback_t callback;
        void *context;
    } audit_callbacks[MU_MAX_AUDIT_CALLBACKS];
    size_t audit_callback_count;
#endif
```

## Feature Gate

All audit functions, callback storage, and event type definitions gate on
`MUC_OPCUA_AUDITING`. When the flag is OFF, the library has zero audit code
footprint.
