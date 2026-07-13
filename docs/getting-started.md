# Getting Started with muc-opcua

> **What you'll learn:**
> - Build the server profiles (`nano`, `micro`, `embedded`, `standard`, `full`) and find the resulting binary
> - Run the host `minimal_server` example and connect to it with a real OPC UA client
> - Browse and read the example's node set from Python (`asyncua`), and with generic clients (UAExpert, the .NET reference client)
> - Run the test suite and the interop scripts to prove your build works
>
> **Prerequisites:** Comfortable on a UNIX-like shell; basic C build experience. No prior OPC UA knowledge required.
>
> **Time:** 20-30 minutes | **Level:** Beginner / Intermediate

muc-opcua is a **freestanding C11, no-heap OPC UA server** meant to run on
microcontrollers (it has been sized for an RP2040). There is no `malloc` in the
protocol path: you hand the library a block of memory and two network buffers,
and it does the rest. This guide gets a working server running on your **host
machine** first, because that is the fastest way to see it work and to validate
your understanding before you embed it in firmware.

---

## 1. Prerequisites

You only need a couple of tools for the basic host build. The rest are optional,
depending on how far you want to go.

| Tool | Why you need it | Required for |
|---|---|---|
| **CMake ≥ 3.20** | Drives the build (the `make` targets are thin wrappers) | Everything |
| **A C11 toolchain** (`gcc` or `clang`) | Compiles the host build | Everything |
| **OpenSSL** (dev headers, e.g. `libssl-dev`) | Backs the host crypto adapter for the secure `embedded` profile | `embedded` profile |
| **`arm-none-eabi-gcc`** | Cross-compiles for ARM Cortex-M targets / firmware size checks | Cross builds (optional) |
| **Python 3 + `asyncua`** | A standards-compliant client to connect, browse and read | Interop / trying it out |
| **.NET 8 SDK** | Runs the OPC Foundation reference-client interop check | `.NET` interop (optional) |
| **UAExpert** (or any generic OPC UA client) | Point-and-click inspection of the address space | Optional, recommended |

> **Tip:** Verify your core tools before you start:
> ```bash
> cmake --version      # need 3.20 or newer
> gcc --version        # or: clang --version
> python3 -c "import asyncua; print(asyncua.__version__)"   # for interop
> ```
> Install `asyncua` with `pip install asyncua` if it is missing.

This tutorial was validated against CMake 3.28, GCC (host), and `asyncua` 2.0.

---

## 2. The profile system (pick what you build)

muc-opcua compiles to a different feature set depending on the **profile**.
Each named profile maps to one `make` target (or `-DMUC_OPCUA_PROFILE=...`).
CMake seeds Kconfig from `configs/<profile>.defconfig`, resolves dependencies,
and then compiles only the enabled source files. Any individual Kconfig symbol can
be added or removed on top of a profile's defaults — see
[docs/build-and-gating.md](build-and-gating.md) for the full symbol reference,
dependency cascade behavior, and override mechanics.

| Profile | Command | SecurityPolicy | Subscriptions | Core flash (Cortex-M0+ `-Os`) |
|---|---|---|---|---|
| **nano** | `make nano` | None | off | 22,531 B |
| **micro** | `make micro` | None | **on** (data-change) | 31,961 B |
| **embedded** | `make embedded` | **Basic256Sha256** (sign/encrypt) | Standard DataChange 2017 | 51,773 B |
| **standard** | `make standard` | + Aes128/256 RsaOaep/RsaPss | Standard profile capacity marker | 52,023 B |
| **full** | `make full` | Same as standard, plus optional ECC by default | Optional services/facets enabled | 80,831 B |

- **nano** — the OPC UA *Nano Embedded Device 2017* surface: Discovery, Session,
  Read, and the View (Browse) service set over **SecurityPolicy None**. Smallest build.
- **micro** — nano **plus** the data-change subscription engine (the Embedded Data
  Change Subscription Server Facet). Choose this if your client polls via subscriptions.
- **standard** — the OPC-10000-7 *Standard 2017 UA Server* target with standard
  profile capacity markers and the mandatory embedded-level surface.
- **full** — not a distinct OPC UA profile; it is the developer/integration build
  that enables optional services/facets such as Write, History, Query,
  NodeManagement, PubSub, Data Access, Method Server, Auditing, Complex Types,
  Redundancy, Reverse Connect, Aggregates, and optional ECC SecurityPolicies
  (`#ECC_curve25519`/`#ECC_nistP256`, spec 059 — see
  [docs/conformance/ecc-security-policy.md](conformance/ecc-security-policy.md)).
- **embedded** — micro **plus** SecurityPolicy **Basic256Sha256**, Standard DataChange
  2017, Base Info Type System exposure, and the required GetMonitoredItems/ResendData
  methods. The host build links OpenSSL to provide the crypto primitives; on an MCU you
  would swap in mbedTLS/PSA.

> **Note:** `nano`, `micro`, and `embedded` are strict no-heap builds. `standard`
> and `full` keep heap-enabled optional paths for larger array-valued services.
> The figures above are flash (code size); see [docs/size/feature-size-ledger.md](size/feature-size-ledger.md)
> for the full memory breakdown.

---

## 3. Build a profile

From the repository root, build the `nano` profile:

```bash
make nano
```

Expected (trimmed) output:

```
>> NANO profile: SecurityPolicy None, full Nano service surface, subscriptions OFF
cmake -S . -B build/nano -DMUC_OPCUA_BUILD_EXAMPLES=ON -DMUC_OPCUA_OPTIMIZE_SIZE=ON -DMUC_OPCUA_PROFILE=nano
...
[100%] Built target minimal_server
```

The `make` target is just a wrapper. The line it echoes is the exact CMake
command, so you can always drop down to raw CMake if you need to:

```bash
# Equivalent to `make nano`, written out longhand:
cmake -S . -B build/nano \
    -DMUC_OPCUA_BUILD_EXAMPLES=ON -DMUC_OPCUA_OPTIMIZE_SIZE=ON \
    -DMUC_OPCUA_PROFILE=nano
cmake --build build/nano
```

The other profiles work the same way:

```bash
make micro            # None + subscriptions
make embedded         # Basic256Sha256 + subscriptions (needs OpenSSL)
make standard         # standard server profile target
make full             # optional-service developer/integration build
make all-profiles     # build all five named profiles
```

To browse and edit the resolved Kconfig tree:

```bash
cmake -S . -B build/custom-standard -DMUC_OPCUA_PROFILE=standard
cmake --build build/custom-standard --target menuconfig
cmake -S . -B build/custom-standard \
    -DMUC_OPCUA_KCONFIG_CONFIG=build/custom-standard/.config
cmake --build build/custom-standard
```

`menuconfig` shows each feature's help text and dependency edges. If you turn off
a prerequisite, dependent symbols cascade off instead of producing an inconsistent
binary.

> **Warning:** A clang vs gcc warning policy and CMake warnings about missing
> `clang-format` / `cppcheck` / `clang-tidy` are harmless — those are optional
> static-analysis tools, not build dependencies.

### Where the binary lands

Each profile builds into its own directory, and the example binary is a file
named `minimal_server` directly under `examples/`:

```
build/nano/examples/minimal_server
build/micro/examples/minimal_server
build/embedded/examples/minimal_server
build/standard/examples/minimal_server
build/full/examples/minimal_server
```

Confirm it exists and is executable:

```bash
ls -l build/nano/examples/minimal_server
# -rwxrwxr-x ... build/nano/examples/minimal_server
```

## Checkpoint 1: You should be able to...

- [ ] Run `make nano` and see `[100%] Built target minimal_server`
- [ ] Find an executable at `build/nano/examples/minimal_server`

If the build fails, jump to [Troubleshooting](#troubleshooting).

---

## 4. Run the host server

Start the server you just built:

```bash
./build/nano/examples/minimal_server
```

Expected output:

```
Initializing muc-opcua Server...
Server initialized successfully. Listening on opc.tcp://localhost:4840
```

The server is now listening on **`opc.tcp://localhost:4840`** and will block in a
poll loop. Leave it running in this terminal; press **Ctrl-C** to stop it (it
prints `Shutting down...` and exits cleanly).

If you built the **`embedded`** profile, you will also see a line confirming the
secure endpoint is active:

```
SecurityPolicy Basic256Sha256 enabled (self-signed certificate)
```

> **Why a self-signed certificate?** The host crypto adapter generates an
> instance certificate at startup so the server can advertise Sign and
> SignAndEncrypt endpoints alongside None — no PKI setup required to experiment.

---

## 5. The example's node set

Before connecting a client, it helps to know what is *in* the server. The example
ships a tiny, self-consistent address space
(`examples/minimal_server/static_address_space.c`):

| NodeId | BrowseName | NodeClass | Value |
|---|---|---|---|
| `i=85` | Objects | Object | — (the standard Objects folder) |
| `ns=1;i=1000` | MyVar1 | Variable | **Int32 = 42** |
| `i=2255` | NamespaceArray | Variable | String[] (the namespace table) |
| `i=2254` | ServerArray | Variable | String[] |
| `i=11705` | MaxNodesPerRead | Variable | UInt32 = 32 |
| `i=11710` | MaxNodesPerBrowse | Variable | UInt32 = 8 |

The structure is: the **Objects** folder (`i=85`) `Organizes` a single read-only
Int32 variable **MyVar1** (`ns=1;i=1000`) whose value is **42**. The remaining
nodes are standard server metadata that real clients read during session setup.

This whole address space lives in `const` (read-only) memory — the server never
copies or mutates it. For *dynamic* values you would use a
`MU_VALUESOURCE_CALLBACK` instead of a static value; see the integration guide.

---

## 6. Connect with a real OPC UA client

With the server running (Section 4), open a **second terminal**.

### 6a. Python (`asyncua`)

This is the quickest way to prove the connect → browse → read path. Save the
following as `connect.py`:

```python
# connect.py - connect to muc-opcua, read MyVar1, list the Objects folder
import asyncio
from asyncua import Client

URL = "opc.tcp://localhost:4840"

async def main():
    client = Client(URL)
    # connect() drives the full handshake: HEL/ACK, OpenSecureChannel,
    # GetEndpoints, CreateSession and ActivateSession.
    await client.connect()
    try:
        # Read the example variable by NodeId.
        myvar = client.get_node("ns=1;i=1000")
        value = await myvar.read_value()
        print(f"MyVar1 (ns=1;i=1000) = {value}")     # -> 42

        # Browse the Objects folder.
        objects = client.get_node("i=85")
        children = await objects.get_children()
        print("Objects children:", [c.nodeid.to_string() for c in children])
    finally:
        await client.disconnect()

asyncio.run(main())
```

Run it:

```bash
python3 connect.py
```

Expected output:

```
MyVar1 (ns=1;i=1000) = 42
Objects children: ['ns=1;i=1000']
```

That's a full OPC UA session: the client negotiated a secure channel (None),
created and activated a session, read a node by its NodeId, and browsed the
address space — all against the no-heap server.

> **Tip:** To talk to the **`embedded`** profile over an encrypted channel, pass a
> security policy and mode to the `asyncua` client, e.g.
> `await client.set_security_string("Basic256Sha256,SignAndEncrypt,<client_cert>,<client_key>")`
> before `connect()`. For a first run, connecting with None (as above) works
> against every profile.

### 6b. Generic GUI clients (UAExpert, etc.)

Any standards-compliant client works. In **UAExpert**:

1. Add a server with the endpoint URL `opc.tcp://localhost:4840`.
2. Connect using **SecurityPolicy: None** (or Basic256Sha256 against the
   `embedded` profile).
3. Expand **Root → Objects** in the address-space tree; you'll see **MyVar1**.
4. Drag MyVar1 into the data-access view to watch its value (`42`).

### 6c. The OPC Foundation .NET reference client

The repo bundles an interop check that drives the official OPC Foundation .NET
reference client — useful for validating against the most authoritative stack.
See Section 7b.

## Checkpoint 2: You should be able to...

- [ ] Connect a client to `opc.tcp://localhost:4840`
- [ ] Read `ns=1;i=1000` and get `42`
- [ ] Browse `i=85` and find `ns=1;i=1000` underneath it

---

## 7. Validate your build

### 7a. The unit/integration test suite

`make test` configures a build with tests enabled and runs the full `ctest`
suite:

```bash
make test
```

Expected tail:

```
100% tests passed, 0 tests failed out of <N>
```

(`make test` doesn't pass `-DMUC_OPCUA_PROFILE`, so it resolves to `full` — the
most exhaustive test count. Building tests against a specific profile, e.g.
`-DMUC_OPCUA_PROFILE=nano -DMUC_OPCUA_BUILD_TESTS=ON`, runs fewer tests: only
the ones meaningful for what that profile actually compiles in.)

Under the hood this is:

```bash
cmake -S . -B build/test -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/test
cd build/test && ctest --output-on-failure
```

### 7b. Interop scripts (client round-trip)

Two scripts in `tests/interop/` start the server and drive a real client against
it. They take the **path to a server binary** as an argument (otherwise they
default to `build/host/examples/minimal_server`, so pass the profile binary you
built):

**Python `asyncua` smoke test** (needs `asyncua`):

```bash
./tests/interop/run_interop.sh build/nano/examples/minimal_server
```

Expected output:

```
Running interop smoke test against build/nano/examples/minimal_server ...
  read  ns=1;i=1000 (MyVar1) = 42   OK
  browse i=85 children = ['ns=1;i=1000']   OK
INTEROP PASS
```

**OPC Foundation .NET reference client** (needs the .NET 8 SDK):

```bash
./tests/interop/run_interop_dotnet.sh build/nano/examples/minimal_server
```

Both scripts launch the server, wait for it to accept on port 4840, run the
client, and clean up the server process when done.

## Checkpoint 3: You should be able to...

- [ ] See `100% tests passed ...` from `make test`
- [ ] See `INTEROP PASS` from `run_interop.sh`

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `make: cmake: Command not found` | CMake not installed / not on `PATH` | Install CMake ≥ 3.20 |
| Configure fails on the `embedded` profile, no crypto | OpenSSL dev headers missing | Install `libssl-dev` (or equivalent), re-run `make embedded` |
| `Server binary not found: ...` from an interop script | Default path is `build/host/...`; you built a profile dir | Pass the real path, e.g. `./tests/interop/run_interop.sh build/nano/examples/minimal_server` |
| `INTEROP FAIL: ... ConnectionRefused` | Server not running / wrong port | Make sure the server prints `Listening on opc.tcp://localhost:4840`; nothing else can hold port 4840 |
| `ModuleNotFoundError: No module named 'asyncua'` | Python client not installed | `pip install asyncua` |
| `Failed to initialize server: Bad_OutOfMemory` | Storage block too small for the compiled feature set | Size storage from `MU_SERVER_STORAGE_BYTES` (the example already does this) — never hardcode a literal |
| `run_interop_dotnet.sh` errors on `dotnet` | .NET 8 SDK not installed | Install the .NET 8 SDK, or use the Python interop instead |
| Address already in use on port 4840 | A previous server is still running | Find and stop it (`Ctrl-C` the old terminal, or `pkill -f minimal_server`) |

---

## How the no-heap memory model fits together

You don't need this to run the example, but it's the key idea before you embed
the library. Look at `examples/minimal_server/main.c`:

```c
/* The library tells you how big the storage block must be for the
   compiled feature set. Never hardcode a literal — it silently
   under-sizes when you enable security or subscriptions. */
static opcua_byte_t g_server_storage[MU_SERVER_STORAGE_BYTES];

static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];   /* 8192 bytes */
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];   /* 8192 bytes */

mu_server_init(g_server_storage, sizeof(g_server_storage), &config, &server);
```

The caller owns **all** memory: one server-storage block (sized by
`MU_SERVER_STORAGE_BYTES`, which grows automatically when subscriptions or
security are compiled in) plus two transport buffers. Configuration also wires
in **platform adapters** — TCP, time, entropy, and optionally crypto — through
function-pointer structs (`include/muc_opcua/platform.h`). On the host these
are provided for you; on an MCU you implement them against your board's network
stack and RNG. The main loop is a single non-blocking call:

```c
while (running) {
    mu_server_poll(server);   /* accept, read, dispatch, write — one iteration */
}
```

---

## Embed with FreeRTOS + lwIP

For a Pico W or similar MCU running FreeRTOS with lwIP sockets, treat the platform
as an application-owned envelope around the portable library. The core still
uses caller-owned storage and does not require heap allocation; FreeRTOS task
stacks, the FreeRTOS heap, lwIP heap, socket buffers, and Wi-Fi driver memory are
platform resources that you budget separately.

Use the external platform mode when your firmware provides the TCP/IP stack:

```cmake
set(MUC_OPCUA_PROFILE nano CACHE STRING "" FORCE)
set(MUC_OPCUA_PLATFORM external CACHE STRING "" FORCE)
set(MUC_OPCUA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MUC_OPCUA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MUC_OPCUA_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(MUC_OPCUA_OPTIMIZE_SIZE ON CACHE BOOL "" FORCE)

add_subdirectory(path/to/muc-opcua ${CMAKE_BINARY_DIR}/muc-opcua)

target_link_libraries(app PRIVATE
    muc_opcua
    pico_stdlib
    pico_cyw43_arch_lwip_sys_freertos
    FreeRTOS-Kernel-Heap4)
```

The in-tree `platform/pico` target is useful as a lifecycle and adapter-wiring
skeleton, but it is not a complete Pico W TCP/IP integration. On a real
FreeRTOS/lwIP build, provide the three required adapters yourself:

| Adapter | FreeRTOS/lwIP backing | Required behavior |
|---|---|---|
| `mu_tcp_adapter_t` | lwIP sockets with `NO_SYS=0`, `LWIP_SOCKET=1`, `LWIP_NETCONN=1` | Nonblocking `listen`, `accept`, `read`, `write`, `close_connection`, `shutdown` |
| `mu_time_adapter_t` | FreeRTOS tick count plus RTC/NTP if available | UTC OPC UA `DateTime` and monotonic millisecond ticks |
| `mu_entropy_adapter_t` | MCU RNG or platform CSPRNG | Fill nonce/random buffers; replace deterministic stubs before deployment |

The TCP callbacks should follow the same nonblocking contract as the host
adapter. `accept` returns `MU_STATUS_GOOD` with `connection_handle = NULL` when
no client is pending. `read` and `write` return `MU_STATUS_GOOD` with zero bytes
for `EAGAIN` or `EWOULDBLOCK`. EOF or a real socket error should return a bad
TCP status so the server closes the connection.

```c
typedef struct {
    int listen_fd;
} mu_lwip_tcp_context_t;

static opcua_statuscode_t lwip_tcp_read(void *context, void *connection_handle,
                                        opcua_byte_t *buffer, size_t buffer_size,
                                        size_t *bytes_read) {
    (void)context;
    int fd = (int)(intptr_t)connection_handle;
    int rc = lwip_read(fd, buffer, buffer_size);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *bytes_read = 0;
            return MU_STATUS_GOOD;
        }
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    if (rc == 0) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    *bytes_read = (size_t)rc;
    return MU_STATUS_GOOD;
}
```

For time, OPC UA wire timestamps use 100 ns ticks since 1601-01-01 UTC. If the
target has no trusted wall clock during bring-up, keep `get_tick_ms` monotonic
for session and subscription timeouts and document that wall-clock timestamps are
not production quality until RTC/NTP is wired.

```c
#define MU_OPCUA_UNIX_EPOCH_DATETIME 116444736000000000ULL

static opcua_datetime_t freertos_get_time(void *context) {
    (void)context;
    return (opcua_datetime_t)(MU_OPCUA_UNIX_EPOCH_DATETIME +
        ((opcua_uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS * 10000ULL));
}

static opcua_uint64_t freertos_get_tick_ms(void *context) {
    (void)context;
    return (opcua_uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}
```

The server task uses the same caller-owned storage model as the host example:

```c
static uintptr_t server_storage[(MU_SERVER_STORAGE_BYTES + sizeof(uintptr_t) - 1u) /
                                sizeof(uintptr_t)];
static opcua_byte_t receive_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t send_buffer[MU_MIN_CHUNK_SIZE];
static mu_lwip_tcp_context_t tcp_context;

static void opcua_task(void *arg) {
    (void)arg;

    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://0.0.0.0:4840";
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);
    config.max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config.max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.address_space = &address_space;

    mu_lwip_tcp_adapter_init(&config.tcp_adapter, &tcp_context);
    mu_freertos_time_adapter_init(&config.time_adapter);
    mu_target_entropy_adapter_init(&config.entropy_adapter);

    mu_server_t *server = NULL;
    opcua_statuscode_t status =
        mu_server_init(server_storage, sizeof(server_storage), &config, &server);
    if (status != MU_STATUS_GOOD) {
        vTaskDelete(NULL);
    }

    for (;;) {
        (void)mu_server_poll(server);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

Budget the firmware as a whole, not just the library archive. A typical
FreeRTOS/lwIP envelope has at least these categories:

| Category | Owner | Example budget item |
|---|---|---|
| Server storage | Application, sized by library | `MU_SERVER_STORAGE_BYTES` |
| OPC UA RX/TX buffers | Application, sized by library minimums | Two `MU_MIN_CHUNK_SIZE` arrays |
| Server task stack | FreeRTOS application | Stack for `mu_server_poll()` plus platform callbacks |
| FreeRTOS heap | Platform/application | Task, queue, timer, and driver allocations |
| lwIP heap and pools | Platform/application | TCP/IP control blocks, pbufs, socket state |
| Wi-Fi/Ethernet driver memory | Platform/application | CYW43 or MAC/PHY driver buffers |

For a deeper adapter contract, including the exact callback rules and storage
layout, continue with [the integration guide](integration-guide.md#3-implementing-the-platform-adapters).

---

## Summary

- muc-opcua builds in named profiles — `make nano` (None), `make micro`
  (+ subscriptions), `make embedded` (+ Basic256Sha256), `make standard`, and
  `make full` — each into
  `build/<profile>/examples/minimal_server`.
- The host example listens on `opc.tcp://localhost:4840` and serves a tiny
  address space whose `MyVar1` (`ns=1;i=1000`) reads back as `42`.
- Any standards-compliant client connects: an `asyncua` Python snippet,
  UAExpert, or the OPC Foundation .NET reference client.
- `make test` runs the CTest suite; `tests/interop/run_interop.sh` proves the
  full client round-trip.
- Memory for the core server is caller-provided (storage sized by
  `MU_SERVER_STORAGE_BYTES` plus two `MU_MIN_CHUNK_SIZE` buffers). The
  `nano`/`micro`/`embedded` profiles are strict no-heap builds.

## Next steps

1. **Embed it in firmware** — implement the TCP/time/entropy (and optional
   crypto) adapters for your board, supply your own static address space, and
   wire `mu_server_poll()` into your main loop. For FreeRTOS + lwIP, start with
   [Embed with FreeRTOS + lwIP](#embed-with-freertos--lwip); for the full adapter
   contract see **[docs/integration-guide.md](integration-guide.md)**.
2. **Look up the API** — every public type and function
   (`mu_server_init`, `mu_server_poll`, the config and adapter structs) is
   documented in **[docs/api-reference.md](api-reference.md)** and in the headers
   under `include/muc_opcua/`.
3. **Tune the footprint** — review **[docs/size/feature-size-ledger.md](size/feature-size-ledger.md)**
   to see exactly what each profile and `MU_MAX_*` knob costs in flash and RAM,
   then trim capacities to fit your target.
