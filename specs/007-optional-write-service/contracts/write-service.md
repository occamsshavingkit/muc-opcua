# Interface Contract: Write Service Callback and Configuration

This document specifies the integration interface exposed to application code to support custom write operations.

---

## 1. Application Callback Type

The application must provide a function pointer matching this signature to process write requests:

```c
typedef opcua_statuscode_t (*mu_write_handler_t)(
    void *handle,
    const opcua_nodeid_t *node_id,
    const opcua_variant_t *value
);
```

### Parameters
*   `handle`: The user-defined context handle registered in the server configuration.
*   `node_id`: Pointer to the `opcua_nodeid_t` identifying the variable node being written to.
*   `value`: Pointer to the `opcua_variant_t` containing the new scalar value.

### Return Values
*   `MU_STATUS_GOOD` (`0`): The write was successfully validated and applied.
*   `MU_STATUS_BAD_NOTWRITABLE` (`0x803F0000`): The variable is read-only or not writable under current state.
*   `MU_STATUS_BAD_OUTOFRANGE` (`0x803C0000`): The written value fails range validation.
*   Any other spec-conforming `opcua_statuscode_t`.

---

## 2. Server Configuration Expansion

We will add the handler and context pointers to `mu_server_config_t` in `include/micro_opcua/server.h`:

```c
typedef struct {
    ...
    /* Optional write handler (set to NULL to disable write operations) */
    mu_write_handler_t write_handler;
    void *write_handler_handle;
} mu_server_config_t;
```

---

## 3. Serialization Interface

The following private decoding and encoding declarations will be added for internal packet processing in `src/core/service_dispatch.h` / `src/encoding/`:

```c
/* Decodes a single WriteValue entry from the stream */
opcua_statuscode_t mu_write_value_decode(mu_binary_reader_t *reader, mu_write_value_t *out_val);

/* Process the WriteRequest packet */
opcua_statuscode_t handle_write(mu_server_t *server,
                                opcua_uint32_t request_id,
                                const opcua_byte_t *body,
                                size_t body_len);
```
