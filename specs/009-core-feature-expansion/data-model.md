# Data Model: Core Feature Expansion

**Feature**: Core Feature Expansion  
**Feature Branch**: `009-core-feature-expansion`

## Public Structures

### 1. Write Service Types

Integrators register a write handler callback in `mu_server_config_t`:

```c
typedef opcua_statuscode_t (*mu_write_handler_t)(void *handle,
                                                 const mu_nodeid_t *node_id,
                                                 opcua_uint32_t attribute_id,
                                                 const mu_variant_t *value);
```

Add these fields to `mu_server_config_t`:

```c
#ifdef MICRO_OPCUA_SERVICE_WRITE
mu_write_handler_t write_handler;
void *write_handler_handle;
#endif
```

### 2. Multiple Connections Array

In `struct mu_server` inside `src/core/server_internal.h`:

```c
#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
void *client_handles[MU_MAX_CONNECTIONS];
size_t num_active_connections;
#endif
```

### 3. X.509 Certificate Token Structure

In `include/micro_opcua/types.h`:

```c
typedef struct {
    mu_string_t policy_id;
    mu_bytestring_t certificate_data;
    mu_string_t signature_algorithm;
    mu_bytestring_t signature;
} mu_certificate_identity_token_t;
```

### 4. Simple Event Notification Structure

In `src/services/subscription_internal.h`:

```c
#ifdef MICRO_OPCUA_EVENTS
typedef struct {
    mu_nodeid_t event_type;
    mu_bytestring_t event_id;
    opcua_datetime_t time;
    mu_string_t message;
    opcua_uint16_t severity;
} mu_event_notification_t;

#define MU_MAX_EVENT_QUEUE_SIZE 8
typedef struct {
    mu_event_notification_t queue[MU_MAX_EVENT_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} mu_event_queue_t;
#endif
```
