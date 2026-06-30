# micro-opcua Public API Reference

A freestanding, no-heap OPC UA server library for embedded targets (freestanding C11).
This document is the authoritative reference for the **public API** exposed under
`include/micro_opcua/`. Every signature, struct field, enum, and macro below is copied
verbatim from the headers on branch `004-docs`.

The umbrella header pulls in the whole public surface:

```c
#include "micro_opcua/micro_opcua.h"
```

which is exactly:

```c
#include "micro_opcua/config.h"
#include "micro_opcua/opcua_types.h"
#include "micro_opcua/types.h"
#include "micro_opcua/status.h"
#include "micro_opcua/platform.h"
#include "micro_opcua/server.h"
#include "micro_opcua/encoding.h"
#include "micro_opcua/opcua_ids.h"
#include "micro_opcua/address_space.h"
```

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [`opcua_types.h` — Primitive Built-in Types](#2-opcua_typesh--primitive-built-in-types)
3. [`types.h` — Core OPC UA Value Types](#3-typesh--core-opc-ua-value-types)
4. [`status.h` — StatusCodes](#4-statush--statuscodes)
5. [`server.h` — Server Lifecycle](#5-serverh--server-lifecycle)
6. [`address_space.h` — Static Address Space](#6-address_spaceh--static-address-space)
7. [`platform.h` — Platform Adapters](#7-platformh--platform-adapters)
8. [`config.h` — Compile-Time Configuration](#8-configh--compile-time-configuration)
9. [`encoding.h` — Binary Reader / Writer](#9-encodingh--binary-reader--writer)
10. [`opcua_ids.h` — Service Node IDs](#10-opcua_idsh--service-node-ids)
11. [`services/history.h` — Historical Access](#11-serviceshistoryh--historical-access)
12. [Build-Time Feature Macros & Profiles (CMake)](#12-build-time-feature-macros--profiles-cmake)

---

## 1. Quick Start

```c
#include "micro_opcua/micro_opcua.h"

/* 1. Static storage for the server (no heap). */
static unsigned char server_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t recv_buf[8192];
static opcua_byte_t send_buf[8192];

int main(void) {
    mu_server_config_t config = {0};
    config.endpoint_url     = "opc.tcp://0.0.0.0:4840";
    config.application_uri  = "urn:example:micro-opcua";
    config.product_uri      = "urn:example:product";
    config.application_name = "Micro OPC UA Server";

    config.receive_buffer      = recv_buf;
    config.receive_buffer_size = sizeof(recv_buf);
    config.send_buffer         = send_buf;
    config.send_buffer_size    = sizeof(send_buf);

    config.max_sessions        = MU_MAX_SESSIONS;
    config.max_secure_channels = MU_MAX_SECURE_CHANNELS;

    /* Required adapters (see platform.h). */
    config.tcp_adapter     = my_tcp_adapter;
    config.time_adapter    = my_time_adapter;
    config.entropy_adapter = my_entropy_adapter;

    /* Optional: static address space, crypto, persistence. */
    config.address_space = &my_address_space;

    mu_server_t *server = NULL;
    opcua_statuscode_t rc =
        mu_server_init(server_storage, sizeof(server_storage), &config, &server);
    if (rc != MU_STATUS_GOOD) { return 1; }

    for (;;) {
        rc = mu_server_poll(server);   /* non-blocking single iteration */
        /* schedule / sleep as appropriate for your platform */
    }

    mu_server_close(server);
    return 0;
}
```

---

## 2. `opcua_types.h` — Primitive Built-in Types

Fixed-width typedefs over `<stdint.h>` / `<stdbool.h>`, matching the OPC UA built-in
types (OPC 10000-6, 5.2.1). These are the scalar building blocks used everywhere else.

| Type | Underlying | Notes |
|------|-----------|-------|
| `opcua_boolean_t` | `bool` | |
| `opcua_sbyte_t` | `int8_t` | |
| `opcua_byte_t` | `uint8_t` | The byte type for all wire buffers. |
| `opcua_int16_t` | `int16_t` | |
| `opcua_uint16_t` | `uint16_t` | |
| `opcua_int32_t` | `int32_t` | |
| `opcua_uint32_t` | `uint32_t` | |
| `opcua_int64_t` | `int64_t` | |
| `opcua_uint64_t` | `uint64_t` | |
| `opcua_float_t` | `float` | |
| `opcua_double_t` | `double` | |
| `opcua_datetime_t` | `int64_t` | 64-bit signed; 100-ns intervals since 1601-01-01 UTC. |
| `opcua_statuscode_t` | `uint32_t` | 32-bit OPC UA StatusCode (see [status.h](#4-statush--statuscodes)). |

---

## 3. `types.h` — Core OPC UA Value Types

These are the structured value types an integrator populates (e.g. in a static address
space) or inspects (e.g. inside a read callback).

### 3.1 `mu_string_t` / `mu_bytestring_t`

```c
typedef struct {
    opcua_int32_t length;        /* -1 == null string; >= 0 == byte length */
    const opcua_byte_t *data;    /* UTF-8 (string) or raw bytes (bytestring) */
} mu_string_t;

typedef struct {
    opcua_int32_t length;
    const opcua_byte_t *data;
} mu_bytestring_t;
```

**Note:** `data` is `const` — strings/bytestrings reference caller-owned (typically
`static const`) storage; the library never copies or frees them. `length == -1`
encodes a null string on the wire. On-the-wire strings are bounded by
`MU_MAX_ENCODED_STRING_LENGTH`; bounded String **variable values** by
`MU_MAX_STRING_VALUE_LENGTH` (see [config.h](#8-configh--compile-time-configuration)).

### 3.2 `mu_nodeid_type_t`

```c
typedef enum {
    MU_NODEID_NUMERIC = 0,
    MU_NODEID_STRING  = 1,
    MU_NODEID_GUID    = 2,
    MU_NODEID_OPAQUE  = 3
} mu_nodeid_type_t;
```

### 3.3 `mu_nodeid_t`

```c
typedef struct {
    opcua_uint16_t namespace_index;
    mu_nodeid_type_t identifier_type;
    union {
        opcua_uint32_t numeric;   /* when identifier_type == MU_NODEID_NUMERIC */
        mu_string_t string;       /* when identifier_type == MU_NODEID_STRING  */
    } identifier;
} mu_nodeid_t;
```

**Note:** Although the enum lists GUID and OPAQUE, the union only carries `numeric`
and `string` identifiers (Numeric and String are the variants supported for the Nano
profile).

**Convenience initializers:**

```c
/* Numeric NodeId ns=0;i=2253 (the Server object) */
mu_nodeid_t id = { 0, MU_NODEID_NUMERIC, { .numeric = 2253 } };

/* String NodeId ns=1;s="Temperature" */
static const opcua_byte_t name[] = "Temperature";
mu_nodeid_t sid = { 1, MU_NODEID_STRING,
                    { .string = { (opcua_int32_t)(sizeof(name) - 1), name } } };
```

### 3.4 `mu_qualified_name_t`

```c
typedef struct {
    opcua_uint16_t namespace_index;
    mu_string_t name;
} mu_qualified_name_t;     /* OPC 10000-6 5.2.2.13 */
```

### 3.5 `mu_localized_text_t`

```c
typedef struct {
    mu_string_t locale;
    mu_string_t text;
} mu_localized_text_t;     /* OPC 10000-6 5.2.2.14 */
```

### 3.6 `mu_builtin_type_t`

The built-in type tag used by `mu_variant_t.type`. Values match the OPC UA built-in
type IDs.

| Constant | Value | Constant | Value |
|----------|-------|----------|-------|
| `MU_TYPE_NULL` | 0 | `MU_TYPE_GUID` | 14 |
| `MU_TYPE_BOOLEAN` | 1 | `MU_TYPE_BYTESTRING` | 15 |
| `MU_TYPE_SBYTE` | 2 | `MU_TYPE_XMLELEMENT` | 16 |
| `MU_TYPE_BYTE` | 3 | `MU_TYPE_NODEID` | 17 |
| `MU_TYPE_INT16` | 4 | `MU_TYPE_EXPANDEDNODEID` | 18 |
| `MU_TYPE_UINT16` | 5 | `MU_TYPE_STATUSCODE` | 19 |
| `MU_TYPE_INT32` | 6 | `MU_TYPE_QUALIFIEDNAME` | 20 |
| `MU_TYPE_UINT32` | 7 | `MU_TYPE_LOCALIZEDTEXT` | 21 |
| `MU_TYPE_INT64` | 8 | `MU_TYPE_EXTENSIONOBJECT` | 22 |
| `MU_TYPE_UINT64` | 9 | `MU_TYPE_DATAVALUE` | 23 |
| `MU_TYPE_FLOAT` | 10 | `MU_TYPE_VARIANT` | 24 |
| `MU_TYPE_DOUBLE` | 11 | `MU_TYPE_DIAGNOSTICINFO` | 25 |
| `MU_TYPE_STRING` | 12 | | |
| `MU_TYPE_DATETIME` | 13 | | |

### 3.7 `mu_variant_t`

A tagged union holding a scalar or 1-D array value.

```c
typedef struct {
    mu_builtin_type_t type;
    union {
        opcua_boolean_t b;
        opcua_sbyte_t sb;
        opcua_byte_t by;
        opcua_int16_t i16;
        opcua_uint16_t ui16;
        opcua_int32_t i32;
        opcua_uint32_t ui32;
        opcua_int64_t i64;
        opcua_uint64_t ui64;
        opcua_float_t f;
        opcua_double_t d;
        mu_string_t str;
        mu_bytestring_t bytestr;
        opcua_datetime_t dt;
        opcua_statuscode_t status;
        mu_nodeid_t nodeid;
        mu_qualified_name_t qualified_name;
        mu_localized_text_t localized_text;
        const void *array;       /* element array when is_array (points to type[array_length]) */
    } value;
    opcua_boolean_t is_array;
    opcua_int32_t array_length;  /* element count when is_array (>= 0; -1 = null array) */
} mu_variant_t;
```

**Field map (`type` → union member):**

| `type` | Union member | C type |
|--------|--------------|--------|
| `MU_TYPE_BOOLEAN` | `.b` | `opcua_boolean_t` |
| `MU_TYPE_SBYTE` | `.sb` | `opcua_sbyte_t` |
| `MU_TYPE_BYTE` | `.by` | `opcua_byte_t` |
| `MU_TYPE_INT16` | `.i16` | `opcua_int16_t` |
| `MU_TYPE_UINT16` | `.ui16` | `opcua_uint16_t` |
| `MU_TYPE_INT32` | `.i32` | `opcua_int32_t` |
| `MU_TYPE_UINT32` | `.ui32` | `opcua_uint32_t` |
| `MU_TYPE_INT64` | `.i64` | `opcua_int64_t` |
| `MU_TYPE_UINT64` | `.ui64` | `opcua_uint64_t` |
| `MU_TYPE_FLOAT` | `.f` | `opcua_float_t` |
| `MU_TYPE_DOUBLE` | `.d` | `opcua_double_t` |
| `MU_TYPE_STRING` | `.str` | `mu_string_t` |
| `MU_TYPE_BYTESTRING` | `.bytestr` | `mu_bytestring_t` |
| `MU_TYPE_DATETIME` | `.dt` | `opcua_datetime_t` |
| `MU_TYPE_STATUSCODE` | `.status` | `opcua_statuscode_t` |
| `MU_TYPE_NODEID` | `.nodeid` | `mu_nodeid_t` |
| `MU_TYPE_QUALIFIEDNAME` | `.qualified_name` | `mu_qualified_name_t` |
| `MU_TYPE_LOCALIZEDTEXT` | `.localized_text` | `mu_localized_text_t` |
| (any, when `is_array`) | `.array` | `const void *` → `type[array_length]` |

**Notes:**
- `is_array` and `array_length` follow the union so existing scalar initializers of
  the form `{ type, { .scalar } }` keep working (`is_array` defaults to `false`).
- When `is_array` is true, `value.array` points to a contiguous array of
  `array_length` elements of the scalar C type for `type`; `array_length == -1`
  encodes a null array.

**Example — scalar:**
```c
mu_variant_t v = { MU_TYPE_DOUBLE, { .d = 23.5 } };
```

**Example — array:**
```c
static const opcua_int32_t samples[] = { 1, 2, 3, 4 };
mu_variant_t v = { MU_TYPE_INT32, { .array = samples } };
v.is_array = true;
v.array_length = 4;
```

### 3.8 `mu_datavalue_t`

OPC UA DataValue: a Variant plus status and timestamps, each with a presence flag.

```c
typedef struct {
    mu_variant_t value;
    opcua_statuscode_t status;
    opcua_datetime_t source_timestamp;
    opcua_datetime_t server_timestamp;
    opcua_boolean_t has_value;
    opcua_boolean_t has_status;
    opcua_boolean_t has_source_timestamp;
    opcua_boolean_t has_server_timestamp;
} mu_datavalue_t;
```

### 3.9 `mu_size_report_t`

Reports the no-heap memory footprint breakdown.

```c
typedef struct {
    size_t config_bytes;
    size_t connection_bytes;
    size_t channel_bytes;
    size_t session_bytes;
    size_t total_allocated;
} mu_size_report_t;
```

---

## 4. `status.h` — StatusCodes

All API functions returning `opcua_statuscode_t` use these macros. `MU_STATUS_GOOD`
(`0x00000000`) signals success; any value with the high bit set is a `Bad_` error.
Compare results against `MU_STATUS_GOOD` for the common success check.

### 4.1 Common / general

| Macro | Value |
|-------|-------|
| `MU_STATUS_GOOD` | `0x00000000` |
| `MU_STATUS_BAD_UNEXPECTEDERROR` | `0x80010000` |
| `MU_STATUS_BAD_INTERNALERROR` | `0x80020000` |
| `MU_STATUS_BAD_OUTOFMEMORY` | `0x80030000` |
| `MU_STATUS_BAD_TIMEOUT` | `0x800A0000` |
| `MU_STATUS_BAD_SERVICEUNSUPPORTED` | `0x800B0000` |
| `MU_STATUS_BAD_NOTHINGTODO` | `0x800F0000` |
| `MU_STATUS_BAD_TOOMANYOPERATIONS` | `0x80100000` |

### 4.2 Encoding / decoding

| Macro | Value |
|-------|-------|
| `MU_STATUS_BAD_ENCODINGERROR` | `0x80060000` |
| `MU_STATUS_BAD_DECODINGERROR` | `0x80070000` |
| `MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED` | `0x80080000` |
| `MU_STATUS_BAD_REQUESTTOOLARGE` | `0x80B80000` |
| `MU_STATUS_BAD_RESPONSETOOLARGE` | `0x80B90000` |

### 4.3 Security / channel / session

| Macro | Value |
|-------|-------|
| `MU_STATUS_BAD_SECURITYCHECKSFAILED` | `0x80130000` |
| `MU_STATUS_BAD_CERTIFICATEINVALID` | `0x80120000` |
| `MU_STATUS_BAD_SECURITYPOLICYREJECTED` | `0x80550000` |
| `MU_STATUS_BAD_SECURITYMODEREJECTED` | `0x80540000` |
| `MU_STATUS_BAD_SECURECHANNELIDINVALID` | `0x80210000` |
| `MU_STATUS_BAD_IDENTITYTOKENINVALID` | `0x80200000` |
| `MU_STATUS_BAD_SESSIONIDINVALID` | `0x80250000` |
| `MU_STATUS_BAD_SESSIONCLOSED` | `0x80260000` |
| `MU_STATUS_BAD_TOOMANYSESSIONS` | `0x80560000` |
| `MU_STATUS_BAD_NOTREADABLE` | `0x803A0000` |

### 4.4 Address space / attributes

| Macro | Value |
|-------|-------|
| `MU_STATUS_BAD_NODEIDUNKNOWN` | `0x80340000` |
| `MU_STATUS_BAD_ATTRIBUTEIDINVALID` | `0x80350000` |
| `MU_STATUS_BAD_NOMATCH` | `0x806F0000` |
| `MU_STATUS_BAD_NOCONTINUATIONPOINTS` | `0x80140000` |
| `MU_STATUS_BAD_CONTINUATIONPOINTINVALID` | `0x804A0000` |

### 4.5 Subscriptions / monitored items

| Macro | Value |
|-------|-------|
| `MU_STATUS_BAD_MONITOREDITEMIDINVALID` | `0x80420000` |
| `MU_STATUS_BAD_TOOMANYMONITOREDITEMS` | `0x80DB0000` |
| `MU_STATUS_BAD_TOOMANYSUBSCRIPTIONS` | `0x80DD0000` |
| `MU_STATUS_BAD_SUBSCRIPTIONIDINVALID` | `0x80280000` |

The following three are defined **only** when `MICRO_OPCUA_SUBSCRIPTIONS` is enabled:

| Macro (gated) | Value |
|---------------|-------|
| `MU_STATUS_BAD_MESSAGENOTAVAILABLE` | `0x80B50000` |
| `MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN` | `0x80B80000` |
| `MU_STATUS_BAD_TOOMANYPUBLISHREQUESTS` | `0x80580000` |

### 4.6 TCP-specific (OPC UA Connection Protocol error codes)

| Macro | Value |
|-------|-------|
| `MU_STATUS_BAD_TCPSERVERTOOBUSY` | `0x807D0000` |
| `MU_STATUS_BAD_TCPMESSAGETYPEINVALID` | `0x807E0000` |
| `MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN` | `0x807F0000` |
| `MU_STATUS_BAD_TCPMESSAGETOOLARGE` | `0x80800000` |
| `MU_STATUS_BAD_TCPNOTENOUGHRESOURCES` | `0x80810000` |
| `MU_STATUS_BAD_TCPINTERNALERROR` | `0x80820000` |
| `MU_STATUS_BAD_TCPENDPOINTURLINVALID` | `0x80830000` |

### 4.7 `mu_status_name` (optional diagnostics)

```c
const char* mu_status_name(opcua_statuscode_t status);
```

**Availability:** Declared **only** when `MICRO_OPCUA_STATUS_STRINGS` is defined.
Embedded builds leave it undefined by default to save flash. Returns a human-readable
name for a StatusCode. Do not reference this symbol in code that may be compiled
without `MICRO_OPCUA_STATUS_STRINGS`.

---

## 5. `server.h` — Server Lifecycle

### 5.1 `mu_server_t`

```c
typedef struct mu_server mu_server_t;
```

Opaque server state. **Allocated by the caller** (usually statically) to avoid heap
usage. You never see its internals; you pass the storage block to `mu_server_init`
and receive a `mu_server_t *` handle.

### 5.2 `mu_server_config_t`

```c
typedef struct {
    /* Identity and Discovery */
    const char *endpoint_url;
    const char *application_uri;
    const char *product_uri;
    const char *application_name;

    /* Buffers for networking (caller-owned to avoid heap) */
    opcua_byte_t *receive_buffer;
    size_t receive_buffer_size;
    opcua_byte_t *send_buffer;
    size_t send_buffer_size;

    /* Limits */
    opcua_uint32_t max_chunk_count;
    opcua_uint32_t max_message_size;
    opcua_uint32_t max_sessions;
    opcua_uint32_t max_secure_channels;

    /* Platform Adapters */
    mu_tcp_adapter_t tcp_adapter;
    mu_time_adapter_t time_adapter;
    mu_entropy_adapter_t entropy_adapter;

    /* Optional Adapters */
    mu_persistence_adapter_t *persistence_adapter;  /* NULL if unsupported */
    mu_crypto_adapter_t *crypto_adapter;            /* NULL if unsupported */

    /* Static Address Space (optional) */
    const mu_address_space_t *address_space;
    
    /* Authorization */
    opcua_boolean_t allow_node_management;

    /* User Authentication (optional) */
    mu_user_auth_handler_t user_auth_handler;
    void *user_auth_handler_handle;

    /* TrustList for Application Authentication (optional) */
    const mu_trust_list_t *trust_list;

#ifdef MICRO_OPCUA_PUBSUB
    /* PubSub Configuration (optional) */
    mu_pubsub_connection_t pubsub;
    mu_udp_adapter_t udp_adapter;
#endif

    /* Write Service Callback (optional) */
#ifdef MICRO_OPCUA_SERVICE_WRITE
    mu_write_handler_t write_handler;
    void *write_handler_handle;
#endif

#ifdef MICRO_OPCUA_SERVICE_HISTORY
    mu_history_adapter_t history_adapter;
#endif
} mu_server_config_t;
```

**Field reference:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `endpoint_url` | `const char *` | Yes | The endpoint URL the server advertises/listens on, e.g. `opc.tcp://host:4840`. |
| `application_uri` | `const char *` | Yes | Application instance URI (used in discovery / certificate matching). |
| `product_uri` | `const char *` | Yes | Product URI. |
| `application_name` | `const char *` | Yes | Human-readable application name. |
| `receive_buffer` | `opcua_byte_t *` | Yes | Caller-owned RX buffer for incoming chunks (no heap). |
| `receive_buffer_size` | `size_t` | Yes | Size of `receive_buffer`. Must accommodate at least one chunk (see `MU_MIN_CHUNK_SIZE`). |
| `send_buffer` | `opcua_byte_t *` | Yes | Caller-owned TX buffer for outgoing chunks. |
| `send_buffer_size` | `size_t` | Yes | Size of `send_buffer`. |
| `max_chunk_count` | `opcua_uint32_t` | Yes | Max chunks per message (default suggestion: `MU_DEFAULT_MAX_CHUNK_COUNT`). |
| `max_message_size` | `opcua_uint32_t` | Yes | Max assembled message size (default suggestion: `MU_DEFAULT_MAX_MESSAGE_SIZE`). |
| `max_sessions` | `opcua_uint32_t` | Yes | Max concurrent sessions; bounded by `MU_MAX_SESSIONS`. |
| `max_secure_channels` | `opcua_uint32_t` | Yes | Max concurrent secure channels; bounded by `MU_MAX_SECURE_CHANNELS`. |
| `tcp_adapter` | `mu_tcp_adapter_t` | Yes | Networking adapter (by value). |
| `time_adapter` | `mu_time_adapter_t` | Yes | Time/tick adapter (by value). |
| `entropy_adapter` | `mu_entropy_adapter_t` | Yes | CSPRNG adapter (by value); needed for session nonces. |
| `persistence_adapter` | `mu_persistence_adapter_t *` | No | Optional cert/key storage; `NULL` if unsupported. |
| `crypto_adapter` | `mu_crypto_adapter_t *` | No | Optional crypto; `NULL` for SecurityPolicy None only. |
| `address_space` | `const mu_address_space_t *` | No | Optional static address space; `NULL` to rely on built-in base nodes only. |
| `allow_node_management` | `opcua_boolean_t` | No | Flag indicating if dynamic node management (AddNodes, etc.) is allowed. |
| `user_auth_handler` | `mu_user_auth_handler_t` | No | Optional callback handler for username/password authentication. |
| `user_auth_handler_handle` | `void *` | No | Optional user context handle passed to `user_auth_handler`. |
| `trust_list` | `const mu_trust_list_t *` | No | Optional pointer to application certificate trust list. |
| `pubsub` | `mu_pubsub_connection_t` | No | Optional PubSub connection parameters. |
| `udp_adapter` | `mu_udp_adapter_t` | No | Optional UDP adapter for PubSub. |
| `write_handler` | `mu_write_handler_t` | No | Optional callback handler for custom Node write operations. |
| `write_handler_handle` | `void *` | No | Optional user context handle passed to `write_handler`. |
| `history_adapter` | `mu_history_adapter_t` | No | Optional history access persistence adapter. |

**Note:** Required adapters are stored **by value** in the config struct;
optional adapters/handlers are stored **by pointer/value** and must remain valid for the
server's lifetime (`NULL` disables the feature).

### 5.2.1 Callbacks & Handlers

#### `mu_user_auth_handler_t`

```c
typedef opcua_statuscode_t (*mu_user_auth_handler_t)(void *handle,
                                                     const mu_string_t *username,
                                                     const mu_bytestring_t *password,
                                                     const mu_string_t *policy_id);
```

#### `mu_write_handler_t`

```c
typedef opcua_statuscode_t (*mu_write_handler_t)(void *handle,
                                                 const mu_nodeid_t *node_id,
                                                 opcua_uint32_t attribute_id,
                                                 const mu_variant_t *value);
```

### 5.2.2 Optional Server Extension APIs

#### `mu_server_register_method_callback`

```c
#ifdef MICRO_OPCUA_CUSTOM_METHODS
opcua_statuscode_t mu_server_register_method_callback(mu_server_t *server,
                                                      const mu_nodeid_t *method_id,
                                                      mu_method_callback_t callback,
                                                      void *context);
#endif
```

#### `mu_server_trigger_event`

```c
#ifdef MICRO_OPCUA_EVENTS
opcua_statuscode_t mu_server_trigger_event(mu_server_t *server,
                                           const mu_event_notification_t *event);
#endif
```

### 5.3 `mu_server_init`

```c
opcua_statuscode_t mu_server_init(void *storage,
                                  size_t storage_size,
                                  const mu_server_config_t *config,
                                  mu_server_t **out_server);
```

Initializes the server in caller-provided static storage.

| Parameter | Description |
|-----------|-------------|
| `storage` | Pointer to a caller-owned memory block that backs `struct mu_server`. |
| `storage_size` | Size of `storage`; **must be at least `MU_SERVER_STORAGE_BYTES`**. |
| `config` | Pointer to a fully populated, validated config (see `mu_server_config_validate`). |
| `out_server` | Out-param; receives the opaque server handle on success. |

**Returns:** `MU_STATUS_GOOD` on success; a `Bad_` StatusCode otherwise (e.g. storage
too small, misaligned storage, or invalid config).

**Storage-size contract:**
- `storage_size` must be `>= MU_SERVER_STORAGE_BYTES`.
- `storage` must be aligned for the server struct, which holds pointers and `size_t`.
  A `static opcua_byte_t[]` (or larger element) is over-aligned by the compiler and is
  accepted; a deliberately mis-offset byte/offset buffer is **rejected**. Prefer
  declaring storage as a plain `static unsigned char buf[MU_SERVER_STORAGE_BYTES];`
  at file scope so the compiler gives it max alignment.
- `MU_SERVER_STORAGE_BYTES` grows when `MICRO_OPCUA_SUBSCRIPTIONS` and/or
  `MICRO_OPCUA_SECURITY` are compiled in (see [config.h](#8-configh--compile-time-configuration)).
  Always size the storage block with the macro rather than a hard-coded number so it
  tracks the build configuration.

### 5.4 `mu_server_poll`

```c
opcua_statuscode_t mu_server_poll(mu_server_t *server);
```

Runs a **single non-blocking iteration** of the server: accepting new connections,
reading available bytes, parsing/assembling chunks, dispatching services, and writing
responses.

**Returns:** `MU_STATUS_GOOD` for a normal iteration; a `Bad_` StatusCode on an error
condition.

**Poll-loop contract:**
- Non-blocking: it performs at most the work currently possible (driven by the TCP
  adapter's non-blocking `accept`/`read`/`write`) and returns promptly.
- Call it repeatedly from your main loop or a scheduler/timer task. It uses the time
  adapter's monotonic tick for timeouts (idle/session timeouts), so call it regularly.
- No internal threads, no blocking, no heap allocation.

### 5.5 `mu_server_close`

```c
void mu_server_close(mu_server_t *server);
```

Cleanly closes all connections and shuts the server down (invokes the TCP adapter's
`close_connection`/`shutdown`). After this call, the server handle and its storage may
be discarded or reused for another `mu_server_init`.

### 5.6 `mu_server_config_validate`

```c
opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config);
```

Validates a config without initializing a server (e.g. required fields/adapters
present, buffer sizes sane). Returns `MU_STATUS_GOOD` if the config is acceptable for
`mu_server_init`, otherwise a `Bad_` StatusCode.

---

## 6. `address_space.h` — Static Address Space

The address space is a flat array of `mu_node_t`, fully describable as
`static const` data — no heap, no runtime mutation required. Variable values may be
static or read on demand via a callback.

### 6.1 `mu_node_class_t`

```c
typedef enum {
    MU_NODECLASS_OBJECT = 1,
    MU_NODECLASS_VARIABLE = 2
} mu_node_class_t;
```

### 6.2 `mu_reference_t`

```c
typedef struct {
    mu_nodeid_t reference_type_id;   /* e.g. Organizes, HasComponent */
    mu_nodeid_t target_id;           /* the referenced node */
    opcua_boolean_t is_forward;      /* true = forward reference */
} mu_reference_t;
```

### 6.3 `mu_value_source_type_t`

```c
typedef enum {
    MU_VALUESOURCE_STATIC = 0,
    MU_VALUESOURCE_CALLBACK = 1
} mu_value_source_type_t;
```

### 6.4 `mu_read_callback_t`

```c
typedef opcua_statuscode_t (*mu_read_callback_t)(void *context,
                                                 const mu_nodeid_t *node_id,
                                                 mu_variant_t *value);
```

Read callback signature for a dynamic variable value.

| Parameter | Description |
|-----------|-------------|
| `context` | The opaque `context` pointer from the value source's `callback` struct. |
| `node_id` | NodeId of the variable being read. |
| `value` | Out-param; the callback fills in the current value as a `mu_variant_t`. |

**Returns:** `MU_STATUS_GOOD` if `value` was populated; a `Bad_` StatusCode (e.g.
`MU_STATUS_BAD_NOTREADABLE`) otherwise.

### 6.5 `mu_value_source_t`

```c
typedef struct {
    mu_value_source_type_t type;
    union {
        mu_variant_t static_value;               /* type == MU_VALUESOURCE_STATIC */
        struct {
            mu_read_callback_t read;             /* type == MU_VALUESOURCE_CALLBACK */
            void *context;
        } callback;
    } data;
} mu_value_source_t;
```

### 6.6 `mu_node_t`

```c
typedef struct {
    mu_nodeid_t node_id;
    mu_node_class_t node_class;
    mu_string_t browse_name;
    mu_string_t display_name;

    /* References array */
    const mu_reference_t *references;
    size_t reference_count;

    /* Optional value source for variables */
    const mu_value_source_t *value;
} mu_node_t;
```

| Field | Description |
|-------|-------------|
| `node_id` | The node's NodeId. |
| `node_class` | Object or Variable. |
| `browse_name` | BrowseName (note: stored as `mu_string_t`, namespace-less in this struct). |
| `display_name` | DisplayName text. |
| `references` | Pointer to a `const` array of references owned by the caller. |
| `reference_count` | Number of entries in `references`. |
| `value` | For variables: pointer to the value source; `NULL` for objects / valueless nodes. |

### 6.7 `mu_address_space_t`

```c
typedef struct {
    const mu_node_t *nodes;
    size_t node_count;
} mu_address_space_t;
```

### 6.8 `mu_address_space_index_t`

```c
typedef struct {
    opcua_uint16_t order[MU_MAX_ADDRESS_SPACE_NODES]; /* node indices sorted by NodeId sort key */
    size_t count;                                     /* number of indexed nodes */
    opcua_boolean_t indexed;     /* false => fall back to linear scan (node_count > cap) */
} mu_address_space_index_t;
```

A bounded NodeId index for sub-linear lookup. Entries are sorted by the NodeId sort
key (namespace, identifier type, numeric value or string hash); a binary-search hit is
confirmed with `mu_nodeid_equal`. If the address space has more than
`MU_MAX_ADDRESS_SPACE_NODES` nodes, `indexed` is `false` and lookups fall back to a
linear scan. This struct is primarily internal bookkeeping; integrators normally only
size it via `MU_MAX_ADDRESS_SPACE_NODES`.

### 6.9 Functions

```c
opcua_statuscode_t mu_address_space_validate(const mu_address_space_t *address_space);
```
Validates an address space (e.g. consistency of nodes and references). Returns
`MU_STATUS_GOOD` if valid.

```c
opcua_boolean_t mu_nodeid_equal(const mu_nodeid_t *n1, const mu_nodeid_t *n2);
```
Returns `true` if two NodeIds are equal (namespace, type, and identifier).

```c
opcua_boolean_t mu_nodeid_in_namespace(const mu_nodeid_t *node_id,
                                       opcua_uint16_t namespace_index);
```
Returns `true` if `node_id` belongs to `namespace_index`.

```c
const mu_node_t *mu_address_space_find_node(const mu_address_space_t *address_space,
                                            const mu_nodeid_t *node_id);
```
Looks up a node by NodeId. Returns a pointer to the node, or `NULL` if not found.

```c
opcua_statuscode_t mu_value_source_read(const mu_value_source_t *source,
                                        const mu_nodeid_t *node_id,
                                        mu_variant_t *value);
```
Reads a value from a value source: returns the static value, or invokes the read
callback. Returns `MU_STATUS_GOOD` and fills `value` on success.

### 6.10 Declaring a static address space

```c
#include "micro_opcua/micro_opcua.h"

/* A dynamic value read on demand. */
static opcua_statuscode_t read_temperature(void *context,
                                           const mu_nodeid_t *node_id,
                                           mu_variant_t *value) {
    (void)context; (void)node_id;
    value->type = MU_TYPE_DOUBLE;
    value->value.d = read_sensor();   /* your hardware read */
    value->is_array = false;
    return MU_STATUS_GOOD;
}

static const mu_value_source_t temp_source = {
    .type = MU_VALUESOURCE_CALLBACK,
    .data.callback = { .read = read_temperature, .context = NULL }
};

/* A static value. */
static const mu_value_source_t serial_source = {
    .type = MU_VALUESOURCE_STATIC,
    .data.static_value = { MU_TYPE_INT32, { .i32 = 42 } }
};

static const opcua_byte_t bn_temp[]  = "Temperature";
static const opcua_byte_t dn_temp[]  = "Temperature";

static const mu_node_t nodes[] = {
    {
        .node_id      = { 1, MU_NODEID_NUMERIC, { .numeric = 1001 } },
        .node_class   = MU_NODECLASS_VARIABLE,
        .browse_name  = { (opcua_int32_t)(sizeof(bn_temp) - 1), bn_temp },
        .display_name = { (opcua_int32_t)(sizeof(dn_temp) - 1), dn_temp },
        .references   = NULL,
        .reference_count = 0,
        .value        = &temp_source,
    },
    /* ... more nodes ... */
};

static const mu_address_space_t my_address_space = {
    .nodes = nodes,
    .node_count = sizeof(nodes) / sizeof(nodes[0]),
};

/* Then: config.address_space = &my_address_space; */
```

---

## 7. `platform.h` — Platform Adapters

Adapters are plain structs of function pointers plus an opaque `void *context` that is
passed back to every callback. They decouple the server from the OS/HAL. Three are
required (TCP, time, entropy); two are optional (persistence, crypto).

Adapter-related constants:

| Macro | Value | Meaning |
|-------|-------|---------|
| `MU_SHA256_LENGTH` | 32 | SHA-256 digest / HMAC-SHA256 output size. |
| `MU_AES_BLOCK_SIZE` | 16 | AES-CBC block / IV size. |
| `MU_THUMBPRINT_LENGTH` | 20 | Certificate thumbprint (SHA-1 of DER) length. |

### 7.1 `mu_tcp_adapter_t` (required)

Non-blocking TCP transport.

```c
typedef struct mu_tcp_adapter {
    void *context;
    opcua_statuscode_t (*listen)(void *context, const char *endpoint_url);
    opcua_statuscode_t (*accept)(void *context, void **connection_handle);
    opcua_statuscode_t (*read)(void *context, void *connection_handle,
                               opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read);
    opcua_statuscode_t (*write)(void *context, void *connection_handle,
                                const opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_written);
    void (*close_connection)(void *context, void *connection_handle);
    void (*shutdown)(void *context);
} mu_tcp_adapter_t;
```

| Member | Contract |
|--------|----------|
| `context` | Opaque pointer passed to every callback. |
| `listen(context, endpoint_url)` | Bind/start listening on `endpoint_url`. Returns `MU_STATUS_GOOD` on success. |
| `accept(context, connection_handle)` | Non-blocking accept. On a new connection, set `*connection_handle` to an opaque per-connection handle and return `MU_STATUS_GOOD`. When no connection is pending, leave `*connection_handle` unset and signal "none" (e.g. a `Bad_` / would-block status per the adapter convention). |
| `read(context, conn, buffer, buffer_size, bytes_read)` | Non-blocking read of up to `buffer_size` bytes into `buffer`. Set `*bytes_read` to the count read (0 if none currently available). Returns `MU_STATUS_GOOD` when no error (including the 0-byte case). |
| `write(context, conn, buffer, buffer_size, bytes_written)` | Non-blocking write of up to `buffer_size` bytes from `buffer`. Set `*bytes_written` to the count accepted (may be partial). Returns `MU_STATUS_GOOD` when no error. |
| `close_connection(context, conn)` | Close a single connection identified by `conn`. No return value. |
| `shutdown(context)` | Tear down the listening endpoint and adapter resources. No return value. |

### 7.2 `mu_time_adapter_t` (required)

```c
typedef struct mu_time_adapter {
    void *context;
    opcua_datetime_t (*get_time)(void *context);     /* UTC, 100-ns since 1601-01-01 */
    opcua_uint64_t (*get_tick_ms)(void *context);    /* monotonic millisecond ticks */
} mu_time_adapter_t;
```

| Member | Contract |
|--------|----------|
| `get_time(context)` | Returns current UTC time as `opcua_datetime_t` (100-ns intervals since 1601-01-01). Used for DataValue timestamps and ServerStatus. |
| `get_tick_ms(context)` | Returns a monotonic millisecond tick count. Used for timeouts (idle/session). Must be monotonic; absolute epoch is irrelevant. |

### 7.3 `mu_entropy_adapter_t` (required)

```c
typedef struct mu_entropy_adapter {
    void *context;
    opcua_statuscode_t (*generate_random)(void *context, opcua_byte_t *buffer, size_t length);
} mu_entropy_adapter_t;
```

| Member | Contract |
|--------|----------|
| `generate_random(context, buffer, length)` | Fill `buffer` with `length` cryptographically secure random bytes (required for session nonces). Returns `MU_STATUS_GOOD` on success. |

### 7.4 `mu_persistence_adapter_t` (optional)

For storing/loading certificates or keys. Set the config field to `NULL` if
unsupported.

```c
typedef struct mu_persistence_adapter {
    void *context;
    opcua_statuscode_t (*read)(void *context, const char *key,
                               opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read);
    opcua_statuscode_t (*write)(void *context, const char *key,
                                const opcua_byte_t *buffer, size_t length);
} mu_persistence_adapter_t;
```

| Member | Contract |
|--------|----------|
| `read(context, key, buffer, buffer_size, bytes_read)` | Read the object named `key` into `buffer` (up to `buffer_size`), set `*bytes_read`. Returns `MU_STATUS_GOOD` on success. |
| `write(context, key, buffer, length)` | Persist `length` bytes from `buffer` under `key`. Returns `MU_STATUS_GOOD` on success. |

### 7.5 `mu_crypto_adapter_t` (optional)

Primitives required by SecurityPolicy **Basic256Sha256** (OPC 10000-7). Set the config
field to `NULL` when only SecurityPolicy None is used. All buffers are caller-owned;
the server's own certificate and private key live in the adapter's `context`.

```c
typedef struct mu_crypto_adapter {
    void *context;

    opcua_statuscode_t (*sha256)(void *context,
        const opcua_byte_t *data, size_t length, opcua_byte_t *digest);

    opcua_statuscode_t (*hmac_sha256)(void *context,
        const opcua_byte_t *key, size_t key_length,
        const opcua_byte_t *data, size_t data_length, opcua_byte_t *mac);

    opcua_statuscode_t (*aes256_cbc_encrypt)(void *context,
        const opcua_byte_t *key, const opcua_byte_t *iv,
        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    opcua_statuscode_t (*aes256_cbc_decrypt)(void *context,
        const opcua_byte_t *key, const opcua_byte_t *iv,
        const opcua_byte_t *input, size_t length, opcua_byte_t *output);

    /* Optional per-channel AES context (all MAY be NULL) */
    opcua_statuscode_t (*cipher_ctx_init)(void *context,
        const opcua_byte_t *key /*32B*/, opcua_byte_t *ctx_storage);
    opcua_statuscode_t (*aes256_cbc_encrypt_ctx)(void *context,
        opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    opcua_statuscode_t (*aes256_cbc_decrypt_ctx)(void *context,
        opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    void (*cipher_ctx_free)(void *context, opcua_byte_t *ctx_storage);

    opcua_statuscode_t (*rsa_sha256_sign)(void *context,
        const opcua_byte_t *data, size_t length,
        opcua_byte_t *signature, size_t *signature_length);
    opcua_statuscode_t (*rsa_sha256_verify)(void *context,
        const opcua_byte_t *certificate, size_t certificate_length,
        const opcua_byte_t *data, size_t data_length,
        const opcua_byte_t *signature, size_t signature_length);

    opcua_statuscode_t (*rsa_oaep_decrypt)(void *context,
        const opcua_byte_t *input, size_t length,
        opcua_byte_t *output, size_t *output_length);
    opcua_statuscode_t (*rsa_oaep_encrypt)(void *context,
        const opcua_byte_t *certificate, size_t certificate_length,
        const opcua_byte_t *input, size_t length,
        opcua_byte_t *output, size_t *output_length);

    opcua_statuscode_t (*get_own_certificate)(void *context,
        const opcua_byte_t **certificate, size_t *length);
    opcua_statuscode_t (*get_certificate_key_bits)(void *context,
        const opcua_byte_t *certificate, size_t certificate_length, size_t *bits);

    opcua_statuscode_t (*get_certificate_thumbprint)(void *context,
        const opcua_byte_t *certificate, size_t certificate_length, opcua_byte_t *thumbprint);
} mu_crypto_adapter_t;
```

**Hashing / MAC:**

| Member | Contract |
|--------|----------|
| `sha256(context, data, length, digest)` | SHA-256 of `data[0..length)` → `digest` (`MU_SHA256_LENGTH` = 32 bytes). |
| `hmac_sha256(context, key, key_length, data, data_length, mac)` | HMAC-SHA-256 of `data` under `key` → `mac` (32 bytes). |

**Symmetric AES (stateless form):**

| Member | Contract |
|--------|----------|
| `aes256_cbc_encrypt(context, key, iv, input, length, output)` | AES-256-CBC, no padding. `key` 32 bytes, `iv` 16 bytes, `length` a multiple of `MU_AES_BLOCK_SIZE` (16). Encrypt `input` → `output`. |
| `aes256_cbc_decrypt(...)` | Same parameters; decrypt `input` → `output`. |

**Symmetric AES (optional per-channel context form):**

These callbacks **MAY all be NULL**, in which case the codec falls back to the
stateless `aes256_cbc_encrypt`/`aes256_cbc_decrypt`. When provided, they let a backend
prepare a key schedule once per channel/direction. `ctx_storage` is a caller-owned
buffer of `MU_CIPHER_CTX_SIZE` bytes.

| Member | Contract |
|--------|----------|
| `cipher_ctx_init(context, key /*32B*/, ctx_storage)` | Initialize a per-channel AES context in `ctx_storage` from the 32-byte `key`. |
| `aes256_cbc_encrypt_ctx(context, ctx_storage, iv, input, length, output)` | Encrypt using a prepared context. |
| `aes256_cbc_decrypt_ctx(context, ctx_storage, iv, input, length, output)` | Decrypt using a prepared context. |
| `cipher_ctx_free(context, ctx_storage)` | Release any resources held in `ctx_storage`. |

**RSA (signing/verification and OAEP):**

| Member | Contract |
|--------|----------|
| `rsa_sha256_sign(context, data, length, signature, signature_length)` | RSA PKCS#1 v1.5 with SHA-256, signing with the server private key (held in `context`). On entry `*signature_length` is the buffer size; on return the written length. |
| `rsa_sha256_verify(context, certificate, certificate_length, data, data_length, signature, signature_length)` | Verify a signature using a peer certificate (DER). Returns `MU_STATUS_GOOD` if valid. |
| `rsa_oaep_decrypt(context, input, length, output, output_length)` | RSA-OAEP (MGF1-SHA1) decrypt with the server private key. |
| `rsa_oaep_encrypt(context, certificate, certificate_length, input, length, output, output_length)` | RSA-OAEP (MGF1-SHA1) encrypt with a peer certificate (DER). |

**Certificate helpers:**

| Member | Contract |
|--------|----------|
| `get_own_certificate(context, certificate, length)` | Return a pointer to the server's own DER certificate and its length (no copy; `*certificate` points to adapter-owned memory). |
| `get_certificate_key_bits(context, certificate, certificate_length, bits)` | Return the RSA modulus size in bits for the given DER certificate. |
| `get_certificate_thumbprint(context, certificate, certificate_length, thumbprint)` | Compute the cert thumbprint = SHA-1 of the DER cert → `thumbprint` (`MU_THUMBPRINT_LENGTH` = 20 bytes). |

---

## 8. `config.h` — Compile-Time Configuration

These macros are **compile-time knobs**. Most are overridable with `-D` for tuning
embedded profiles; a few are fixed library constants. Feature toggles
(`MICRO_OPCUA_*`) are normally set by the CMake build (see
[section 11](#11-build-time-feature-macros-cmake)).

### 8.1 Overridable knobs (`-D`)

| Macro | Default | Effect |
|-------|---------|--------|
| `MU_MAX_ADDRESS_SPACE_NODES` | `64` | Maximum nodes in the address-space index before falling back to a linear scan. Index RAM scales by 2 bytes per node (`order[]` in `mu_address_space_index_t`). |
| `MU_CIPHER_CTX_SIZE` | `512` | Size (bytes) of opaque per-channel cipher-context storage. 512 bytes gives headroom for an AES-256 key schedule; backend size asserts are added with adapter use. |
| `MU_SECURE_SCRATCH_SIZE` | `6144` | Shared secure-path scratch owned by `struct mu_server` when `MICRO_OPCUA_SECURITY` is enabled. Sized for the worst-case secure response / OPN scratch (replaces former `respbody[5120]` + `opn_buf[1024]` stack buffers). |
| `MICRO_OPCUA_STATUS_STRINGS` | *(undefined)* | When defined, exposes `mu_status_name()`. OFF for embedded builds by default to save flash; supply with `-D` to enable. |

### 8.2 Fixed library constants

| Macro | Value | Meaning |
|-------|-------|---------|
| `MU_MIN_CHUNK_SIZE` | `8192` | Minimum chunk size (OPC 10000-6 7.1.2.3/7.1.2.4). |
| `MU_DEFAULT_MAX_CHUNK_COUNT` | `1` | Default `max_chunk_count`. |
| `MU_DEFAULT_MAX_MESSAGE_SIZE` | `8192` | Default `max_message_size`. |
| `MU_MAX_SESSIONS` | `2` | Max sessions (Nano profile bound). |
| `MU_MAX_SECURE_CHANNELS` | `1` | Max secure channels. |
| `MU_MAX_ENCODED_STRING_LENGTH` | `4096` | Upper bound on any String/ByteString on the wire (must fit the Hello EndpointUrl, which "shall be less than 4096 bytes"). |
| `MU_MAX_STRING_VALUE_LENGTH` | `64` | Tighter limit for a bounded String *variable value* (UTF-8 bytes); enforced at the value-source layer, not on every wire string. |

### 8.3 Storage-size macros (derived)

These compute the size of the no-heap server storage block, and **depend on the
feature toggles**. Always pass `MU_SERVER_STORAGE_BYTES` as the storage size to
`mu_server_init`.

```c
#ifdef MICRO_OPCUA_SECURITY
/* secure_scratch + 2 per-direction prepared cipher contexts (in client/server keys) */
#define MU_SERVER_SECURITY_STORAGE_BYTES (MU_SECURE_SCRATCH_SIZE + 2 * MU_CIPHER_CTX_SIZE)
#else
#define MU_SERVER_SECURITY_STORAGE_BYTES 0
#endif

#ifdef MICRO_OPCUA_SUBSCRIPTIONS
#define MU_SERVER_STORAGE_BYTES \
    (3072 + MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES + MU_SERVER_SECURITY_STORAGE_BYTES + \
     MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES + MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES + MU_EVENTS_STORAGE_BYTES)
#else
#define MU_SERVER_STORAGE_BYTES \
    (1024 + MU_SERVER_SECURITY_STORAGE_BYTES + MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES + MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES)
#endif
```

| Macro | Definition | Notes |
|-------|------------|-------|
| `MU_SERVER_SECURITY_STORAGE_BYTES` | `MU_SECURE_SCRATCH_SIZE + 2*MU_CIPHER_CTX_SIZE` when `MICRO_OPCUA_SECURITY`, else `0` | Secure scratch + two per-direction prepared cipher contexts. With defaults: `12288 + 2*512 = 13312`. |
| `MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES` | `MU_MAX_ADDRESS_SPACE_NODES * 2 + 128` | Caller-owned lookup-index storage; default `256` B. |
| `MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES` | Standard DataChange storage when `MICRO_OPCUA_SUBSCRIPTIONS_STANDARD`, else `0` | Covers the Embedded 2017 monitored-item queues and trigger links. Default Embedded 2017 capacity contributes `35,200` B. |
| `MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES` | `MU_MAX_CONNECTIONS * 2500` when multiple connections are enabled, else `0` | Bounded multi-connection state. Defaults to `10000` B. |
| `MU_EVENTS_STORAGE_BYTES` | `MU_MAX_SUBSCRIPTIONS * 700` when events are enabled, else `0` | Bounded event-queue storage. Defaults to `1400` B. |
| `MU_SERVER_STORAGE_BYTES` | Sum of all configured profile/engine storage slices | Total caller-provided memory block required for `mu_server_init`. |

**Worked totals (with default knob values):**

| `MICRO_OPCUA_SUBSCRIPTIONS` | `MICRO_OPCUA_SECURITY` | `MICRO_OPCUA_MULTIPLE_CONNECTIONS` | `MU_SERVER_STORAGE_BYTES` |
|:---:|:---:|:---:|---:|
| off | off | off | `1024 + 256 = 1280` |
| off | off | on | `1024 + 256 + 10000 = 11280` |
| off | on | off | `1024 + 13312 + 256 = 14592` |
| off | on | on | `1024 + 13312 + 256 + 10000 = 24592` |
| on | off | off | `3072 + 256 = 3328` |
| on | off | on | `3072 + 256 + 10000 = 13328` |
| on + Standard Subscriptions | on | on | `3072 + 35200 + 13312 + 256 + 10000 + 1400 = 63240` (Embedded / Full) |

---

## 9. `encoding.h` — Binary Reader / Writer

A bounded OPC UA Binary codec over caller-owned buffers, with **sticky error**
semantics: once `status` becomes a `Bad_` code, subsequent chained calls are no-ops and
preserve the first failure. Public surface for integrators who need to encode/decode
OPC UA Binary directly.

### 9.1 `mu_binary_reader_t`

```c
typedef struct {
    const opcua_byte_t *buffer;
    size_t length;
    size_t position;
    opcua_statuscode_t status;   /* sticky first failure for chained decoder calls */
} mu_binary_reader_t;

void mu_binary_reader_init(mu_binary_reader_t *reader,
                           const opcua_byte_t *buffer, size_t length);
```

### 9.2 `mu_binary_writer_t`

```c
typedef struct {
    opcua_byte_t *buffer;
    size_t length;
    size_t position;
    opcua_statuscode_t status;   /* sticky first failure for chained encoder calls */
} mu_binary_writer_t;

void mu_binary_writer_init(mu_binary_writer_t *writer,
                           opcua_byte_t *buffer, size_t length);
```

### 9.3 Scalar reads

All return `opcua_statuscode_t` (`MU_STATUS_GOOD` or a sticky `Bad_` code) and advance
`position`.

```c
opcua_statuscode_t mu_binary_read_boolean(mu_binary_reader_t *reader, opcua_boolean_t *value);
opcua_statuscode_t mu_binary_read_sbyte(mu_binary_reader_t *reader, opcua_sbyte_t *value);
opcua_statuscode_t mu_binary_read_byte(mu_binary_reader_t *reader, opcua_byte_t *value);
opcua_statuscode_t mu_binary_read_int16(mu_binary_reader_t *reader, opcua_int16_t *value);
opcua_statuscode_t mu_binary_read_uint16(mu_binary_reader_t *reader, opcua_uint16_t *value);
opcua_statuscode_t mu_binary_read_int32(mu_binary_reader_t *reader, opcua_int32_t *value);
opcua_statuscode_t mu_binary_read_uint32(mu_binary_reader_t *reader, opcua_uint32_t *value);
opcua_statuscode_t mu_binary_read_int64(mu_binary_reader_t *reader, opcua_int64_t *value);
opcua_statuscode_t mu_binary_read_uint64(mu_binary_reader_t *reader, opcua_uint64_t *value);
opcua_statuscode_t mu_binary_read_float(mu_binary_reader_t *reader, opcua_float_t *value);
opcua_statuscode_t mu_binary_read_double(mu_binary_reader_t *reader, opcua_double_t *value);
opcua_statuscode_t mu_binary_read_statuscode(mu_binary_reader_t *reader, opcua_statuscode_t *value);
```

### 9.4 Scalar writes

```c
opcua_statuscode_t mu_binary_write_boolean(mu_binary_writer_t *writer, opcua_boolean_t value);
opcua_statuscode_t mu_binary_write_sbyte(mu_binary_writer_t *writer, opcua_sbyte_t value);
opcua_statuscode_t mu_binary_write_byte(mu_binary_writer_t *writer, opcua_byte_t value);
opcua_statuscode_t mu_binary_write_int16(mu_binary_writer_t *writer, opcua_int16_t value);
opcua_statuscode_t mu_binary_write_uint16(mu_binary_writer_t *writer, opcua_uint16_t value);
opcua_statuscode_t mu_binary_write_int32(mu_binary_writer_t *writer, opcua_int32_t value);
opcua_statuscode_t mu_binary_write_uint32(mu_binary_writer_t *writer, opcua_uint32_t value);
opcua_statuscode_t mu_binary_write_int64(mu_binary_writer_t *writer, opcua_int64_t value);
opcua_statuscode_t mu_binary_write_uint64(mu_binary_writer_t *writer, opcua_uint64_t value);
opcua_statuscode_t mu_binary_write_float(mu_binary_writer_t *writer, opcua_float_t value);
opcua_statuscode_t mu_binary_write_double(mu_binary_writer_t *writer, opcua_double_t value);
opcua_statuscode_t mu_binary_write_statuscode(mu_binary_writer_t *writer, opcua_statuscode_t value);
```

### 9.5 Structured read/write

```c
opcua_statuscode_t mu_binary_read_string(mu_binary_reader_t *reader, mu_string_t *value);
opcua_statuscode_t mu_binary_write_string(mu_binary_writer_t *writer, const mu_string_t *value);
opcua_statuscode_t mu_binary_read_bytestring(mu_binary_reader_t *reader, mu_bytestring_t *value);
opcua_statuscode_t mu_binary_write_bytestring(mu_binary_writer_t *writer, const mu_bytestring_t *value);

opcua_statuscode_t mu_binary_read_nodeid(mu_binary_reader_t *reader, mu_nodeid_t *value);
opcua_statuscode_t mu_binary_write_nodeid(mu_binary_writer_t *writer, const mu_nodeid_t *value);

opcua_statuscode_t mu_binary_read_extension_object_header(mu_binary_reader_t *reader,
                                                          mu_nodeid_t *type_id, size_t *length);
opcua_statuscode_t mu_binary_write_extension_object_header(mu_binary_writer_t *writer,
                                                           const mu_nodeid_t *type_id, size_t length);
opcua_statuscode_t mu_binary_skip_extension_object(mu_binary_reader_t *reader);

opcua_statuscode_t mu_binary_read_variant(mu_binary_reader_t *reader, mu_variant_t *value);
opcua_statuscode_t mu_binary_write_variant(mu_binary_writer_t *writer, const mu_variant_t *value);

opcua_statuscode_t mu_binary_read_datavalue(mu_binary_reader_t *reader, mu_datavalue_t *value);
opcua_statuscode_t mu_binary_write_datavalue(mu_binary_writer_t *writer, const mu_datavalue_t *value);
```

**Note:** `mu_binary_read_string`/`read_bytestring` set the resulting `mu_string_t` /
`mu_bytestring_t` `data` pointer to reference **into the reader's input buffer** (no
copy); the value is valid only while that buffer lives.

**Example — decode a request prefix:**
```c
mu_binary_reader_t r;
mu_binary_reader_init(&r, recv_buf, recv_len);

opcua_uint32_t secure_channel_id;
mu_nodeid_t type_id;
mu_binary_read_uint32(&r, &secure_channel_id);
mu_binary_read_nodeid(&r, &type_id);

if (r.status != MU_STATUS_GOOD) {
    /* first failure is sticky in r.status */
}
```

---

## 10. `opcua_ids.h` — Service Node IDs

Numeric NodeIds (namespace 0) for the request/response and encoding types the server
dispatches. Useful when matching `type_id` from a decoded message header.

### 10.1 Discovery & secure channel

| Macro | ID | Macro | ID |
|-------|----|-------|----|
| `MU_ID_FINDSERVERSREQUEST` | 422 | `MU_ID_FINDSERVERSRESPONSE` | 425 |
| `MU_ID_GETENDPOINTSREQUEST` | 428 | `MU_ID_GETENDPOINTSRESPONSE` | 431 |
| `MU_ID_OPENSECURECHANNELREQUEST` | 446 | `MU_ID_OPENSECURECHANNELRESPONSE` | 449 |
| `MU_ID_CLOSESECURECHANNELREQUEST` | 452 | `MU_ID_CLOSESECURECHANNELRESPONSE` | 455 |

### 10.2 Session

| Macro | ID | Macro | ID |
|-------|----|-------|----|
| `MU_ID_CREATESESSIONREQUEST` | 461 | `MU_ID_CREATESESSIONRESPONSE` | 464 |
| `MU_ID_ACTIVATESESSIONREQUEST` | 467 | `MU_ID_ACTIVATESESSIONRESPONSE` | 470 |
| `MU_ID_CLOSESESSIONREQUEST` | 473 | `MU_ID_CLOSESESSIONRESPONSE` | 476 |

### 10.3 Misc encodings

| Macro | ID |
|-------|----|
| `MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY` | 321 |
| `MU_ID_SERVICEFAULT` | 397 (ServiceFault_Encoding_DefaultBinary) |

### 10.4 View / node-management

| Macro | ID | Macro | ID |
|-------|----|-------|----|
| `MU_ID_BROWSEREQUEST` | 527 | `MU_ID_BROWSERESPONSE` | 530 |
| `MU_ID_BROWSENEXTREQUEST` | 533 | `MU_ID_BROWSENEXTRESPONSE` | 536 |
| `MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST` | 554 | `MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE` | 557 |
| `MU_ID_REGISTERNODESREQUEST` | 560 | `MU_ID_REGISTERNODESRESPONSE` | 563 |
| `MU_ID_UNREGISTERNODESREQUEST` | 566 | `MU_ID_UNREGISTERNODESRESPONSE` | 569 |

### 10.5 Attribute & method services

| Macro | ID | Macro | ID |
|-------|----|-------|----|
| `MU_ID_READREQUEST` | 631 | `MU_ID_READRESPONSE` | 634 |
| `MU_ID_HISTORYREADREQUEST` | 664 | `MU_ID_HISTORYREADRESPONSE` | 667 |
| `MU_ID_WRITEREQUEST` | 673 | `MU_ID_WRITERESPONSE` | 676 |
| `MU_ID_CALLREQUEST` | 712 | `MU_ID_CALLRESPONSE` | 715 |

### 10.6 Subscription & monitored-item services

| Macro | ID | Macro | ID |
|-------|----|-------|----|
| `MU_ID_CREATEMONITOREDITEMSREQUEST` | 751 | `MU_ID_CREATEMONITOREDITEMSRESPONSE` | 754 |
| `MU_ID_MODIFYMONITOREDITEMSREQUEST` | 763 | `MU_ID_MODIFYMONITOREDITEMSRESPONSE` | 766 |
| `MU_ID_SETMONITORINGMODEREQUEST` | 767 | `MU_ID_SETMONITORINGMODERESPONSE` | 770 |
| `MU_ID_DELETEMONITOREDITEMSREQUEST` | 781 | `MU_ID_DELETEMONITOREDITEMSRESPONSE` | 784 |
| `MU_ID_CREATESUBSCRIPTIONREQUEST` | 787 | `MU_ID_CREATESUBSCRIPTIONRESPONSE` | 790 |
| `MU_ID_MODIFYSUBSCRIPTIONREQUEST` | 793 | `MU_ID_MODIFYSUBSCRIPTIONRESPONSE` | 796 |
| `MU_ID_SETPUBLISHINGMODEREQUEST` | 799 | `MU_ID_SETPUBLISHINGMODERESPONSE` | 802 |
| `MU_ID_PUBLISHREQUEST` | 826 | `MU_ID_PUBLISHRESPONSE` | 829 |
| `MU_ID_REPUBLISHREQUEST` | 832 | `MU_ID_REPUBLISHRESPONSE` | 835 |
| `MU_ID_DELETESUBSCRIPTIONSREQUEST` | 847 | `MU_ID_DELETESUBSCRIPTIONSRESPONSE` | 850 |

---

## 11. `services/history.h` — Historical Access

### 11.1 `mu_historical_data_point_t`

```c
typedef struct {
    opcua_datetime_t source_timestamp;
    opcua_datetime_t server_timestamp;
    opcua_statuscode_t status;
    mu_variant_t value;
} mu_historical_data_point_t;
```

Represents a single historical data point stored or retrieved from persistence.

### 11.2 `mu_history_adapter_t`

```c
typedef struct {
    opcua_statuscode_t (*read_raw_modified)(
        void *context,
        const mu_nodeid_t *node_id,
        opcua_boolean_t is_read_modified,
        opcua_datetime_t start_time,
        opcua_datetime_t end_time,
        opcua_uint32_t num_values_per_node,
        opcua_boolean_t return_bounds,
        const opcua_byte_t *cp_in,
        size_t cp_in_length,
        opcua_byte_t *cp_out,
        size_t *cp_out_length,
        mu_historical_data_point_t *data_points,
        size_t max_data_points,
        size_t *actual_data_points
    );

    opcua_statuscode_t (*update_data)(
        void *context,
        const mu_nodeid_t *node_id,
        opcua_uint32_t perform_insert_replace,
        const mu_historical_data_point_t *data_points,
        size_t data_points_count,
        opcua_statuscode_t *results
    );

    opcua_statuscode_t (*delete_raw_modified)(
        void *context,
        const mu_nodeid_t *node_id,
        opcua_boolean_t is_delete_modified,
        opcua_datetime_t start_time,
        opcua_datetime_t end_time
    );
    
    void *context;
} mu_history_adapter_t;
```

The interface mapping the server dispatcher to the platform-specific persistence engine (SD card, flash, database).

---

## 12. Build-Time Feature Macros & Profiles (CMake)

These are CMake options that configure which code is compiled. All features default **OFF** unless activated via a profile.

| CMake option | Define when ON | Default | Effect |
|--------------|----------------|---------|--------|
| `MICRO_OPCUA_PROFILE` | *(string)* | `nano` | Target OPC UA profile (`nano`, `micro`, `embedded`, `full`, `custom`). Automatically turns on the relevant set of services. |
| `MICRO_OPCUA_SECURITY` | `MICRO_OPCUA_SECURITY=1` | OFF | Build SecurityPolicy Basic256Sha256. |
| `MICRO_OPCUA_SUBSCRIPTIONS` | `MICRO_OPCUA_SUBSCRIPTIONS=1` | OFF | Build the data-change subscription engine. |
| `MICRO_OPCUA_SUBSCRIPTIONS_STANDARD` | `MICRO_OPCUA_SUBSCRIPTIONS_STANDARD=1` | OFF | Build standard subscription additions. |
| `MICRO_OPCUA_SERVICE_READ` | `MICRO_OPCUA_SERVICE_READ=1` | ON | Build the Read service. |
| `MICRO_OPCUA_SERVICE_BROWSE` | `MICRO_OPCUA_SERVICE_BROWSE=1` | ON | Build Browse + BrowseNext + TranslateBrowsePaths. |
| `MICRO_OPCUA_SERVICE_DISCOVERY` | `MICRO_OPCUA_SERVICE_DISCOVERY=1` | ON | Build GetEndpoints/FindServers. |
| `MICRO_OPCUA_SERVICE_REGISTER_NODES` | `MICRO_OPCUA_SERVICE_REGISTER_NODES=1` | OFF | Build RegisterNodes/UnregisterNodes. |
| `MICRO_OPCUA_SERVICE_WRITE` | `MICRO_OPCUA_SERVICE_WRITE=1` | OFF | Build the Write service. |
| `MICRO_OPCUA_SERVICE_HISTORY` | `MICRO_OPCUA_SERVICE_HISTORY=1` | OFF | Build Historical Access (HistoryRead/HistoryUpdate). |
| `MICRO_OPCUA_SERVICE_QUERY` | `MICRO_OPCUA_SERVICE_QUERY=1` | OFF | Build the Query services. |
| `MICRO_OPCUA_SERVICE_NODEMANAGEMENT` | `MICRO_OPCUA_SERVICE_NODEMANAGEMENT=1` | OFF | Build the optional NodeManagement service set. |
| `MICRO_OPCUA_DYNAMIC_NODES` | `MICRO_OPCUA_DYNAMIC_NODES=1` | OFF | Build AddNodes/DeleteNodes dynamic node management. |
| `MICRO_OPCUA_PUBSUB` | `MICRO_OPCUA_PUBSUB=1` | OFF | Build Publish/Subscribe capabilities. |
| `MICRO_OPCUA_CUSTOM_METHODS` | `MICRO_OPCUA_CUSTOM_METHODS=1` | OFF | Build support for custom method calls. |
| `MICRO_OPCUA_SERVER_DIAGNOSTICS` | `MICRO_OPCUA_SERVER_DIAGNOSTICS=1` | OFF | Build support for server diagnostics summary nodes. |
| `MICRO_OPCUA_EVENTS` | `MICRO_OPCUA_EVENTS=1` | OFF | Build support for event notifications. |
| `MICRO_OPCUA_BASE_NODES` | `MICRO_OPCUA_BASE_NODES=1` | OFF | Build the standard Base Information node set. |
| `MICRO_OPCUA_BASE_TYPE_SYSTEM` | `MICRO_OPCUA_BASE_TYPE_SYSTEM=1` | OFF | Expose the Base Info Type System node set. |
| `MICRO_OPCUA_LTO` | *(toolchain LTO)* | OFF | Enable link-time / interprocedural optimization. |
| `MICRO_OPCUA_OPTIMIZE_SIZE` | `-Os` | OFF | Optimize the library for size. |
| `MICRO_OPCUA_PLATFORM` | *(string)* | `host` | Target platform: `host`, `external`, `pico`, or `arduino-skeleton`. |

**Notes:**
- OpenSecureChannel and the Session services are **always present** (not gated).
- `MICRO_OPCUA_STATUS_STRINGS` (the `mu_status_name()` gate) is a source-level `-D` knob, not a CMake option; it is left undefined unless supplied.
- Because `MU_SERVER_STORAGE_BYTES` depends on enabled options, always compile your application with the **same** feature flags as the library.

---

*Generated from the public headers in `include/micro_opcua/`.*
