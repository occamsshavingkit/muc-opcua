# Build Configuration & Feature Gating

muc-opcua is one C library that compiles into many different servers: a
few-hundred-byte Nano stub through a full-featured Standard 2017 server.
Every service, facet, and security layer is gated behind a CMake option that
either compiles a translation unit in or leaves it out entirely — there is no
runtime dispatch on a disabled feature; the code for it is not in the binary
(verified in CI by symbol-checking an ECC-off archive, see
[the size ledger](size/feature-size-ledger.md)).

This document is the single reference for that gating system: what
`MUC_OPCUA_PROFILE` does, the full flag list, how flags depend on each other,
and — the question that prompted this doc — **how to build a profile with one
feature removed** ("standard minus encryption").

## Quick answer: can I subtract a feature from a profile?

**Yes**, as of the profile-option override mechanism added alongside this
doc. Pass the profile and the flag you want to change on the same
`cmake` invocation:

```sh
cmake -S . -B build/standard-no-crypto \
    -DMUC_OPCUA_PROFILE=standard \
    -DMUC_OPCUA_SECURITY=OFF \
    -DMUC_OPCUA_ECC=OFF          # ECC requires SECURITY; drop both together
```

This builds every other `standard` default (Write, Subscriptions, Data
Access, Method Server, …) with the RSA/ECC crypto layer entirely compiled
out. Any `-D<FLAG>=<value>` on the same invocation as `-DMUC_OPCUA_PROFILE=X`
wins over that profile's default for that one flag — everything else still
comes from the profile. This works for **adding** a flag a profile doesn't
default to as well as **removing** one the profile does.

If you try to remove a flag something else still requires, configure fails
loudly instead of silently building an inconsistent binary:

```
$ cmake -S . -B build/bad -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF
CMake Error at CMakeLists.txt:370 (message):
  MUC_OPCUA_ECC requires MUC_OPCUA_SECURITY (pass -DMUC_OPCUA_ECC=OFF
  alongside your override, or also enable MUC_OPCUA_SECURITY)
```

See [Overriding a profile default](#overriding-a-profile-default-subtraction--addition)
for the full mechanics and more worked examples, and
[Dependencies between flags](#dependencies-between-flags) for the complete
list of what requires what.

## The profile system

`MUC_OPCUA_PROFILE` (a CMake cache string) is the single source of truth for
which named tier you're building. It resolves to one of:

| Profile | What it targets | Roughly |
|---|---|---|
| `nano` | Nano Embedded Device 2025 Server Profile | Read/Browse/discovery only, no heap, no security |
| `micro` | Micro Embedded Device 2025 Server Profile | + data-change Subscriptions, Write, 2 concurrent sessions |
| `embedded` | Embedded 2025 UA Server Profile | + Security (Basic256Sha256), base node set, events, multi-chunk |
| `standard` | Standard UA Server 2017 | + all optional services/facets (History, Query, NodeManagement, PubSub, Data Access, Method Server, Auditing, Complex Types, Redundancy, full Aggregates, Reverse Connect, ECC) |
| `full` | Not a distinct OPC UA profile | Same feature set as `standard`, larger capacity presets |
| `custom` | You hand-pick every flag | Nothing is forced; every `option()` keeps its own bare default (mostly OFF) unless you `-D` it |

Each named profile (everything except `custom`) is implemented as one block
in `CMakeLists.txt` that forces a specific set of feature options `ON`
(everything else in the controlled set is forced `OFF`). Profiles are
strictly additive tiers — `standard`/`full` is a superset of `embedded`,
which is a superset of `micro`, which is a superset of `nano` — **except**
where the override mechanism above is used to subtract from that.

**Capacities are separate from feature gating** and are not covered by this
doc: session/connection/subscription/array-length limits resolve through a
default → profile → user cascade in `include/muc_opcua/capacities.h`, keyed
off the `MUC_OPCUA_EMBEDDED_PROFILE`/`MUC_OPCUA_STANDARD_PROFILE` markers.
Override any capacity directly with `-DMU_MAX_*=<n>` regardless of profile.
See [docs/conformance/documentation.md](conformance/documentation.md) for the
per-profile capacity table.

### The `MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS` list

Every flag a named profile forces is enumerated in one CMake list
(`MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS` near the top of `CMakeLists.txt`).
Anything **not** in that list — `MUC_OPCUA_ALLOW_HEAP`,
`MUC_OPCUA_SESSION_TIMEOUT`, `MUC_OPCUA_READ_CACHE`, the
`MUC_OPCUA_HAVE_{MBEDTLS,WOLFSSL}` backend switches, `MUC_OPCUA_PLATFORM`,
the `MUC_OPCUA_BUILD_*` toggles — is either always independently settable, or
(for `MUC_OPCUA_ALLOW_HEAP` specifically) forced by nano/micro/embedded as a
memory-model consequence of the profile, not a facet you subtract. Everything
in the controlled list supports the override mechanism below.

## Overriding a profile default (subtraction / addition)

### The mechanism

Naively, "the profile forces every flag" and "a `-D` overrides the profile"
are in tension: if the profile block always wins, your `-D` is silently
discarded; if your `-D` always wins, a fresh `cmake -B build -DMUC_OPCUA_PROFILE=full`
with no other flags wouldn't reliably turn *on* full's defaults on a build
directory that happens to have stale cache entries from a previous run.

The resolution: **a per-option `-D` on the same invocation as the profile
choice wins for that option; anything you don't `-D` still comes from the
profile.** Concretely:

- Every controlled option's cache entry is snapshotted **before** anything in
  `CMakeLists.txt` touches it. On a fresh build directory, this exactly
  captures "did the user pass `-D<OPT>=...` this invocation".
- On a **reconfigure** of an existing build directory, mere presence in the
  cache is not enough — a value the *previous* profile forced is *also*
  always present, and indistinguishable from a fresh `-D` by presence alone.
  So the snapshot instead compares the option's current value against the
  value **this file itself last forced it to** (tracked internally per
  option): unchanged since our own last force ⇒ still profile-controlled,
  free to re-derive; changed since ⇒ something (a `-D`, or hand-editing the
  cache) overrode it deliberately, and that wins. This is the "check the
  *value*, not just whether the macro/cache-entry is defined" rule — checking
  bare definedness gives a false positive on every reconfigure, since
  `option()`/`set(...CACHE...)` make a variable permanently "defined" the
  moment they first run, regardless of who chose the value.
- Options are only re-derived at all when `MUC_OPCUA_PROFILE` itself changes
  (including a build directory's first-ever configure). Reconfiguring the
  *same* profile with one new `-D<OPT>=...` and nothing else is a no-op for
  every other option — nothing in the forcing machinery runs, so your new `-D`
  just sits in the cache.

The behavioral contract this guarantees, verified by
`scripts/test_profile_gating.sh`:

| Scenario | Result |
|---|---|
| `-DMUC_OPCUA_PROFILE=standard` alone | Every standard default, unchanged from before this mechanism existed |
| `-DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF -DMUC_OPCUA_ECC=OFF` | Standard minus crypto; everything else still ON |
| `-DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF` (ECC left implicit) | Configure fails: `MUC_OPCUA_ECC requires MUC_OPCUA_SECURITY` |
| Reconfigure the same build dir, same profile, `-DMUC_OPCUA_AUDITING=OFF` only | Only `AUDITING` changes; everything else untouched |
| Reconfigure an existing `full` build dir with `-DMUC_OPCUA_PROFILE=nano` | Fully re-derives nano's defaults — no leftover `full` flags survive |
| Switch back to `-DMUC_OPCUA_PROFILE=full` | Fully restores full's defaults |
| `-DMUC_OPCUA_PROFILE=custom` | Nothing forced; every flag is exactly what you `-D`'d (or its bare `option()` default) |

Run `scripts/test_profile_gating.sh` yourself to see all of the above
demonstrated against live `cmake` configures (it also builds one subtraction
config and checks with `nm` that the dropped feature's symbols are genuinely
absent from the archive, not just flagged off in the cache).

### More worked examples

Standard, but without the optional Redundancy and Reverse Connect facets
(smaller `.text`, same everything else):

```sh
cmake -S . -B build/standard-lean \
    -DMUC_OPCUA_PROFILE=standard \
    -DMUC_OPCUA_REDUNDANCY=OFF \
    -DMUC_OPCUA_REVERSE_CONNECT=OFF
```

Embedded, but with PubSub added on top (embedded doesn't default to it):

```sh
cmake -S . -B build/embedded-pubsub \
    -DMUC_OPCUA_PROFILE=embedded \
    -DMUC_OPCUA_PUBSUB=ON
```

Micro, but keep the crypto layer available even though micro doesn't
mandate it (adds SecurityPolicy support to a Micro-tier server):

```sh
cmake -S . -B build/micro-secure \
    -DMUC_OPCUA_PROFILE=micro \
    -DMUC_OPCUA_SECURITY=ON
```

### When to reach for `custom` instead

The override mechanism is for **small deltas off a named profile**. If you
want a feature set that looks nothing like any named profile — e.g. Write +
PubSub but nothing else, skipping the entire Standard facet bundle — hand-select
every flag with `-DMUC_OPCUA_PROFILE=custom`. Capacities then come from the
`standard` baseline unless you also `-DMU_MAX_*=<n>` them. The `standard`
profile block in `CMakeLists.txt` is the copy-paste starting list of every
flag name if you want to build up a custom set incrementally.

## Full flag reference

### Feature / facet flags (profile-controlled — support the override mechanism above)

| Flag | What it builds | nano | micro | embedded | standard/full | Depends on |
|---|---|:-:|:-:|:-:|:-:|---|
| `MUC_OPCUA_SUBSCRIPTIONS` | Data-change subscription engine (Subscription + MonitoredItem service sets) | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` | Standard DataChange Subscription 2017 facet additions | | | ✅ | ✅ | `SUBSCRIPTIONS` |
| `MUC_OPCUA_USER_AUTH` | Username/certificate user identity tokens | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_SERVICE_WRITE` | Write service (Value attribute) | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_SECURITY` | SecurityPolicy Basic256Sha256 / Aes128_Sha256_RsaOaep / Aes256_Sha256_RsaPss (asym+sym crypto, ~10 KB) | | | ✅ | ✅ | |
| `MUC_OPCUA_ECC` | ECC SecurityPolicies `#ECC_curve25519` + `#ECC_nistP256` (optional CU, spec 059) | | | | ✅ | `SECURITY` |
| `MUC_OPCUA_BASE_NODES` | Standard Base Information node set (Server object, ServerStatus, ServerCapabilities) | | | ✅ | ✅ | |
| `MUC_OPCUA_BASE_TYPE_SYSTEM` | Base Info Type System node subtree | | | ✅ | ✅ | `BASE_NODES` |
| `MUC_OPCUA_MULTIPLE_CONNECTIONS` | Multiple concurrent TCP connections / SecureChannels | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_EVENTS` | Event notifications | | | ✅ | ✅ | `SUBSCRIPTIONS` |
| `MUC_OPCUA_EMBEDDED_PROFILE` | Embedded 2017 profile gates + capacity minima marker | | | ✅ | ✅ | |
| `MUC_OPCUA_SERVICE_REGISTER_NODES` | RegisterNodes/UnregisterNodes | | | | ✅ | |
| `MUC_OPCUA_SERVICE_HISTORY` | Historical Access | | | | ✅ | |
| `MUC_OPCUA_SERVICE_QUERY` | Query services | | | | ✅ | |
| `MUC_OPCUA_SERVICE_NODEMANAGEMENT` | Optional NodeManagement service set | | | | ✅ | |
| `MUC_OPCUA_DYNAMIC_NODES` | Runtime-added address-space nodes | | | | ✅ | |
| `MUC_OPCUA_PUBSUB` | Publish/Subscribe capabilities | | | | ✅ | |
| `MUC_OPCUA_CUSTOM_METHODS` | Arbitrary custom Call method dispatch (paired with `METHOD_SERVER`) | | | | ✅ | |
| `MUC_OPCUA_SERVER_DIAGNOSTICS` | Server diagnostics node set | | | | ✅ | |
| `MUC_OPCUA_MULTI_CHUNK` | Multi-chunk (continuation) message support | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_EXTENDED_NODEIDS` | GUID / Opaque NodeId formats | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_DATA_ACCESS` | Data Access Server Facet (deadband, EURange, AnalogItem metadata) | | | | ✅ | `BASE_NODES`; **required by** `STANDARD_PROFILE` |
| `MUC_OPCUA_METHOD_SERVER` | Method Server Facet | | | | ✅ | **required by** `STANDARD_PROFILE` |
| `MUC_OPCUA_EVENT_FILTER_WHERE` | EventFilter where-clause evaluation engine | | | | ✅ | `EVENTS` |
| `MUC_OPCUA_AUDITING` | Auditing Server Facet (audit event types) | | | | ✅ | `EVENTS` |
| `MUC_OPCUA_COMPLEX_TYPES` | ComplexType Server Facet (custom structs/enums) | | | | ✅ | `BASE_NODES` |
| `MUC_OPCUA_REDUNDANCY` | Client Redundancy Facet (TransferSubscriptions) | | | | ✅ | `SUBSCRIPTIONS` |
| `MUC_OPCUA_AGGREGATE_FULL` | Full 42-aggregate set (OPC-10000-13) | | | | ✅ | `SUBSCRIPTIONS_STANDARD` |
| `MUC_OPCUA_REVERSE_CONNECT` | Reverse Connect Facet (server-initiated connections) | | | | ✅ | |
| `MUC_OPCUA_TIME_SYNC` | Security Time Synchronization (timestamp population) | | | | ✅ | |
| `MUC_OPCUA_NAMESPACES` | Namespaces metadata node (OPC-10000-5 §6.2.10) | | | | ✅ | `BASE_NODES` |
| `MUC_OPCUA_STANDARD_PROFILE` | Standard 2017 profile gates + capacity minima marker | | | | ✅ | requires `DATA_ACCESS` + `METHOD_SERVER` |

`embedded` and `standard`/`full` also each force `MUC_OPCUA_ALLOW_HEAP=OFF`
for nano/micro/embedded — **not** in the table above because it isn't a
profile-controlled option (see [above](#the-muc_opcua_profile_controlled_options-list)):
it can't be individually overridden via the same mechanism, only by choosing
`custom` or editing `CMakeLists.txt`.

### Base services (independent of `MUC_OPCUA_PROFILE`, mostly default ON)

These are not in the profile-controlled list at all — they're always
individually settable regardless of which profile you choose, because
OpenSecureChannel/Session/Read/Browse/Discovery are close to universal:

| Flag | What it builds | Default |
|---|---|:-:|
| `MUC_OPCUA_SERVICE_READ` | Read service | ON |
| `MUC_OPCUA_SERVICE_BROWSE` | Browse + BrowseNext + TranslateBrowsePaths | ON |
| `MUC_OPCUA_SERVICE_DISCOVERY` | GetEndpoints/FindServers | ON |

### Non-feature build settings

| Flag | What it does | Default |
|---|---|:-:|
| `MUC_OPCUA_ALLOW_HEAP` | Permits heap allocation in optional adapters/features (embedded/MCU tiers force OFF: no-heap is a project constitution rule) | ON |
| `MUC_OPCUA_SESSION_TIMEOUT` | Session timeout enforcement (auto-forced ON when `MULTI_CHUNK` or `MULTIPLE_CONNECTIONS` is on) | OFF |
| `MUC_OPCUA_READ_CACHE` | Read value cache (`maxAge` optimization) — a latency/size tradeoff, not a conformance requirement, so it's opt-in independent of profile | OFF |
| `MUC_OPCUA_HAVE_MBEDTLS` / `MUC_OPCUA_HAVE_WOLFSSL` | Compile in the mbedTLS / wolfSSL crypto backend (OpenSSL is auto-detected via `find_package`) | OFF |
| `MUC_OPCUA_PLATFORM` | `host`, `external`, `pico`, `arduino-skeleton` — selects the TCP/entropy/time adapters | `host` |
| `MUC_OPCUA_CLIENT_PROFILE` | `none`/`nano`/`standard` — client-side feature tier (`micro`/`embedded`/`full` are planned, not implemented; passing them is a hard `FATAL_ERROR`) | `none` |
| `MUC_OPCUA_BUILD_TESTS` / `_EXAMPLES` / `_FUZZERS` / `_BENCHMARKS` | Build the test/example/fuzzer/benchmark targets | OFF |
| `MUC_OPCUA_SANITIZERS` | Comma-separated sanitizer list (`address,undefined`) added to compile/link flags | empty |

## Dependencies between flags

This is the same problem Kconfig (Linux, Zephyr, ESP-IDF) calls out by name:
a preset that force-selects a feature without checking *that feature's own*
dependencies can silently produce an inconsistent config the moment presets
stop being all-or-nothing. Kconfig's own documented answer is to prefer
`depends on` (fail/hide rather than silently cascade) over blind `select`;
this project follows the same rule — **every dependency violation fails
loudly at `cmake` configure time**, it never gets silently dropped or
silently auto-disables the thing that requires it.

Two enforcement layers exist (both fail loud, at different points in the
pipeline — subtraction is what makes both load-bearing; before the override
mechanism, the FORCE-everything-together approach made invalid combinations
structurally unreachable):

1. **CMake configure-time (primary)**: `MUC_OPCUA_FEATURE_DEPENDENCIES` near
   the top of `CMakeLists.txt` is an explicit `OPTION:REQUIRES` table,
   checked with `message(FATAL_ERROR ...)` right after every profile-controlled
   option resolves — so a violation is caught before a single file compiles,
   with a message that tells you exactly which extra `-D` to add. This is
   the list in the [flag table](#feature--facet-flags-profile-controlled--support-the-override-mechanism-above)'s
   "Depends on" column.
2. **C preprocessor, compile-time (backstop)**: the identical set of
   requirements is *also* an `#error` in `include/muc_opcua/features.h`,
   included first from `muc_opcua/config.h` so every translation unit sees
   it. This exists for anyone who bypasses this project's own CMake profile
   logic entirely (a build system wrapping this one with raw `-D`s that
   skips `CMakeLists.txt`'s checks) — belt and suspenders, one layer
   deliberately redundant with the other. Keep `MUC_OPCUA_FEATURE_DEPENDENCIES`
   and `features.h` in sync; `features.h` is the normative source if they
   ever drift, since it's what the compiler actually enforces regardless of
   how the build was invoked.

None of the named profiles (`nano` through `full`) can produce an invalid
combination on their own — every dependency above is satisfied by
construction in each profile block. Invalid combinations only become
reachable once you start overriding individual flags (via the mechanism in
this doc) or via `custom`.

## Verifying gating behavior

- `scripts/test_profile_gating.sh` — runs the CMake-level scenarios in the
  table [above](#the-mechanism) against live `cmake` configures and asserts
  the resulting `CMakeCache.txt` (plus one `nm`-based check that a subtracted
  feature's code is truly absent from the archive). Run it directly; it's
  not wired into CTest because each scenario is a full `cmake` configure and
  that's too slow for the unit/integration suite.
- [`scripts/measure_size.sh`](../scripts/measure_size.sh) — cross-compiles
  each named profile for ARM Cortex-M0+ and reports `.text`/`.data`/`.bss`.
  Use it to size any subtraction/addition you make (e.g. confirm "standard
  minus encryption" actually saves the ~10 KB the `MUC_OPCUA_SECURITY` option
  doc string promises).
- [docs/size/feature-size-ledger.md](size/feature-size-ledger.md) — the
  historical record of what each feature costs, per profile.
