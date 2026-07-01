# muc-opcua

**A freestanding C11, zero-heap OPC UA server for small microcontrollers.**

muc-opcua is an embedded [OPC UA](https://opcfoundation.org/) **server** library
designed to run on parts as small as an RP2040 / Arm Cortex-M0+. It speaks OPC UA TCP
(`opc.tcp`) with OPC UA Binary encoding (OPC 10000-6), never calls `malloc`, and gets
all of its OS/hardware services (TCP, clock, entropy, crypto, persistence) through
narrow caller-supplied adapter structs. A host build (POSIX + OpenSSL) is provided for
development, tests, and interop against standard OPC UA clients.

It ships in three feature tiers — **nano**, **micro**, **embedded** — that map to the
OPC Foundation 2017 device server profiles, so you compile in only the surface you need.

> **Status:** profile-*targeting*. The Nano surface is complete; Micro adds data-change
> subscriptions; SecurityPolicy Basic256Sha256 has been validated against the OPC
> Foundation .NET reference client. No formal profile-compliance claim is made until the
> OPC UA CTT passes — see [docs/conformance/](docs/conformance/).

---

## Why muc-opcua

- **Zero heap.** No `malloc` anywhere in the protocol path. The application owns all
  memory: one server-storage block plus the RX/TX buffers. Footprint is deterministic.
- **Tiny flash.** Measured snapshot (2026-06-30, reproduce with
  `scripts/measure_size.sh all`): a complete Nano server is **15.9 KiB**
  (16,278 B) of Arm Cortex-M0+ `-Os` core `.text`; Micro is **23.2 KiB**
  (23,785 B); Embedded 2017 is **42.0 KiB** (42,990 B). Static `.bss` is 0 B.
- **Freestanding & portable.** Plain C11 core with no OS assumptions. Hardware and OS
  services are injected via small adapter structs in
  [`include/muc_opcua/platform.h`](include/muc_opcua/platform.h) — bring your own
  lwIP/socket stack, RTC, RNG, and (optionally) mbedTLS/BearSSL/PSA crypto.
- **Cooperative, non-blocking.** A single `mu_server_poll()` call drives accept, read,
  dispatch, write, and subscription sampling/publishing. No threads required.
- **Spec-cited surface.** Services and security are documented against the OPC UA spec
  (OPC 10000-4 / -6 / -7); see [docs/conformance/](docs/conformance/).
- **Pay for what you use.** Per-service and per-feature CMake options drop unused code
  (Read, Browse, Discovery, subscriptions, crypto, base nodes) from the binary.

---

## Profiles

Each profile is a build configuration (`make nano|micro|embedded`, or the
`MUC_OPCUA_*` CMake options). Footprint figures are a measured snapshot on Arm
Cortex-M0+ with `-Os`; reproduce with `scripts/measure_size.sh all`. Full
methodology and breakdown are in
[docs/size/feature-size-ledger.md](docs/size/feature-size-ledger.md).

| Profile | Adds | Core `.text` (flash) | `MU_SERVER_STORAGE_BYTES` | Heap |
|---|---|---|---|---|
| **nano** | SecurityPolicy None; Discovery + Session + Read + View service sets + Base Information node set; no subscriptions | **15.9 KiB** (16,278 B) | 1,280 B | 0 |
| **micro** | nano **+** data-change subscriptions (MonitoredItems / Publish) | **23.2 KiB** (23,785 B) | 3,328 B | 0 |
| **embedded**¹ | micro **+** Basic256Sha256, Standard DataChange 2017, Base Info Type System, GetMonitoredItems/ResendData | **42.0 KiB** (42,990 B) | **63,240 B** | 0 |

¹ The **`embedded` profile is profile-targeting**, not CTT-certified. It implements the
Embedded 2017 profile surface selected in the conformance docs, but no formal compliance
claim is made until the OPC UA CTT passes. See [docs/conformance/status.md](docs/conformance/status.md).

In every profile, `.data`, `.bss`, and heap are **0** (the core has no mutable global
state). RAM is caller-provided: the `MU_SERVER_STORAGE_BYTES` block above plus two
transport buffers (default **2 × 8 KiB**).
On the RP2040's 264 KiB of RAM, a Micro server leaves roughly 244 KiB for your
application and network stack before stack and adapter storage.

---

## Quick start (host build)

```bash
# 1. Clone
git clone <your-fork-url> muc-opcua
cd muc-opcua

# 2. Build the Micro profile (Nano + subscriptions) with the example server
make micro

# 3. Run the example server (listens on opc.tcp://localhost:4840)
build/micro/examples/minimal_server
```

You should see:

```
Initializing muc-opcua Server...
Server initialized successfully. Listening on opc.tcp://localhost:4840
```

Now point any OPC UA client at **`opc.tcp://localhost:4840`** — for example
[UAExpert](https://www.unified-automation.com/products/development-tools/uaexpert.html),
the OPC Foundation .NET reference client, or `opcua-client` from `python-opcua` /
`asyncua` — connect anonymously, and browse the `Server` object, read `ServerStatus`,
or (on the micro/embedded profiles) create a subscription on a variable.

To build the other tiers: `make nano`, `make embedded`, or `make all-profiles`.
Run `make help` for a summary. The example binary always lands at
`build/<profile>/examples/minimal_server`.

---

## Integration sketch

The library never allocates. You declare the storage and buffers, fill in a config with
your platform adapters and address space, then poll. Condensed from
[`examples/minimal_server/main.c`](examples/minimal_server/main.c) and
[`include/muc_opcua/server.h`](include/muc_opcua/server.h):

```c
#include "muc_opcua/muc_opcua.h"

/* Caller-owned, no-heap storage. Size tracks the compiled feature set. */
static opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t recv_buffer[MU_MIN_CHUNK_SIZE];   /* 8 KiB default */
static opcua_byte_t send_buffer[MU_MIN_CHUNK_SIZE];

int main(void)
{
    mu_server_config_t config;
    mu_server_t *server = NULL;

    memset(&config, 0, sizeof(config));

    /* Identity / discovery */
    config.endpoint_url     = "opc.tcp://0.0.0.0:4840";
    config.application_uri   = "urn:device:muc_opcua:server";
    config.application_name  = "My Device";

    /* Caller-owned transport buffers */
    config.receive_buffer      = recv_buffer;
    config.receive_buffer_size = sizeof(recv_buffer);
    config.send_buffer         = send_buffer;
    config.send_buffer_size    = sizeof(send_buffer);

    config.max_sessions        = MU_MAX_SESSIONS;
    config.max_secure_channels = MU_MAX_SECURE_CHANNELS;

    /* Platform adapters: you implement these for your board.
       (See include/muc_opcua/platform.h.) */
    my_tcp_adapter_init(&config.tcp_adapter);          /* listen/accept/read/write/close */
    config.time_adapter.get_time    = my_get_utc_time; /* 100ns ticks since 1601 */
    config.time_adapter.get_tick_ms = my_get_tick_ms;  /* monotonic ms */
    config.entropy_adapter.generate_random = my_rng;   /* CSPRNG for nonces */
    /* config.crypto_adapter = &my_crypto;   // optional: enables Basic256Sha256 */

    /* Your nodes (static, in flash). NULL falls back to the Base Information set. */
    config.address_space = &my_address_space;

    if (mu_server_init(server_storage, sizeof(server_storage),
                       &config, &server) != MU_STATUS_GOOD) {
        return 1;
    }

    for (;;) {
        mu_server_poll(server);   /* one non-blocking iteration */
        my_sleep_ms(10);          /* cooperative pacing */
    }
}
```

Key entry points (all in [`include/muc_opcua/server.h`](include/muc_opcua/server.h)):

- `mu_server_init(storage, storage_size, config, &server)` — bind caller storage + config.
- `mu_server_poll(server)` — run one non-blocking iteration (accept/read/dispatch/write/sample).
- `mu_server_close(server)` — close all connections and shut down.
- `mu_server_config_validate(config)` — pre-flight a config.

Your data lives in a `mu_address_space_t` of `mu_node_t`s
([`include/muc_opcua/address_space.h`](include/muc_opcua/address_space.h)). Variable
values are either static or served live through a read callback
(`mu_read_callback_t`) — ideal for sensor registers.

---

## Supported OPC UA surface

muc-opcua transports OPC UA TCP (`opc.tcp`) with UA-SecureConversation and UA-Binary
encoding. Implemented service sets are profile-targeting subsets with bounded
multi-session and secure-channel capacity controlled by the build configuration:

- **Discovery** — FindServers, GetEndpoints
- **SecureChannel** — Open/CloseSecureChannel (None and Basic256Sha256)
- **Session** — Create/Activate/CloseSession (Anonymous identity)
- **Attribute** — Read
- **View** — Browse, BrowseNext, TranslateBrowsePathsToNodeIds, RegisterNodes, UnregisterNodes
- **Subscription / MonitoredItem** *(micro & embedded)* — Create/Modify/Delete
  Subscriptions, SetPublishingMode, Publish, Republish, Create/Modify/Delete
  MonitoredItems, SetMonitoringMode (data-change monitoring; the Embedded Data Change
  Subscription Server Facet)

**Security:** SecurityPolicy None everywhere; **Basic256Sha256** (Sign and
Sign&Encrypt) in the embedded profile when a crypto adapter is supplied — validated
against the OPC Foundation .NET reference client.

Full, spec-section-cited matrices:

- [docs/conformance/services.md](docs/conformance/services.md) — per-service status
- [docs/conformance/profile-nano.md](docs/conformance/profile-nano.md) — Nano profile
- [docs/conformance/profile-micro.md](docs/conformance/profile-micro.md) — Micro profile
- [docs/conformance/security.md](docs/conformance/security.md) — security notes

---

## Build & test

### CMake (direct)

The `make` targets are thin wrappers over CMake. To configure directly:

```bash
cmake -S . -B build \
  -DMUC_OPCUA_BUILD_EXAMPLES=ON \
  -DMUC_OPCUA_SUBSCRIPTIONS=ON \    # micro profile
  -DMUC_OPCUA_SECURITY=OFF          # ON adds Basic256Sha256 (embedded)
cmake --build build
```

Selected options (see [CMakeLists.txt](CMakeLists.txt) and
[cmake/MucOpcUaOptions.cmake](cmake/MucOpcUaOptions.cmake)):

| Option | Default | Purpose |
|---|---|---|
| `MUC_OPCUA_SUBSCRIPTIONS` | ON | Data-change subscription engine (Micro tier) |
| `MUC_OPCUA_SECURITY` | ON | SecurityPolicy Basic256Sha256 (Embedded tier) |
| `MUC_OPCUA_BASE_NODES` | ON | Standard Base Information node set |
| `MUC_OPCUA_SERVICE_READ` / `_BROWSE` / `_DISCOVERY` / `_REGISTER_NODES` | ON | Per-service code gating |
| `MUC_OPCUA_BUILD_EXAMPLES` / `_BUILD_TESTS` / `_BUILD_FUZZERS` | OFF | Build extras |
| `MUC_OPCUA_PLATFORM` | `host` | Target: `host`, `external`, `pico`, `arduino-skeleton` |

### Tests

```bash
make test          # configure with tests + run the full ctest suite
# or:
cmake -S . -B build/test -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/test
cd build/test && ctest --output-on-failure
```

The host build links a POSIX TCP adapter and (with `MUC_OPCUA_SECURITY=ON`) an OpenSSL
crypto adapter for interop against real OPC UA clients.

### Cross-compiling for RP2040

Set `-DMUC_OPCUA_PLATFORM=pico` with `PICO_SDK_PATH` pointing at a Pico SDK checkout
(the SDK is imported before `project()`); an Arduino skeleton lives under
[`platform/arduino`](platform/arduino).

---

## Status & limitations

- **Profile-targeting, not yet CTT-certified.** Surfaces are implemented and tested, but
  no formal compliance claim is made until the OPC UA CTT passes.
- **The `embedded` profile targets Embedded 2017.** It includes Basic256Sha256,
  Standard DataChange 2017, Base Info Type System exposure, and the required
  GetMonitoredItems/ResendData methods, but it is not CTT-verified.
- **The `full-featured` profile** adds multiplexed client connections, the Write service,
  username/X509 user tokens, dynamic nodes, arbitrary user methods, Alarms & Conditions
  events, and server diagnostics.
- **SecurityPolicy None is for trusted/isolated networks or testing only.** Use
  Basic256Sha256 (embedded profile + crypto adapter) for anything exposed.
- **Optional services are build-gated.** History, NodeManagement, Query, Write,
  Events, custom Methods, diagnostics, aggregate subscriptions, and PubSub are
  available only behind their documented CMake options and remain scoped to the
  tested subsets in [docs/conformance/services.md](docs/conformance/services.md).
- **PubSub is scoped.** The PubSub surface is a UADP/UDP publisher plus a
  caller-storage decoder for the matching Data Key Frame subset; PubSub security,
  broker mappings, JSON mappings, dynamic PubSub configuration, and full
  DataSetReader runtime management remain out of scope.

---

## Repository layout

```
include/muc_opcua/   Public API headers (server.h, platform.h, address_space.h, ...)
src/
  core/                Server state machine, dispatch, sessions, channels
  encoding/            OPC UA Binary encode/decode
  services/            Service handlers (read, view, subscription, ...)
  security/            Basic256Sha256: asym/sym chunk, key derivation, certificate
  address_space/       Base Information node set + address-space helpers
  platform/            Host POSIX TCP + OpenSSL crypto adapters (dev/interop)
  generated/           Code-generated OPC UA type/id tables
examples/minimal_server/   Runnable example server
platform/pico|arduino/     MCU platform integration
docs/                  conformance/, size/, adr/, validation/, traceability/
tests/                 Unit + integration tests (ctest), fuzzers
cmake/                 Build options, warnings, codegen, static analysis
Makefile               Profile build presets (nano/micro/embedded)
```

---

## Documentation

- [docs/getting-started.md](docs/getting-started.md) — install, build, first run
- [docs/integration-guide.md](docs/integration-guide.md) — implementing platform adapters & your address space
- [docs/architecture.md](docs/architecture.md) — internals, memory model, poll loop
- [docs/api-reference.md](docs/api-reference.md) — full API reference
- [docs/conformance/](docs/conformance/) — service & profile conformance matrices
- [docs/size/feature-size-ledger.md](docs/size/feature-size-ledger.md) — measured footprint

---

## License

Released under the [MIT License](LICENSE).
