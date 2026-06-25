# Contract: Public C API

## Purpose

The public API lets firmware provide all memory, platform services, identity metadata, endpoint configuration, and static address-space data required by the minimal OPC UA server. The core must be usable without an OS and without mandatory heap allocation in the protocol hot path.

## Header Layout

- `include/micro_opcua/micro_opcua.h`: umbrella include.
- `include/micro_opcua/config.h`: limits and compile-time feature flags.
- `include/micro_opcua/types.h`: scalar types, strings, NodeIds, variants, values.
- `include/micro_opcua/status.h`: OPC UA StatusCode names and numeric values.
- `include/micro_opcua/platform.h`: platform adapter declarations.
- `include/micro_opcua/address_space.h`: static address-space declarations.
- `include/micro_opcua/server.h`: server lifecycle and polling API.

## API Rules

- All public functions return `mu_StatusCode` or a documented local error enum that maps to an OPC UA StatusCode before any wire-visible response.
- The core never takes ownership of caller memory unless an API explicitly states a lifetime contract.
- Public structs use explicit sizes and counts for every buffer and array.
- Public API must compile as C11 and remain usable from C99-style code where practical.
- No public API may expose Pico SDK, Arduino, POSIX, socket, or TLS implementation types.

## Core Types

```c
typedef uint32_t mu_StatusCode;

typedef struct {
    const uint8_t *data;
    size_t len;
} mu_BytesView;

typedef struct {
    uint8_t *data;
    size_t len;
} mu_BytesMut;

typedef struct {
    const char *data;
    size_t len;
} mu_StringView;

typedef enum {
    MU_VALUE_BOOLEAN,
    MU_VALUE_INT32,
    MU_VALUE_UINT32,
    MU_VALUE_FLOAT,
    MU_VALUE_STRING
} mu_ValueKind;

typedef struct {
    uint16_t namespace_index;
    uint8_t kind;
    union {
        uint32_t numeric;
        mu_StringView string;
    } id;
} mu_NodeId;

typedef struct {
    mu_ValueKind kind;
    union {
        bool boolean_value;
        int32_t int32_value;
        uint32_t uint32_value;
        float float_value;
        mu_StringView string_value;
    } value;
} mu_Value;
```

## Static Address Space Contract

```c
typedef mu_StatusCode (*mu_ReadCallback)(
    void *user_context,
    const mu_NodeId *node_id,
    mu_Value *out_value
);

typedef enum {
    MU_VALUE_SOURCE_FIXED,
    MU_VALUE_SOURCE_CALLBACK
} mu_ValueSourceKind;

typedef struct {
    mu_ValueSourceKind kind;
    mu_Value fixed_value;
    mu_ReadCallback read_callback;
    void *user_context;
} mu_ValueSource;

typedef enum {
    MU_NODE_OBJECT,
    MU_NODE_VARIABLE
} mu_NodeClass;

typedef struct {
    mu_NodeId node_id;
    mu_NodeClass node_class;
    mu_StringView browse_name;
    mu_StringView display_name;
    mu_NodeId data_type;
    mu_ValueSource value_source;
} mu_StaticNode;

typedef struct {
    mu_NodeId source_node_id;
    mu_NodeId reference_type_id;
    bool is_forward;
    mu_NodeId target_node_id;
} mu_StaticReference;

typedef struct {
    const mu_StringView *namespace_uris;
    size_t namespace_uri_count;
    const mu_StaticNode *nodes;
    size_t node_count;
    const mu_StaticReference *references;
    size_t reference_count;
} mu_AddressSpace;
```

### Validation Requirements

- `mu_AddressSpace` validation must reject duplicate NodeIds.
- Variable String fixed values and callback results must be <= 64 encoded UTF-8 bytes in v1.
- Variable values must be scalar Boolean, Int32, UInt32, Float, or bounded String.
- All references must resolve to known static nodes or documented namespace 0 built-ins.

## Server Configuration Contract

```c
typedef struct {
    uint32_t max_clients;
    uint32_t max_secure_channels;
    uint32_t max_sessions;
    uint32_t max_read_nodes_per_request;
    uint32_t max_browse_nodes_per_request;
    uint32_t max_browse_results_per_node;
    uint32_t max_string_value_len;
} mu_ServerLimits;

typedef struct {
    mu_StringView application_uri;
    mu_StringView product_uri;
    mu_StringView application_name;
    mu_StringView endpoint_url;
    mu_StringView security_policy_uri;
    mu_BytesMut rx_buffer;
    mu_BytesMut tx_buffer;
    mu_ServerLimits limits;
    const mu_AddressSpace *address_space;
    const struct mu_PlatformAdapter *platform;
} mu_ServerConfig;

typedef struct {
    uint8_t storage[MU_SERVER_STORAGE_BYTES];
} mu_ServerStorage;

typedef struct mu_Server mu_Server;
```

### Validation Requirements

- `endpoint_url` must use the `opc.tcp` scheme.
- `rx_buffer` and `tx_buffer` must be non-null and sized for the advertised transport behavior.
- Default limits are one client, one SecureChannel, one Session, and 64 bytes for bounded String values.
- `MU_SERVER_STORAGE_BYTES` must be generated or defined from configured feature limits and documented in size reports.

## Server Lifecycle Contract

```c
mu_StatusCode mu_server_init(
    mu_ServerStorage *storage,
    const mu_ServerConfig *config,
    mu_Server **out_server
);

mu_StatusCode mu_server_validate_config(
    const mu_ServerConfig *config
);

mu_StatusCode mu_server_poll(mu_Server *server);

void mu_server_close(mu_Server *server);

mu_StatusCode mu_server_get_size_report(
    const mu_Server *server,
    struct mu_SizeReport *out_report
);
```

### Behavior Requirements

- `mu_server_init` performs configuration validation and initializes caller-provided storage.
- `mu_server_poll` performs bounded progress on platform TCP I/O and protocol state machines.
- `mu_server_close` closes the active connection/session/channel and releases only internal state.
- No lifecycle call may allocate heap memory in the protocol hot path.

## Status and Unsupported-Service Contract

```c
const char *mu_status_name(mu_StatusCode status);

mu_StatusCode mu_status_from_decode_error(uint32_t condition);

mu_StatusCode mu_status_from_service_error(uint32_t condition);
```

### Required Mappings

- Unknown or unsupported service: `Bad_ServiceUnsupported`, OPC-10000-4 7.38.2.
- Browse operation failures: OPC-10000-4 5.9.2.4 and 7.38.2.
- Read service/operation failures: OPC-10000-4 5.11.2.3 and 7.38.2.
- TCP pre-service failures: OPC-10000-6 7.1.5.
- Decode failures: mapped by decode context to transport close, service error, or operation StatusCode with traceability records.

## Compatibility Contract

The public API and generated compatibility documentation must expose:

- Role: embedded OPC UA server.
- Encoding: OPC UA Binary.
- Transport: OPC UA TCP over `opc.tcp`.
- Transport profile URI: `http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary`.
- Profile status: `profile-targeting` until verified.
- Target profile URI: `http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice`.
- SecurityPolicy behavior: SecurityPolicy None only as profile-permitted or non-production interoperability behavior.
- Supported service list and unsupported-service behavior.
