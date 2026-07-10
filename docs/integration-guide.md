# Integration Guide: Embedding muc-opcua in Firmware

This guide is for an integrator putting the **muc-opcua** server library onto
their own microcontroller. It is a hands-on, code-first companion to the
architecture and conformance docs: it tells you exactly which memory you must
provide, which platform callbacks you must implement, how to declare a static
address space, how to drive the run loop, and how to size and configure the build.

The library is freestanding C11, **no-heap**, and single-connection. There is no
`malloc` in the protocol path. Every byte of working RAM is owned and supplied by
*you*, the integrator: a server storage block plus a receive and a transmit
buffer. The library talks to the outside world only through a small set of
function-pointer *adapters* (TCP, time, entropy, and optionally crypto and
persistence) that you fill in for your platform.

> **Reading paths**
> - *Just want it running?* Read [Quick start](#1-quick-start), then
>   [The no-heap memory model](#2-the-no-heap-memory-model) and
>   [Implementing the platform adapters](#3-implementing-the-platform-adapters).
> - *Bringing up a new board?* Jump to the
>   [Porting checklist](#8-porting-checklist).
> - *Budgeting flash/RAM?* See [Build configuration](#7-build-configuration-and-footprint).

---

## Table of contents

1. [Quick start](#1-quick-start)
2. [The no-heap memory model](#2-the-no-heap-memory-model)
3. [Implementing the platform adapters](#3-implementing-the-platform-adapters)
4. [Defining the address space](#4-defining-the-address-space)
5. [The run loop](#5-the-run-loop)
6. [Security: certificates, keys, and SecurityPolicy](#6-security-certificates-keys-and-securitypolicy)
7. [Build configuration and footprint](#7-build-configuration-and-footprint)
8. [Porting checklist](#8-porting-checklist)
9. [Appendix: API quick reference](#9-appendix-api-quick-reference)

---

## 1. Quick start

A complete server is three function calls — `mu_server_init`, then
`mu_server_poll` in a loop, then `mu_server_close` — wrapped around statically
declared storage and a handful of platform callbacks. The minimal host example
(`examples/minimal_server/main.c`) and the Pico example
(`platform/pico/pico_minimal_server.c`) are the canonical references; the skeleton
below is distilled from them.

```c
#include "muc_opcua/muc_opcua.h"
#include "my_board_adapter.h"        /* your TCP/time/entropy adapters */
#include "static_address_space.h"    /* your const node tables */

/* 1. Caller-owned, statically allocated memory (no heap). */
static opcua_byte_t g_server_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];   /* 8192 */
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];   /* 8192 */

int main(void)
{
    mu_server_config_t config;
    mu_server_t *server = NULL;

    memset(&config, 0, sizeof(config));   /* zero first: NULLs the optional adapters */

    /* 2. Identity / discovery strings (must outlive the server). */
    config.endpoint_url     = "opc.tcp://0.0.0.0:4840";
    config.application_uri   = "urn:my-device:muc_opcua:server";
    config.product_uri       = "urn:muc_opcua:server";
    config.application_name   = "My Device OPC UA Server";

    /* 3. Hand the library your transport buffers. */
    config.receive_buffer       = g_recv_buffer;
    config.receive_buffer_size  = sizeof(g_recv_buffer);
    config.send_buffer          = g_send_buffer;
    config.send_buffer_size     = sizeof(g_send_buffer);

    /* 4. Protocol limits. */
    config.max_chunk_count    = MU_DEFAULT_MAX_CHUNK_COUNT;   /* 1 */
    config.max_message_size   = MU_DEFAULT_MAX_MESSAGE_SIZE;  /* 8192 */
    config.max_sessions       = MU_MAX_SESSIONS;              /* 2 */
    config.max_secure_channels = MU_MAX_SECURE_CHANNELS;     /* 1 */

    /* 5. Platform adapters (required: tcp, time, entropy). */
    my_board_adapter_init(&config.tcp_adapter,
                          &config.time_adapter,
                          &config.entropy_adapter);

    /* 6. Static address space. */
    config.address_space = &g_my_address_space;

    /* 7. Init against caller storage, then poll forever. */
    if (mu_server_init(g_server_storage, sizeof(g_server_storage),
                       &config, &server) != MU_STATUS_GOOD) {
        return 1;
    }
    for (;;) {
        mu_server_poll(server);
        my_sleep_ms(10);   /* yield; sockets are non-blocking */
    }
    /* mu_server_close(server); on shutdown paths */
}
```

The remaining sections explain each of the seven steps in depth.

---

## 2. The no-heap memory model

muc-opcua never allocates. All mutable state lives in memory you declare and
hand over at init time. There are **three** caller-owned regions.

### 2.1 The three regions

| Region | What it holds | How to size it | Declared as |
|---|---|---|---|
| **Server storage** | The opaque `struct mu_server`: connection/channel/session state, subscription arrays, secure scratch | `MU_SERVER_STORAGE_BYTES` (grows with profile — see below) | `static opcua_byte_t[MU_SERVER_STORAGE_BYTES]` |
| **Receive buffer** | One incoming TCP chunk, decoded/decrypted in place | `>= MU_MIN_CHUNK_SIZE` (8192) | `static opcua_byte_t[MU_MIN_CHUNK_SIZE]` |
| **Transmit buffer** | One outgoing chunk being encoded | `>= MU_MIN_CHUNK_SIZE` (8192) | `static opcua_byte_t[MU_MIN_CHUNK_SIZE]` |

```c
static opcua_byte_t g_server_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];
```

`MU_MIN_CHUNK_SIZE` (8192 bytes) is the protocol floor: the OPC UA transport spec
(OPC 10000-6 §7.1.2) requires a buffer of at least 8 KiB, and the Hello
`EndpointUrl` alone may be up to 4096 bytes. Do not make the transport buffers
smaller. You *may* make them larger if you raise `max_message_size`, but for a
chunk count of 1 there is no benefit.

### 2.2 `MU_SERVER_STORAGE_BYTES` grows with the feature set

The single most important rule: **size the storage block from the macro, never
from a literal.** The macro is computed at compile time from the options you
enabled, so it automatically tracks the cost of subscriptions and security
(`include/muc_opcua/config.h`):

```c
#ifdef MUC_OPCUA_SECURITY
#define MU_SERVER_SECURITY_STORAGE_BYTES (MU_SECURE_SCRATCH_SIZE + 2 * MU_CIPHER_CTX_SIZE)
#else
#define MU_SERVER_SECURITY_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_SUBSCRIPTIONS
#define MU_SERVER_STORAGE_BYTES \
    (3072 + MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES + \
     MU_SERVER_SECURITY_STORAGE_BYTES + MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES)
#else
#define MU_SERVER_STORAGE_BYTES \
    (1024 + MU_SERVER_SECURITY_STORAGE_BYTES + MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES)
#endif
```

This yields the per-profile sizes you must budget for:

| Profile | Options | `MU_SERVER_STORAGE_BYTES` |
|---|---|---|
| Nano | base | **1,280** |
| Micro | `+ MUC_OPCUA_SUBSCRIPTIONS` | **3,328** |
| Embedded 2017 | `+ MUC_OPCUA_EMBEDDED_PROFILE`, 100 monitored items, queue depth 2 | **63,240** |
| Full Featured | embedded + methods + diagnostics + dynamic nodes | **63,240** |

> **Pitfall.** If you hardcode `static opcua_byte_t storage[1024]` and later turn
> on security, `mu_server_init` returns `Bad_OutOfMemory` at runtime. The example
> deliberately uses the macro and comments on exactly this trap
> (`examples/minimal_server/main.c:18-23`). Always:
> ```c
> static opcua_byte_t g_server_storage[MU_SERVER_STORAGE_BYTES];
> ```

### 2.3 The `mu_server_init` contract

```c
opcua_statuscode_t mu_server_init(void *storage, size_t storage_size,
                                  const mu_server_config_t *config,
                                  mu_server_t **out_server);
```

- **`storage`** — pointer to your block. It must be **at least
  `MU_SERVER_STORAGE_BYTES`** and **suitably aligned for the server struct**
  (which contains pointers and `size_t`). A plain `static opcua_byte_t[]` is
  over-aligned by the compiler and is the supported way to declare it; a
  byte/offset-sliced sub-buffer is rejected. Always pass `sizeof(g_server_storage)`
  as `storage_size` so the library can verify the size for the compiled feature
  set.
- **`config`** — pointer to a fully-populated config (see below). The strings,
  buffers, adapters, and address space it points at **must outlive the server**;
  the library stores the pointers, it does not copy. `memset(&config, 0, ...)`
  first so the optional adapter pointers (`persistence_adapter`,
  `crypto_adapter`) and `address_space` default to `NULL`.
- **`out_server`** — receives the opaque handle (a pointer *into* your storage
  block) used by `mu_server_poll`/`mu_server_close`.
- **Return** — `MU_STATUS_GOOD`, or a `Bad_*` status. Common failures:
  `Bad_OutOfMemory` (storage too small), `Bad_InvalidArgument` (missing required
  adapter callback or buffer). You can also pre-flight a config without storage
  via `mu_server_config_validate(&config)`.

`mu_server_init` performs **no heap allocation and no network I/O** — it lays the
server struct out inside your block, wires the config, validates the address
space, and (for security builds) primes the secure scratch. The listening socket
is not opened until the first `mu_server_poll` (the adapter's `listen` is invoked
there).

### 2.4 A note on near-zero `.bss`

The library keeps `.data` at zero and `.bss` to ~156 bytes (an address-space
lookup-index cache and an OPN policy hand-off; see
`docs/size/feature-size-ledger.md`). It performs **no dynamic allocation** in the
protocol hot path, including the subscription engine, which uses fixed-size
arrays. The practical consequence for you: RAM usage is static and fully
predictable at link time — `MU_SERVER_STORAGE_BYTES` + two 8 KiB buffers + peak
stack.

---

## 3. Implementing the platform adapters

The library is hardware-agnostic. It reaches the network, clock, and RNG only
through the function-pointer structs in `include/muc_opcua/platform.h`. You fill
these in; the reference implementations under `src/platform/` (host POSIX/OpenSSL)
and the skeletons under `platform/pico/` and `platform/arduino/` show the shape.

Three adapters are **required**: TCP, time, entropy. Two are **optional** and
default to `NULL`: crypto (needed only for Basic256Sha256) and persistence.

### 3.1 TCP transport adapter (required)

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

Everything is **non-blocking**. The contract, derived from the host reference
(`src/platform/host_tcp_adapter.c`):

- **`listen(ctx, endpoint_url)`** — parse the port out of `opc.tcp://host:port`,
  open a listening socket in non-blocking mode, bind, and listen. Called once on
  the first poll. Return `MU_STATUS_GOOD` on success.
- **`accept(ctx, &handle)`** — if a client is waiting, set `*handle` to a non-NULL
  per-connection token and return `GOOD`. If **no** client is pending, set
  `*handle = NULL` and **still return `GOOD`** (this is the normal idle case — do
  not return an error). The handle is any `void*` you choose; the host adapter
  stuffs the fd via `(void *)(intptr_t)client_fd`.
- **`read(ctx, handle, buf, len, &n)`** — read up to `len` bytes. On would-block,
  set `*n = 0` and return `GOOD`. On a graceful peer close (0 bytes read), return
  an error (`Bad_TcpInternalError`) so the server tears the connection down. On a
  hard error, return a `Bad_*` status.
- **`write(ctx, handle, buf, len, &n)`** — write up to `len` bytes, set `*n` to the
  count actually written. On would-block, `*n = 0`, return `GOOD`.
- **`close_connection(ctx, handle)`** — close that one connection.
- **`shutdown(ctx)`** — close the listening socket and release adapter resources;
  called from `mu_server_close`.

Minimal would-block pattern (from the host adapter):

```c
static opcua_statuscode_t my_read(void *ctx, void *h,
                                  opcua_byte_t *buf, size_t len, size_t *n) {
    ssize_t res = recv(fd_of(h), buf, len, 0);
    if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { *n = 0; return MU_STATUS_GOOD; }
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    if (res == 0) return MU_STATUS_BAD_TCPINTERNALERROR; /* peer closed */
    *n = (size_t)res;
    return MU_STATUS_GOOD;
}
```

On an RTOS/lwIP target, back these with `netconn`/socket APIs in `O_NONBLOCK`
mode, or a raw-API state machine. The Pico skeleton
(`platform/pico/mu_pico_adapter.c`) currently stubs every callback to `GOOD` so
the cross-compile and lifecycle validate before a real lwIP stack is wired — use
it as the struct-wiring template, not a working transport.

### 3.2 Time adapter (required)

```c
typedef struct mu_time_adapter {
    void *context;
    opcua_datetime_t (*get_time)(void *context);      /* UTC, 100-ns ticks since 1601-01-01 */
    opcua_uint64_t   (*get_tick_ms)(void *context);   /* monotonic milliseconds */
} mu_time_adapter_t;
```

- **`get_time`** returns a Windows-epoch `DateTime` (100-nanosecond intervals
  since 1601-01-01 UTC) — the OPC UA wire format. Used for `sourceTimestamp` /
  `serverTimestamp` and the Server `CurrentTime` node. Convert from your RTC:
  `filetime = unix_seconds * 10000000ULL + 116444736000000000ULL`.
- **`get_tick_ms`** is a free-running monotonic millisecond counter used purely for
  **timeout/idle** arithmetic (session timeout, idle disconnect, subscription
  publishing intervals). It need not relate to wall-clock time; it must be
  monotonic and not wrap during a session. Back it with `to_ms_since_boot(get_absolute_time())`
  on the Pico, `xTaskGetTickCount()` on FreeRTOS, or `HAL_GetTick()` on STM32 HAL.

### 3.3 Entropy adapter (required)

```c
typedef struct mu_entropy_adapter {
    void *context;
    opcua_statuscode_t (*generate_random)(void *context, opcua_byte_t *buffer, size_t length);
} mu_entropy_adapter_t;
```

`generate_random` must fill `length` bytes with **cryptographically secure**
random data. Even SecurityPolicy None uses nonces in `CreateSession`/
`ActivateSession`; security policies derive channel keys from nonces, so weak
randomness there is exploitable. Back it with a hardware TRNG (RP2040 ROSC
randomness, STM32 `RNG` peripheral, nRF `RNG`) or a properly seeded CSPRNG.

> **Do not ship the stubs.** The example and skeletons fill a fixed pattern
> (`memset(buffer, 0xAA, length)` / `0x00`) so the build links without a TRNG.
> That is fine for a smoke test on an isolated bench and **unacceptable in
> production** — replace it before deployment.

### 3.4 Crypto adapter (optional — for Basic256Sha256, Aes128-Sha256-RsaOaep, Aes256-Sha256-RsaPss)

Leave `config.crypto_adapter = NULL` and the server offers **SecurityPolicy None
only**. Supply a `mu_crypto_adapter_t *` and the server additionally advertises
`Basic256Sha256`, `Aes128-Sha256-RsaOaep`, and `Aes256-Sha256-RsaPss` Sign and
SignAndEncrypt endpoints. The interface (`include/muc_opcua/platform.h`) is the
full set of primitives OPC 10000-7 requires for these policies. The server certificate
and private key live **inside the adapter's `context`** — the library never sees raw key material.

Callbacks you must implement:

| Callback | Primitive |
|---|---|
| `sha256` | SHA-256 digest (32 B, `MU_SHA256_LENGTH`) |
| `hmac_sha256` | HMAC-SHA-256 (key derivation, message signing) |
| `aes256_cbc_encrypt` / `decrypt` | AES-256-CBC, no padding, 16-B blocks/IV (`MU_AES_BLOCK_SIZE`) |
| `aes128_cbc_encrypt` / `decrypt` | AES-128-CBC, no padding, 16-B blocks/IV (for Aes128-Sha256-RsaOaep) |
| `rsa_sha256_sign` / `verify` | RSA PKCS#1 v1.5 + SHA-256 (sign with own key, verify with peer DER cert) |
| `rsa_pss_sha256_sign` / `verify` | RSA-PSS (RSASSA-PSS with SHA-256, salt length = 32B) (for Aes256-Sha256-RsaPss) |
| `rsa_oaep_decrypt` / `encrypt` | RSA-OAEP (MGF1-SHA1) for asymmetric secret exchange |
| `rsa_oaep_sha256_decrypt` / `encrypt` | RSA-OAEP (MGF1-SHA256) (for Aes256-Sha256-RsaPss) |
| `get_own_certificate` | return server DER certificate |
| `get_certificate_key_bits` | RSA modulus size of a cert (bits) |
| `get_certificate_thumbprint` | SHA-1 of DER cert (20 B, `MU_THUMBPRINT_LENGTH`) |

**Optional per-channel cipher context** (a perf/RAM optimization). The codec can
prepare an AES key schedule once per channel direction instead of per chunk:

```c
opcua_statuscode_t (*cipher_ctx_init)(void *context, const opcua_byte_t *key /*32B*/, opcua_byte_t *ctx_storage);
opcua_statuscode_t (*aes256_cbc_encrypt_ctx)(void *context, opcua_byte_t *ctx_storage, const opcua_byte_t *iv, const opcua_byte_t *input, size_t length, opcua_byte_t *output);
opcua_statuscode_t (*aes256_cbc_decrypt_ctx)(void *context, opcua_byte_t *ctx_storage, const opcua_byte_t *iv, const opcua_byte_t *input, size_t length, opcua_byte_t *output);
void (*cipher_ctx_free)(void *context, opcua_byte_t *ctx_storage);
```

`ctx_storage` is a `MU_CIPHER_CTX_SIZE`-byte (default 32 when compiled with OpenSSL/pointer-based adaptors, 512 otherwise) opaque scratch carved
from server storage — the library owns the memory, your backend owns the layout.
**All four may be `NULL`**; if so, the codec falls back to the stateless
`aes256_cbc_encrypt`/`decrypt`. If your backend's key-schedule struct is larger
than the default size, raise `MU_CIPHER_CTX_SIZE` with `-D` (and remember it feeds
`MU_SERVER_STORAGE_BYTES`).

**Reference and porting target.** `src/platform/host_crypto_adapter.c` is a
complete OpenSSL backend that also generates a self-signed RSA-2048 instance
certificate at init (`mu_host_crypto_adapter_init`). It is for host
development/interop; on an MCU you implement the same `mu_crypto_adapter_t` on top
of **mbedTLS** or an **ARM PSA Crypto** / hardware crypto accelerator.

### 3.5 Persistence adapter (optional)

```c
typedef struct mu_persistence_adapter {
    void *context;
    opcua_statuscode_t (*read)(void *context, const char *key,
                               opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read);
    opcua_statuscode_t (*write)(void *context, const char *key,
                                const opcua_byte_t *buffer, size_t length);
} mu_persistence_adapter_t;
```

A key/value store for certificates or keys, backed by your flash/EEPROM/LittleFS.
Optional; leave `config.persistence_adapter = NULL` if your crypto adapter holds
its own key material (the typical embedded case where cert/key are provisioned
into a secure element or baked into the image).

### 3.6 UDP adapter (optional — for PubSub)

```c
typedef struct mu_udp_adapter {
    void *context;
    opcua_statuscode_t (*init)(void *context, uint16_t port);
    opcua_statuscode_t (*send)(void *context, const opcua_byte_t *buffer, size_t buffer_size, const char *address, uint16_t port);
    void (*shutdown)(void *context);
} mu_udp_adapter_t;
```

A UDP transport adapter interface to broadcast UADP NetworkMessages over UDP/IP connectionless channels. Enable this by setting `config.pubsub.enabled = true` and supplying the `udp_adapter` interface.

The current PubSub surface is a scoped UADP/UDP Publisher plus a matching
caller-storage decoder for the same UADP NetworkMessage layout. The publisher
encodes one DataSetWriter per WriterGroup with a UADP PayloadHeader, a sized
Data Key Frame, and scalar fields encoded as OPC UA Binary Variants. The field
array is caller-owned and must outlive the registered writer group. The decoder
is for scoped Publisher/Subscriber integration, not full PubSub Subscriber profile
compliance. For decoded String, ByteString, QualifiedName, or LocalizedText
Variant fields, the input buffer must outlive decoded field values because the
binary decoder may borrow bytes from that buffer. Scope follows OPC-10000-14
§5.4.2.1/§5.4.2.2 for Subscriber message reception, §5.4.6.2.2 for UADP over UDP,
and §7.3.2.1 for UDP transport:

```c
static mu_pubsub_field_t fields[] = {
    {.value = {MU_TYPE_DOUBLE, {.d = 23.5}}},
};

config.pubsub.enabled = true;
config.pubsub.port = 4840;
config.pubsub.publisher_id = 0x12345678u;
config.pubsub.address = "239.0.0.1"; /* NULL defaults to broadcast */

mu_pubsub_writer_group_t wg = {
    .writer_group_id = 1,
    .publishing_interval_ms = 1000,
    .dataset_writer = {
        .data_set_writer_id = 1,
        .fields = fields,
        .field_count = 1,
    },
};
mu_server_add_writer_group(server, &wg);
```

Decoder callers provide the UDP payload buffer and `mu_variant_t` output slots.
Keep the input buffer alive while reading decoded variable-length field values:

```c
mu_variant_t decoded_fields[1]; /* caller-provided decode output slots */
mu_pubsub_received_message_t message = {
    .fields = decoded_fields,
    .field_capacity = 1,
    .field_count = 0,
};

opcua_statuscode_t rc =
    mu_decode_uadp_network_message(payload, payload_len, &message);
if (rc == MU_STATUS_GOOD) {
    /* message.field_count is the number of decoded slots now valid. */
    if (message.field_count == 1) {
        mu_variant_t first_value = message.fields[0];
        (void)first_value;
    }
}
```

Unsupported in this slice: PubSub security, MQTT/AMQP/JSON mappings, dynamic
PublishedDataSet and DataSetReader management, arrays, delta frames, event
messages, and multiple DataSetWriters per WriterGroup.

### 3.7 Wiring the adapters

Follow the skeleton convention (`platform/pico/mu_pico_adapter.c`): one init
function that `memset`s each struct and assigns the callbacks. The host TCP
adapter additionally `malloc`s its context — on an MCU, point `context` at a
`static` struct instead:

```c
static struct my_tcp_ctx s_tcp_ctx;   /* static, no heap */

void my_board_adapter_init(mu_tcp_adapter_t *tcp,
                           mu_time_adapter_t *time,
                           mu_entropy_adapter_t *entropy) {
    memset(tcp, 0, sizeof(*tcp));
    tcp->context          = &s_tcp_ctx;
    tcp->listen           = my_listen;
    tcp->accept           = my_accept;
    tcp->read             = my_read;
    tcp->write            = my_write;
    tcp->close_connection = my_close;
    tcp->shutdown         = my_shutdown;

    memset(time, 0, sizeof(*time));
    time->get_time    = my_get_time;
    time->get_tick_ms = my_get_tick_ms;

    memset(entropy, 0, sizeof(*entropy));
    entropy->generate_random = my_trng;
}
```

---

## 4. Defining the address space

The address space is **static const data** — node tables in flash, zero RAM
overhead. The model (`include/muc_opcua/address_space.h`) is deliberately
minimal: only `Object` and `Variable` node classes, each node carrying a NodeId,
browse/display names, a reference array, and (for variables) a value source.

### 4.1 The node and reference structs

```c
typedef struct {
    mu_nodeid_t node_id;
    mu_node_class_t node_class;        /* MU_NODECLASS_OBJECT | MU_NODECLASS_VARIABLE */
    mu_string_t browse_name;
    mu_string_t display_name;
    const mu_reference_t *references;  /* array, flash */
    size_t reference_count;
    const mu_value_source_t *value;    /* NULL for objects */
} mu_node_t;

typedef struct {
    mu_nodeid_t reference_type_id;     /* e.g. Organizes = ns0;i=35, HasComponent = ns0;i=47 */
    mu_nodeid_t target_id;
    opcua_boolean_t is_forward;        /* true = forward, false = inverse */
} mu_reference_t;
```

A `mu_nodeid_t` is `{ namespace_index, identifier_type, identifier }`; numeric ids
use the brace form `{ 0, MU_NODEID_NUMERIC, { 85 } }`. Strings are
`{ length, (const opcua_byte_t *)"..." }` (not NUL-terminated on the wire — give
the explicit byte length).

### 4.2 Value sources: static vs read-callback

A variable's value is either a baked-in constant or computed on demand:

```c
typedef enum { MU_VALUESOURCE_STATIC = 0, MU_VALUESOURCE_CALLBACK = 1 } mu_value_source_type_t;

typedef opcua_statuscode_t (*mu_read_callback_t)(void *context,
                                                 const mu_nodeid_t *node_id,
                                                 mu_variant_t *value);

typedef struct {
    mu_value_source_type_t type;
    union {
        mu_variant_t static_value;
        struct { mu_read_callback_t read; void *context; } callback;
    } data;
} mu_value_source_t;
```

**Static** (constant in flash):

```c
static const mu_value_source_t s_myvar1_value = {
    MU_VALUESOURCE_STATIC, { .static_value = { MU_TYPE_INT32, { .i32 = 42 } } }
};
```

**Callback** (live sensor reading, computed each Read). The callback fills the
`mu_variant_t` and returns a status. This is how you expose dynamic process data:

```c
static opcua_statuscode_t read_temperature(void *context,
                                           const mu_nodeid_t *node_id,
                                           mu_variant_t *value) {
    (void)context; (void)node_id;
    value->type = MU_TYPE_FLOAT;
    value->value.f = adc_read_temperature_c();
    return MU_STATUS_GOOD;
}

static const mu_value_source_t s_temp_value = {
    MU_VALUESOURCE_CALLBACK,
    { .callback = { read_temperature, /*context=*/ NULL } }
};
```

(The callback signature and invocation are exercised directly in
`tests/unit/test_address_space_callbacks.c`; the Server status/time nodes use the
same mechanism in `src/address_space/base_nodes.c`.) Arrays use the
`is_array`/`array_length` fields of `mu_variant_t` with `value.array` pointing at a
typed flash array — see the `NamespaceArray` (`String[]`) example below.

### 4.3 Assembling the table and the well-known Base Information nodes

Build the node array and wrap it in `mu_address_space_t { nodes, node_count }`.
The canonical, self-consistent example is
`examples/minimal_server/static_address_space.c`: the standard **Objects** folder
(`ns=0;i=85`) `Organizes` a single Int32 variable. Crucially it also defines the
well-known nodes a real client reads during session setup:

| NodeId | Browse name | Why it matters |
|---|---|---|
| `ns=0;i=85` | Objects | Root folder for your instances |
| `ns=0;i=2255` | NamespaceArray | Client builds its namespace table from this `String[]` |
| `ns=0;i=2254` | ServerArray | `String[]` of server URIs |
| `ns=0;i=11705` | MaxNodesPerRead | Advertises your per-Read batch cap so clients self-limit |
| `ns=0;i=11710` | MaxNodesPerBrowse | Same for Browse |
| `ns=0;i=2258` / `i=2256` | CurrentTime / ServerStatus | Provided automatically when `MUC_OPCUA_BASE_NODES` is on (callback-backed by the time adapter) |

```c
static const mu_node_t s_nodes[] = {
    { { 0, MU_NODEID_NUMERIC, { 85 } }, MU_NODECLASS_OBJECT,
      { 7, (const opcua_byte_t *)"Objects" }, { 7, (const opcua_byte_t *)"Objects" },
      s_objects_refs, 1, NULL },
    { { 1, MU_NODEID_NUMERIC, { 1000 } }, MU_NODECLASS_VARIABLE,
      { 6, (const opcua_byte_t *)"MyVar1" }, { 6, (const opcua_byte_t *)"MyVar1" },
      s_myvar1_refs, 1, &s_myvar1_value },
    /* ... NamespaceArray (2255), ServerArray (2254), OperationLimits ... */
};
const mu_address_space_t g_minimal_address_space = { s_nodes, 6 };
```

Set the `MaxNodesPerRead`/`MaxNodesPerBrowse` values to match the dispatch caps
(`MU_DISPATCH_MAX_READ_NODES` = 32, browse = 8) so a client never sends a batch
the server will truncate.

### 4.4 Dynamic Address Space (NodeManagement)

The NodeManagement service set allows adding and deleting nodes and references at runtime.

To enable dynamic node management:
1. Compile with `-DMUC_OPCUA_SERVICE_NODEMANAGEMENT=1`.
2. Configure limits via `-DMU_MAX_DYNAMIC_NODES` (default: 32) and `-DMU_MAX_DYNAMIC_REFERENCES` (default: 64). Note that this proportionally increases `MU_SERVER_STORAGE_BYTES` for your static memory budget.
3. Set `server->config.allow_node_management = true` in your server configuration. If false, NodeManagement services return `BadUserAccessDenied`.

Once enabled, clients can dynamically add variables or objects using `AddNodes` or create relationships via `AddReferences`. Dynamic items are automatically discoverable via `Browse` and `Read` alongside the static address space.

### 4.5 Validation and lookup

- `mu_address_space_validate(&as)` checks that every reference target resolves to a
  node in the space. `mu_server_init` runs this for you; call it yourself in unit
  tests. **Every reference target must resolve** or validation fails.
- Lookup is by NodeId. For up to `MU_MAX_ADDRESS_SPACE_NODES` nodes (default 64,
  `-D`-overridable; index costs 2 bytes RAM/node) the library keeps a sorted index
  for binary-search lookup; beyond that it falls back to a linear scan.

---

## 5. The run loop

muc-opcua is **cooperative and single-connection**. You drive it by calling
`mu_server_poll` repeatedly; it never blocks and never spawns threads.

```c
opcua_statuscode_t mu_server_poll(mu_server_t *server);
```

One `mu_server_poll` runs a single non-blocking iteration of the whole pipeline:
accept a pending connection, read available bytes through the TCP adapter, parse
and dispatch any complete message (Hello/OpenSecureChannel/CreateSession/
ActivateSession/Read/Browse/subscription services…), and write the response. If
nothing is pending it returns quickly.

```c
for (;;) {
    mu_server_poll(server);
    my_sleep_ms(10);   /* yield ~10 ms; sockets are non-blocking, so this caps CPU */
}
```

The 10 ms yield (host `usleep(10000)`, Pico `sleep_ms(10)`) keeps a busy-poll from
pinning the CPU. On an RTOS, run the poll in its own task and yield with
`vTaskDelay`, or block on a socket-ready event and poll on wake for lower latency.

**Single-connection model.** The server services one client connection at a time
(`max_secure_channels` = 1). It does support **≥2 concurrent sessions** on that
channel (`max_sessions` = 2), per the Micro profile. While a client is connected,
`accept` returning a new handle is handled per the connection policy; design your
application around one active peer.

**Idle / timeout behavior.** The library uses `get_tick_ms` to enforce session
timeouts and to drop idle connections, so a peer that disappears without a clean
FIN is eventually reaped — provided your monotonic clock advances. There is no
work for you to do here beyond supplying a correct `get_tick_ms`.

**Shutdown.** Call `mu_server_close(server)` to close all connections and invoke
the TCP adapter's `shutdown`. After that the storage block is free to reuse or
re-init.

---

## 6. Security: certificates, keys, SecurityPolicy, and TrustLists

The authoritative conformance reference is
[`docs/conformance/security.md`](conformance/security.md). Integration essentials:

- **SecurityPolicy selection is automatic from the crypto adapter.** No crypto
  adapter (`config.crypto_adapter == NULL`) ⇒ **None only**. With a crypto adapter
  ⇒ the server *also* advertises `Basic256Sha256`, `Aes128-Sha256-RsaOaep`, and `Aes256-Sha256-RsaPss`
  Sign and SignAndEncrypt endpoints alongside None. Build security support in with the `MUC_OPCUA_SECURITY`
  option (see §7).
- **None is non-production.** Per the conformance note, SecurityPolicy None
  endpoints are for trusted/isolated networks and bench testing only. Ship
  with strong Security Policies (or restrict None to a closed network) for anything real.
- **TrustLists.** If you provide a `config.trust_list` containing peer certificates,
  the server will reject any SecureChannel Open requests using a certificate not explicitly in that list.
  If left `NULL`, the server relies on the application handling authentication elsewhere (not recommended).
- **Certificate & key provisioning.** The server's instance certificate (DER) and
  RSA private key are owned entirely by your crypto adapter's `context`; the
  library only requests them through `get_own_certificate` /
  `rsa_sha256_sign` / `rsa_oaep_decrypt`. Two provisioning patterns:
  - *Self-signed at boot* — like the host OpenSSL reference
    (`mu_host_crypto_adapter_init` generates RSA-2048 self-signed). Convenient for
    bring-up; the client must trust it out-of-band.
  - *Provisioned* — bake a CA-issued cert/key into the image, a secure element, or
    load them via the persistence adapter. Preferred for production; keep the
    private key in a secure element / PSA key store where possible.
- **`application_uri` must match the certificate.** The `application_uri` in your
  config has to equal the URI in the certificate's SubjectAltName, or interoperable
  clients (e.g. the OPC Foundation .NET reference client) reject the session.
- **Crypto primitives:** Basic256Sha256 = SHA-256, HMAC-SHA-256, AES-256-CBC,
  RSA PKCS#1 v1.5 (SHA-256) for signatures, RSA-OAEP (MGF1-SHA1) for the
  asymmetric secret exchange — see §3.4. On an MCU, implement these over mbedTLS
  or ARM PSA Crypto / a hardware crypto block. muc-opcua ships ready-made adapters
  for OpenSSL (host), mbedTLS, and wolfSSL (`MUC_OPCUA_HAVE_{MBEDTLS,WOLFSSL}`).
- **Optional ECC SecurityPolicies** (`#ECC_curve25519`, `#ECC_nistP256`; spec 059,
  `MUC_OPCUA_ECC`, default ON for standard/full) add ephemeral-ECDH secure channels
  (X25519+Ed25519 or P-256+ECDSA) alongside the RSA policies above — same crypto
  adapter interface, an extra per-curve certificate/key via
  `get_own_ecc_certificate`. The mbedTLS backend only implements the nistP256 half
  (no Ed25519 in that library); OpenSSL and wolfSSL implement both. See
  [`docs/conformance/ecc-security-policy.md`](conformance/ecc-security-policy.md).
- **Entropy quality is part of the security model** — nonces and derived keys are
  only as strong as `generate_random`. Use a real TRNG (§3.3).

---

## 7. Build configuration and footprint

### 7.1 Choosing a profile

muc-opcua ships five named profiles, selected via `-DMUC_OPCUA_PROFILE=...`
(or the `make nano` / `make micro` / `make embedded` convenience targets for
the first three). Any individual flag can be added to or removed from a
profile's defaults on the same `cmake` invocation — see
[`docs/build-and-gating.md`](build-and-gating.md) for the full flag reference
and override mechanics (e.g. "standard minus the crypto layer").

| Profile | Service set | Key options |
|---|---|---|
| **Nano** | Core + View (Browse) + Read, SecurityPolicy None | subscriptions OFF, security OFF |
| **Micro** | Nano + data-change Subscriptions / MonitoredItems | `MUC_OPCUA_SUBSCRIPTIONS=ON` |
| **Embedded** | Micro + Basic256Sha256 | `+ MUC_OPCUA_SECURITY=ON` |
| **Standard** | Embedded + Write/History/Query/NodeManagement/PubSub/Data Access/Method Server/Auditing/Complex Types/Redundancy/Reverse Connect, optional ECC | `+ MUC_OPCUA_DATA_ACCESS=ON MUC_OPCUA_METHOD_SERVER=ON MUC_OPCUA_ECC=ON` (and more) |
| **Full** | Same feature set as Standard, larger capacity presets | same flags, larger `MU_MAX_*` defaults |

### 7.2 CMake options

From the top-level `CMakeLists.txt` and `cmake/MucOpcUaOptions.cmake`
(defaults shown):

| Option | Default | Effect |
|---|---|---|
| `MUC_OPCUA_SECURITY` | ON | Compile advanced security policies (and grow `MU_SERVER_STORAGE_BYTES`) |
| `MUC_OPCUA_SUBSCRIPTIONS` | ON | Data-change subscription engine (Micro profile) |
| `MUC_OPCUA_PUBSUB` | OFF | OPC UA PubSub UADP/UDP Publisher and scoped decoder |
| `MUC_OPCUA_BASE_NODES` | ON | Standard Base Information node set (ServerStatus, CurrentTime, …) |
| `MUC_OPCUA_SERVICE_READ` | ON | Read service |
| `MUC_OPCUA_SERVICE_BROWSE` | ON | Browse / BrowseNext / TranslateBrowsePaths |
| `MUC_OPCUA_SERVICE_DISCOVERY` | ON | GetEndpoints / FindServers |
| `MUC_OPCUA_SERVICE_REGISTER_NODES` | ON | RegisterNodes / UnregisterNodes |
| `MUC_OPCUA_LTO` | OFF | Link-time / interprocedural optimization for smaller firmware |
| `MUC_OPCUA_PLATFORM` | `host` | Target platform: `host`, `external`, `pico`, `arduino-skeleton` |
| `MUC_OPCUA_BUILD_EXAMPLES` | OFF | Build the example servers |

Turn features **off** to shrink the build for Nano-class targets, e.g.:

```sh
cmake -B build -DMUC_OPCUA_SUBSCRIPTIONS=OFF -DMUC_OPCUA_SECURITY=OFF \
      -DMUC_OPCUA_LTO=ON
```

### 7.3 Capacity and sizing `-D` knobs

These are plain `#define`s overridable with `-D…` at compile time. They trade RAM
(and a little flash) for capacity:

| Macro | Default | Meaning |
|---|---|---|
| `MU_MAX_ADDRESS_SPACE_NODES` | 64 | Nodes covered by the fast lookup index (2 B RAM/node) |
| `MU_MAX_SUBSCRIPTIONS` | 2 | Concurrent subscriptions |
| `MU_MAX_MONITORED_ITEMS` | 8 | Total monitored items |
| `MU_MAX_PUBLISH_REQUESTS` | 4 | Parked Publish requests |
| `MU_RETRANSMIT_BYTES` | 256 | Republish buffer per subscription |
| `MU_CIPHER_CTX_SIZE` | 32 / 512 | Per-channel AES context scratch (32 with pointer-based/OpenSSL adaptors, 512 otherwise) |
| `MU_SECURE_SCRATCH_SIZE` | 6144 | Server-owned secure-channel scratch (security builds) |
| `MUC_OPCUA_STATUS_STRINGS` | *(undefined)* | Define to compile `mu_status_name()` human-readable strings (costs flash; off by default for embedded). Guard logging with `#ifdef`. |

Note the subscription, cipher, and scratch knobs feed `MU_SERVER_STORAGE_BYTES`
indirectly (via the profile macros) or directly — always size your storage array
from the macro so changes propagate.

Example tightening RAM on a constrained Micro build:

```sh
cmake -B build -DMUC_OPCUA_SUBSCRIPTIONS=ON \
      -DMU_MAX_MONITORED_ITEMS=4 -DMU_MAX_SUBSCRIPTIONS=1 \
      -DMU_MAX_ADDRESS_SPACE_NODES=32
```

### 7.4 Flash / RAM budget

Measured 2026-07-10 on ARM Cortex-M0+ (RP2040), `arm-none-eabi-gcc -Os
-mcpu=cortex-m0plus -mthumb -ffunction-sections -fdata-sections`; your board TCP/IP
stack and crypto backend are extra. Reproduce with `scripts/measure_size.sh all`.
Full details in
[`docs/size/feature-size-ledger.md`](size/feature-size-ledger.md).

| Profile | Core `.text` (flash) | Caller RAM = storage + 2×8 KiB buffers | Heap |
|---|---|---|---|
| **Nano** | **17.5 KiB** (17,956 B) | 1,408 B + 16 KiB ≈ **17.4 KiB** | **0** |
| **Micro** | **28.9 KiB** (29,550 B) | 11,680 B + 16 KiB ≈ **27.4 KiB** | **0** |
| **Embedded 2017** | **53.3 KiB** (54,616 B) | 128,232 B + 16 KiB ≈ **141.2 KiB** | **0** |
| **Standard 2017** | **69.7 KiB** (71,370 B) | 741,300 B + 16 KiB ≈ **740.0 KiB** | array-valued Write/Call |
| **Full** | **69.7 KiB** (71,354 B) | 1,387,500 B + 16 KiB ≈ **1.34 MiB** | array-valued Write/Call |

Additional notes for budgeting:

- **`.bss`** and **`.data`** are 0 for `.text`; **heap is 0** on nano/micro/embedded —
  the subscription engine is fixed-size, no `malloc`. Standard/full keep the heap
  enabled specifically for array-valued Write/Call decoding.
- **Caller RAM scales with capacity presets**, not just feature flags: standard/full
  default to far larger session/subscription/monitored-item counts than
  embedded (see `MU_SERVER_STORAGE_BYTES` per profile in
  [`docs/size/feature-size-ledger.md`](size/feature-size-ledger.md)) — this is why
  Embedded→Standard is a ~5× RAM jump despite a much smaller `.text` jump. Every
  capacity is overridable independent of profile with `-DMU_MAX_*=<n>` (e.g.
  `-DMU_MAX_SESSIONS=4`) if the profile default is more than your target needs.
- **Crypto backend flash** (mbedTLS/wolfSSL/OpenSSL) is *not* included above and is
  typically the largest single addition on a Standard/Full build; ECC adds a further
  ~3.1 KB of protocol code on top when `MUC_OPCUA_ECC` is on (default for
  standard/full) — see
  [`docs/conformance/ecc-security-policy.md`](conformance/ecc-security-policy.md).
  Size it from your TLS library plus the ECC delta.
- **Peak stack:** ~5.5 KiB on the plaintext (None) path; ~12 KiB on the secured
  path (response buffer + 32-deep dispatch arrays). **Provision ≥16 KiB of stack
  for a security build.** Secured-channel scratch was moved off the stack into the
  caller storage block, capping secured-OPN stack at ~7 KiB. Full Embedded 2017
  storage is larger because it also carries the 100-item Standard DataChange capacity.
- On a 264 KiB-RAM RP2040, a full Embedded 2017 server leaves roughly 200 KiB for your
  application and network stack after protocol storage and the two default transport
  buffers.
- If RAM/stack constrained: lower the `MU_MAX_*` subscription capacities, shrink
  `MU_RETRANSMIT_BYTES`, or reduce `MU_DISPATCH_MAX_READ_NODES` (and advertise a
  smaller `MaxNodesPerRead`).

---

## 8. Porting checklist

A board bring-up, in order. The two in-tree skeletons —
`platform/pico/` (Pico SDK) and `platform/arduino/` (Arduino/PlatformIO) — give
you the file layout and the adapter-wiring template; both currently stub the
network primitives so the cross-compile and server lifecycle validate before a
real stack is attached.

1. **Compile the core.** Add `src/**/*.c` (excluding the host POSIX/OpenSSL
   adapters under `src/platform/`) and the `include/` headers to your build. It is
   freestanding C11; you supply only `<stddef.h>`/`<stdint.h>`/`<string.h>`-level
   facilities. Set `MUC_OPCUA_PLATFORM` and feature options (§7).
2. **Declare storage.** Three statics:
   `g_server_storage[MU_SERVER_STORAGE_BYTES]`, `g_recv_buffer[MU_MIN_CHUNK_SIZE]`,
   `g_send_buffer[MU_MIN_CHUNK_SIZE]`.
3. **Implement the TCP adapter** over your stack (lwIP/Pico, WIZnet/Arduino
   Ethernet, an RTOS socket layer) — non-blocking, idle = `GOOD` + zero bytes (§3.1).
4. **Implement the time adapter** — UTC FILETIME from your RTC, monotonic ms from
   your tick source (§3.2).
5. **Implement the entropy adapter** over your hardware TRNG (§3.3). Replace the
   stub before shipping.
6. **(Embedded only) implement the crypto adapter** over mbedTLS / PSA, and
   provision the instance certificate + key (§3.4, §6).
7. **Author the static address space** — node tables in flash, validate with
   `mu_address_space_validate` (§4).
8. **Populate `mu_server_config_t`** (`memset` to zero first), call
   `mu_server_init`, check the status (§2.3).
9. **Run `mu_server_poll` in a loop** with a small yield (§5). Confirm a client
   (e.g. UaExpert or the OPC Foundation reference client) can connect, browse, and
   read.
10. **Budget flash/RAM/stack** against §7.4; raise the stack to ≥16 KiB for
    security builds.

**Pico SDK specifics.** `platform/pico/CMakeLists.txt` wires the cross-compile;
`pico_minimal_server.c` shows the full `init` + `while (true) { poll; sleep_ms(10); }`
pattern. Replace the stubbed `mu_pico_adapter.c` TCP callbacks with lwIP, back
`get_tick_ms` with `to_ms_since_boot(get_absolute_time())`, and `generate_random`
with the RP2040 ROSC/SDK RNG.

**Arduino specifics.** `platform/arduino/platformio.ini` + `src/mu_arduino_adapter.c`
provide the skeleton; implement the TCP callbacks over your `EthernetServer`/
`WiFiServer` (non-blocking), `get_tick_ms` over `millis()`, and `get_time` over an
NTP/RTC source.

---

## 9. Appendix: API quick reference

### Lifecycle (`include/muc_opcua/server.h`)

```c
opcua_statuscode_t mu_server_init(void *storage, size_t storage_size,
                                  const mu_server_config_t *config,
                                  mu_server_t **out_server);
opcua_statuscode_t mu_server_poll(mu_server_t *server);
void               mu_server_close(mu_server_t *server);
opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config);
```

### `mu_server_config_t` fields

| Field | Required | Notes |
|---|---|---|
| `endpoint_url` | yes | `opc.tcp://host:port`; passed to `tcp_adapter.listen` |
| `application_uri`, `product_uri`, `application_name` | yes | Identity/discovery; `application_uri` must match the cert for security |
| `receive_buffer` / `receive_buffer_size` | yes | `>= MU_MIN_CHUNK_SIZE` |
| `send_buffer` / `send_buffer_size` | yes | `>= MU_MIN_CHUNK_SIZE` |
| `max_chunk_count`, `max_message_size` | yes | Use `MU_DEFAULT_MAX_CHUNK_COUNT` (1), `MU_DEFAULT_MAX_MESSAGE_SIZE` (8192) |
| `max_sessions`, `max_secure_channels` | yes | Must not exceed the compiled `MU_MAX_SESSIONS` (2) / `MU_MAX_SECURE_CHANNELS` (== `MU_MAX_CONNECTIONS`, 1) ceilings, or `mu_server_config_validate`/`mu_server_init` reject the config; see "Raising the concurrency limits" below |
| `tcp_adapter`, `time_adapter`, `entropy_adapter` | yes | By value; fill before init |
| `crypto_adapter` | no | `NULL` ⇒ None only; non-NULL ⇒ advertises all built security policies |
| `trust_list` | no | Array of trusted peer certificates for strict authentication |
| `persistence_adapter` | no | `NULL` if unused |
| `address_space` | no (but normally yes) | `const`, in flash |

### Key constants

| Constant | Value | Header |
|---|---|---|
| `MU_SERVER_STORAGE_BYTES` | 1280 / 3328 / 63240 (per profile) | `config.h` |
| `MU_MIN_CHUNK_SIZE` | 8192 | `config.h` |
| `MU_DEFAULT_MAX_CHUNK_COUNT` | 1 | `config.h` |
| `MU_DEFAULT_MAX_MESSAGE_SIZE` | 8192 | `config.h` |
| `MU_MAX_SESSIONS` / `MU_MAX_SECURE_CHANNELS` / `MU_MAX_CONNECTIONS` | 2 / 1 / 1 | `config.h` |
| `MU_SHA256_LENGTH` / `MU_AES_BLOCK_SIZE` / `MU_THUMBPRINT_LENGTH` | 32 / 16 / 20 | `platform.h` |

### Raising the concurrency limits

`MU_MAX_SESSIONS` and `MU_MAX_CONNECTIONS` are both `#ifndef`-guarded, so both can
be raised with a `-D` flag (e.g. `-DMUC_OPCUA_MULTIPLE_CONNECTIONS -DMU_MAX_CONNECTIONS=8`).
`MU_MAX_SECURE_CHANNELS` always equals `MU_MAX_CONNECTIONS` (one secure channel
per connection) and does not need to be set independently. Two things to know
before raising either:

- `config.max_sessions`/`config.max_secure_channels` must not exceed the
  compiled `MU_MAX_SESSIONS`/`MU_MAX_CONNECTIONS` — `mu_server_init` rejects
  the config with `Bad_InternalError` otherwise, rather than silently
  under-provisioning.
- `MU_SERVER_STORAGE_BYTES` is a flat size calibrated for the **default**
  limits; it does not automatically grow when you raise `MU_MAX_SESSIONS` or
  `MU_MAX_CONNECTIONS`. If you raise either, over-allocate the storage buffer
  passed to `mu_server_init` beyond `MU_SERVER_STORAGE_BYTES` (or size it from
  a build with your actual flags) — `mu_server_init` returns
  `Bad_OutOfMemory` cleanly if the buffer is too small, rather than
  overflowing it.

### Related docs

- [`docs/conformance/security.md`](conformance/security.md) — SecurityPolicy / production notes
- [`docs/conformance/profile-nano.md`](conformance/profile-nano.md), [`profile-micro.md`](conformance/profile-micro.md) — profile definitions
- [`docs/size/feature-size-ledger.md`](size/feature-size-ledger.md) — full footprint measurements
- `examples/minimal_server/` — host reference (config, adapters, address space)
- `platform/pico/`, `platform/arduino/` — embedded skeletons
```
