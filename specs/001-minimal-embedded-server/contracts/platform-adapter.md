# Contract: Platform Adapter

## Purpose

The platform adapter isolates the portable OPC UA core from TCP, timing, entropy, persistence, logging, and optional cryptography. The core must build without POSIX, Pico SDK, Arduino, RTOS, or TLS headers.

## Adapter Rules

- Adapter callbacks must be narrow C function pointers.
- Adapter callbacks must never require heap allocation by the core.
- Blocking behavior must be bounded and documented by the adapter.
- Callback return values must distinguish success, would-block/no-data, closed, timeout, and fatal I/O error.
- Platform-specific types are hidden behind `void *user_context` or small core-defined enums.

## TCP Adapter

```c
typedef enum {
    MU_IO_OK,
    MU_IO_WOULD_BLOCK,
    MU_IO_CLOSED,
    MU_IO_TIMEOUT,
    MU_IO_FATAL
} mu_IoResult;

typedef struct {
    mu_IoResult result;
    size_t byte_count;
} mu_IoTransfer;

typedef mu_IoResult (*mu_tcp_listen_fn)(
    void *user_context,
    const char *endpoint_url
);

typedef mu_IoResult (*mu_tcp_accept_fn)(
    void *user_context,
    void **out_connection
);

typedef mu_IoTransfer (*mu_tcp_read_fn)(
    void *user_context,
    void *connection,
    uint8_t *dst,
    size_t dst_len
);

typedef mu_IoTransfer (*mu_tcp_write_fn)(
    void *user_context,
    void *connection,
    const uint8_t *src,
    size_t src_len
);

typedef void (*mu_tcp_close_fn)(
    void *user_context,
    void *connection
);

typedef struct {
    void *user_context;
    mu_tcp_listen_fn listen;
    mu_tcp_accept_fn accept;
    mu_tcp_read_fn read;
    mu_tcp_write_fn write;
    mu_tcp_close_fn close;
} mu_TcpAdapter;
```

### TCP Requirements

- Host adapter may use POSIX sockets internally, but no POSIX type crosses into public core headers.
- Pico adapter may wrap Pico SDK or lwIP APIs internally.
- Arduino adapter skeleton may return unsupported/not-ready errors but must preserve the same contract.
- The core owns no TCP buffers other than caller-provided RX/TX spans.

## Time Adapter

```c
typedef struct {
    uint64_t monotonic_ms;
    int64_t opcua_datetime_100ns;
    bool has_utc;
} mu_TimeSnapshot;

typedef mu_TimeSnapshot (*mu_time_now_fn)(void *user_context);

typedef struct {
    void *user_context;
    mu_time_now_fn now;
} mu_TimeAdapter;
```

### Time Requirements

- Monotonic time is required for timeouts and revised lifetimes.
- UTC time may be unavailable on bare-metal targets; when unavailable, responses must use documented fallback timestamp behavior.

## Entropy Adapter

```c
typedef mu_StatusCode (*mu_entropy_fill_fn)(
    void *user_context,
    uint8_t *dst,
    size_t dst_len
);

typedef struct {
    void *user_context;
    mu_entropy_fill_fn fill;
} mu_EntropyAdapter;
```

### Entropy Requirements

- Required when channel/session nonce or token generation needs unpredictable bytes.
- Deterministic host-test adapter may be used only in tests and must be labeled as non-production.

## Persistence Adapter

```c
typedef mu_StatusCode (*mu_persist_read_fn)(
    void *user_context,
    const char *key,
    uint8_t *dst,
    size_t dst_len,
    size_t *out_len
);

typedef mu_StatusCode (*mu_persist_write_fn)(
    void *user_context,
    const char *key,
    const uint8_t *src,
    size_t src_len
);

typedef struct {
    void *user_context;
    mu_persist_read_fn read;
    mu_persist_write_fn write;
} mu_PersistenceAdapter;
```

### Persistence Requirements

- Optional for v1.
- Must be present before production certificate/key storage is claimed.
- Null adapter means no persistent identity beyond caller-provided configuration.

## Crypto Adapter

```c
typedef enum {
    MU_CRYPTO_NONE,
    MU_CRYPTO_MBEDTLS,
    MU_CRYPTO_PSA,
    MU_CRYPTO_WOLFSSL,
    MU_CRYPTO_PLATFORM
} mu_CryptoBackendKind;

typedef struct {
    mu_CryptoBackendKind kind;
    void *user_context;
    mu_StatusCode (*random)(void *user_context, uint8_t *dst, size_t dst_len);
    mu_StatusCode (*verify_certificate)(void *user_context, mu_BytesView cert);
    mu_StatusCode (*sign)(void *user_context, mu_BytesView input, mu_BytesMut output);
    mu_StatusCode (*verify_signature)(void *user_context, mu_BytesView input, mu_BytesView signature);
    mu_StatusCode (*encrypt)(void *user_context, mu_BytesView input, mu_BytesMut output);
    mu_StatusCode (*decrypt)(void *user_context, mu_BytesView input, mu_BytesMut output);
} mu_CryptoAdapter;
```

### Crypto Requirements

- Null or `MU_CRYPTO_NONE` is allowed only for SecurityPolicy None and must be labeled non-production unless profile evidence permits.
- SecureChannel state machines must call through this boundary when security is enabled.
- Crypto dependency code size must be measured separately when a backend is added.

## Platform Adapter Aggregate

```c
typedef struct mu_PlatformAdapter {
    mu_TcpAdapter tcp;
    mu_TimeAdapter time;
    mu_EntropyAdapter entropy;
    mu_PersistenceAdapter persistence;
    mu_CryptoAdapter crypto;
} mu_PlatformAdapter;
```

## Required Implementations

- Host adapter for integration tests.
- Pico SDK adapter and minimal example integration.
- Arduino/PlatformIO adapter skeleton with documented unsupported portions.

## Test Obligations

- Host fake adapter tests must cover short reads, short writes, would-block, close, timeout, and fatal errors.
- Pico cross-compile must prove no core header depends on host-only APIs.
- Adapter contract tests must ensure the core can run with optional persistence/crypto/logging callbacks omitted when configuration allows it.
