# muc-opcua

**A freestanding C11, zero-heap OPC UA server for small microcontrollers.**

muc-opcua is an embedded [OPC UA](https://opcfoundation.org/) **server** library
designed to run on parts as small as an RP2040 / Arm Cortex-M0+. It speaks OPC UA TCP
(`opc.tcp`) with OPC UA Binary encoding (OPC 10000-6), never calls `malloc`, and gets
all of its OS/hardware services (TCP, clock, entropy, crypto, persistence) through
narrow caller-supplied adapter structs. A host build (POSIX + OpenSSL) is provided for
development, tests, and interop against standard OPC UA clients.

It ships in five feature tiers — **nano**, **micro**, **embedded**, **standard**,
**full** — that map to the OPC Foundation 2017 device server profiles, so you compile
in only the surface you need.

> **Status:** profile-*targeting*. The Nano surface is complete; Micro adds data-change
> subscriptions; Standard targets the full OPC-10000-7 Standard 2017 UA Server face.
> SecurityPolicy Basic256Sha256 has been validated against the OPC Foundation .NET
> reference client. No formal profile-compliance claim is made until the OPC UA CTT
> passes — see [docs/conformance/](docs/conformance/).

---

## Why muc-opcua

- **Zero heap.** No `malloc` anywhere in the protocol path. The application owns all
  memory: one server-storage block plus the RX/TX buffers. Footprint is deterministic.
- **Tiny flash.** Measured snapshot (2026-07-10, reproduce with
  `scripts/measure_size.sh all`): a complete Nano server is **17.5 KiB**
  (17,956 B) of Arm Cortex-M0+ `-Os` core `.text`; Micro is **28.9 KiB**
  (29,550 B); Standard 2017 is **74.0 KiB** (75,827 B). Every profile has
  0 B `.data` and 0 B `.bss` — the library holds no mutable static state and
  never calls `malloc`.
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

Each profile is a CMake configuration (`make nano|micro|embedded|standard`, or
`-DMUC_OPCUA_PROFILE=...`). All figures are Arm Cortex-M0+ Thumb `-Os`; the
library archive has zero `.data`/`.bss` on every profile (heap use is per-profile,
detailed below). Reproduce with `scripts/measure_size.sh all`.

**Core `.text` (flash)** — compiled library archive (`-Os`). The linked server is
smaller after `--gc-sections` dead-code elimination.

| Profile | .text | OPC UA Profile |
|---------|-------|----------------|
| nano | 17,956 B | Nano Embedded Device 2017 |
| micro | 29,550 B | Micro Embedded Device 2017 |
| embedded | 54,616 B | Embedded 2017 UA Server |
| standard | 75,827 B | Standard 2017 UA Server |
| full | 75,811 B | — (everything on; also carries `MUC_OPCUA_ECC` + Data Access) |

Built with LTO (`MUC_OPCUA_LTO=ON`, the default). The `nano`/`micro`/`embedded`
profiles are strictly no-heap (`MUC_OPCUA_ALLOW_HEAP=OFF`): 0 B `.data`, 0 B `.bss`,
no `malloc` in the linked server. With LTO's cross-module optimization the real
linked `nano` server is **15,764 B** `.text` (`--gc-sections`), below the archive
figure above. `standard`/`full` keep the heap enabled for array-valued Write/Call.
`standard`/`full` also carry the optional ECC SecurityPolicies (spec 059, ~3.1 KB);
see [docs/conformance/ecc-security-policy.md](docs/conformance/ecc-security-policy.md)
and [docs/build-and-gating.md](docs/build-and-gating.md) to build without them.

The library declares **no mutable static state** — its archive has 0 B `.data`
and 0 B `.bss` on every profile. `nano`/`micro`/`embedded` are strictly no-heap
end-to-end: the linked server never calls `malloc` (a project-constitution
requirement) and keeps 0 B `.data`/`.bss`. `standard`/`full` retain a bounded
`malloc` path for array-valued Write/Call, so their linked server shows a small
nonzero `.data`/`.bss`. All server state otherwise lives in the two caller-owned
blocks below.

**Server object** (`sizeof(struct mu_server)`) — the control block `mu_server_init`
places into your storage. Scales with the compiled capacities.

| Profile | sizeof(struct mu_server) |
|---------|--------------------------|
| nano | 784 B |
| micro | 27,600 B |
| embedded | 97,216 B |
| standard | 1,068,200 B |
| full | 2,084,320 B |

**Caller-provided storage** (`MU_SERVER_STORAGE_BYTES`) — the single block you hand to
`mu_server_init`; holds the server object plus its scratch/chunk/security buffers.
Connections scale with sessions (each concurrent session may be an independent
client holding its own SecureChannel), so the connection pool dominates the
standard/full storage. Retune per build with `-DMU_MAX_CONNECTIONS=n`.

| Profile | Storage | Sessions | Connections | Subscriptions | Monitored Items | Publish |
|---------|---------|----------|-------------|---------------|-----------------|---------|
| nano | 1,408 B | 2 | 1 | — | — | — |
| micro | 30,720 B | 2 | 2 | 2 | 8 | 4 |
| embedded | 127,272 B | 2 | 4 | 2 | 100 | 5 |
| standard | 1,178,260 B | 50 | 50 | 50 | 1,000 | 50 |
| full | 2,300,460 B | 100 | 100 | 100 | 2,000 | 100 |

Full methodology in [docs/size/feature-size-ledger.md](docs/size/feature-size-ledger.md).

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

To build the other tiers: `make nano`, `make embedded`, `make standard`, or
`make all-profiles`. Run `make help` for a summary. The example binary always lands
at `build/<profile>/examples/minimal_server`.

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
- [docs/conformance/ecc-security-policy.md](docs/conformance/ecc-security-policy.md) — optional ECC SecurityPolicy CU (ECC-A + ECC-B)
- [docs/conformance/data-access.md](docs/conformance/data-access.md) — Data Access Server Facet (AnalogItem/discrete types, percent deadband)

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
- **The `standard`/`full` profiles** add multiplexed client connections, the Write
  service, username/X509 user tokens, dynamic nodes, arbitrary user methods,
  Alarms & Conditions events, server diagnostics, and (optionally) ECC SecurityPolicies.
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
- [docs/build-and-gating.md](docs/build-and-gating.md) — every `MUC_OPCUA_*` CMake flag, profile composition, and how to override a profile default (add/remove a feature)
- [docs/architecture.md](docs/architecture.md) — internals, memory model, poll loop
- [docs/api-reference.md](docs/api-reference.md) — full API reference
- [docs/conformance/](docs/conformance/) — service & profile conformance matrices
- [docs/conformance/ecc-security-policy.md](docs/conformance/ecc-security-policy.md) — optional ECC SecurityPolicy CU
- [docs/size/feature-size-ledger.md](docs/size/feature-size-ledger.md) — measured footprint

---

## License

Released under the [MIT License](LICENSE).
