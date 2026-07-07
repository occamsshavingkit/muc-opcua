# Data Model: Nano Client

## mu_client_t

The client instance struct. Allocated by the caller, initialized by
`mu_client_init`. Contains the complete client state.

| Field | Type | Description |
|-------|------|-------------|
| config | mu_client_config_t | Client configuration (endpoint URL, timeouts, buffer pointers) |
| transport | mu_client_transport_t * | Platform TCP adapter (caller-provided) |
| security | mu_client_security_t | Security policy state |
| channel | mu_secure_channel_t | SecureChannel state (reuses server type) |
| session | mu_client_session_t | Session state (ID, auth token) |
| state | mu_client_state_t | Current client state enum |
| last_error | mu_status_code_t | Last error StatusCode |
| send_buf | mu_binary_writer_t | Outbound message buffer |
| recv_buf | mu_binary_reader_t | Inbound message buffer |

### mu_client_state_t

| State | Description |
|-------|-------------|
| MU_CLIENT_DISCONNECTED | Initial state. No connection. |
| MU_CLIENT_RESOLVING | Resolving hostname to address. |
| MU_CLIENT_CONNECTING | TCP connection in progress. |
| MU_CLIENT_CHANNEL_OPENING | OpenSecureChannel sent, awaiting response. |
| MU_CLIENT_CHANNEL_OPEN | SecureChannel established. |
| MU_CLIENT_SESSION_CREATING | CreateSession sent, awaiting response. |
| MU_CLIENT_ACTIVATING | ActivateSession sent, awaiting response. |
| MU_CLIENT_ACTIVE | Session active. Ready for Read/Browse. |
| MU_CLIENT_CLOSING | CloseSession sent, closing channel. |
| MU_CLIENT_ERROR | Error state. Caller must disconnect. |

## mu_client_config_t

| Field | Type | Description |
|-------|------|-------------|
| endpoint_url | const char * | Server endpoint URL |
| timeout_ms | uint32_t | Connection/request timeout |
| security_policy_uri | const char * | Requested security policy URI |
| send_buf | uint8_t * | Caller-provided send buffer |
| send_buf_size | size_t | Send buffer size |
| recv_buf | uint8_t * | Caller-provided receive buffer |
| recv_buf_size | size_t | Receive buffer size |

## mu_endpoint_description_t

| Field | Type | Description |
|-------|------|-------------|
| endpoint_url | mu_string_t | Endpoint URL string |
| security_policy_uri | mu_string_t | Security policy URI |
| security_mode | mu_security_mode_t | None/Sign/SignAndEncrypt |
| server_certificate | mu_byte_string_t | Server certificate |
| security_level | uint8_t | Security level hint |

## mu_read_value_t

| Field | Type | Description |
|-------|------|-------------|
| node_id | mu_node_id_t | The node that was read |
| value | mu_variant_t | The value (Variant) |
| status | mu_status_code_t | Result StatusCode |
| source_timestamp | mu_date_time_t | Source timestamp |
| source_picoseconds | uint16_t | Source picoseconds |

## mu_client_transport_t

Platform TCP adapter interface. Caller provides an implementation matching
this function pointer table.

| Field | Type | Description |
|-------|------|-------------|
| connect | mu_status_code_t (*)(void *ctx, const char *host, uint16_t port, uint32_t timeout_ms) | Open TCP connection |
| send | mu_status_code_t (*)(void *ctx, const uint8_t *data, size_t len) | Send data |
| recv | mu_status_code_t (*)(void *ctx, uint8_t *buf, size_t *len) | Receive data (blocking with timeout) |
| close | void (*)(void *ctx) | Close TCP connection |
| ctx | void * | Opaque transport context (socket fd, etc.) |

## mu_identity_token_t

User identity token for ActivateSession. For Nano profile, only anonymous
is supported.

| Field | Type | Description |
|-------|------|-------------|
| type | mu_identity_token_type_t | Token type (anonymous, username, certificate) |
| data | void * | Token-specific data (NULL for anonymous) |

## mu_browse_result_t

| Field | Type | Description |
|-------|------|-------------|
| node_id | mu_node_id_t | Referenced node ID |
| browse_name | mu_qualified_name_t | Browse name |
| display_name | mu_localized_text_t | Display name |
| node_class | mu_node_class_t | Node class |
| type_definition | mu_node_id_t | Type definition node |
| reference_type_id | mu_node_id_t | Reference type ID |
