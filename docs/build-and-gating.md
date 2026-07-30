# Build Configuration & Feature Gating

muc-opcua is one C library that compiles into many different servers: a
few-hundred-byte Nano stub through a full-featured optional-service build.
Every service, facet, and security layer is gated behind a Kconfig feature that
either compiles a translation unit in or leaves it out entirely — there is no
runtime dispatch on a disabled feature; the code for it is not in the binary
(verified in CI by symbol-checking an ECC-off archive, see
[the size ledger](size/feature-size-ledger.md)).

This document is the single reference for that gating system: what
`MUC_OPCUA_PROFILE` does, the full flag list, how flags depend on each other,
and — the question that prompted this doc — **how to build a profile with one
feature removed** ("standard minus encryption").

You do **not** have to use interactive `menuconfig`. It is a convenience UI over
the same Kconfig model used by the non-interactive build. Profiles, checked-in
defconfigs, CI jobs, and downstream non-CMake builds can all resolve Kconfig
programmatically and produce a valid `muc_opcua_autoconf.h`; see
[Programmatic configuration and autoconf.h](#programmatic-configuration-and-autoconfh).

> **Now Kconfig-based (2026-07).** The feature flags and their dependencies are
> declared in **`/Kconfig`** (resolved by the vendored `kconfiglib` in
> `scripts/kconfig/`), replacing the hand-rolled CMake `option()` / profile-block /
> `FEATURE_DEPENDENCIES` machinery. The **user-facing contract is unchanged**:
> `-DMUC_OPCUA_PROFILE=<tier>` selects a profile (a `configs/<tier>.defconfig`), and
> `-DMUC_OPCUA_<FLAG>=ON/OFF` still subtracts/adds a single flag. Two things changed:
> (1) an override that violates a dependency now **cascades** — the dependents are
> disabled too — instead of failing with an error (Kconfig's model, see
> [Dependencies](#dependencies-between-flags)); (2) you can browse/edit the whole tree
> interactively:
>
> ```sh
> cmake -B build -DMUC_OPCUA_PROFILE=standard   # seeds build/.config
> cmake --build build --target menuconfig        # edit build/.config (help + deps live)
> cmake -B build -DMUC_OPCUA_KCONFIG_CONFIG=build/.config   # apply the edits
> cmake --build build --target savedefconfig     # export a minimal build/defconfig
> ```
>
> Sections below that describe the old `MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS` list and
> the `_LAST_FORCED` override mechanism are superseded by Kconfig's native `depends on`
> + `default y if PROFILE_X`; the *behavior* they describe (per-flag override wins over
> the profile default) still holds.

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

This builds every other `standard` default with the RSA crypto layer entirely
compiled out. Any `-D<FLAG>=<value>` on the same invocation as `-DMUC_OPCUA_PROFILE=X`
wins over that profile's default for that one flag — everything else still
comes from the profile. This works for **adding** a flag a profile doesn't
default to as well as **removing** one the profile does.

If you remove a flag something else still requires, Kconfig **cascades** — the
dependent features are turned off too, so the build stays consistent (no inconsistent
binary, no error). E.g. `-DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BASE_NODES=OFF` also
disables `BASE_TYPE_SYSTEM`, `DATA_ACCESS`, `NAMESPACES`, `COMPLEX_TYPES` (all
`depends on BASE_NODES`). Use `menuconfig` to see, live, what a candidate override
would take down with it. (The `features.h` `#error` guards remain as a compile-time
backstop for anyone building with raw `-D`s outside this CMake path.)

See [Overriding a profile default](#overriding-a-profile-default-subtraction--addition)
for the full mechanics and more worked examples, and
[Dependencies between flags](#dependencies-between-flags) for the complete
list of what requires what.

## The profile system

`MUC_OPCUA_PROFILE` (a CMake cache string) is the single source of truth for
which named tier you're building. It resolves to one of:

+ `nano`: Nano Embedded Device 2017 Server Profile. Read/Browse/discovery only,
  no heap, no security.
+ `micro`: Micro Embedded Device 2017 Server Profile. Adds data-change
  subscriptions and multiple sessions/connections.
+ `embedded`: Embedded 2017 UA Server Profile. Adds Security
  (Basic256Sha256), the base node set, and Standard DataChange additions.
+ `standard`: Standard UA Server 2017. Adds the Standard profile marker and
  standard capacity minima to the Embedded-level feature surface.
+ `full`: not a distinct OPC UA profile. Uses the Standard profile/capacity
  family plus optional services/facets: History, Query, NodeManagement, PubSub,
  Data Access, Method Server, Auditing, Complex Types, Redundancy, full
  Aggregates, Reverse Connect, and ECC.
+ `custom`: you hand-pick every flag. Nothing is preset beyond the always-on
  core services (Read/Browse/Discovery); every feature stays OFF unless you
  `-D` it.

Each named profile (everything except `custom`) is a
`configs/<profile>.defconfig` selecting the profile choice; the resolved feature
set falls out of the per-child `default y if PROFILE_*` presets in `/Kconfig`.
Profiles are additive through `standard`; `full` then enables the optional
services/facets used for integration and development coverage. Any profile can
be further adjusted by the override mechanism above.

**Capacities are separate from feature gating**: session/connection/subscription/
array-length limits resolve through a default -> profile -> user cascade in
`include/muc_opcua/capacities.h`, keyed off the Kconfig profile choice symbols
(still emitted as compile defs). Override any capacity directly with
`-DMU_MAX_*=<n>` regardless of profile. The generated
[Capacity symbols](#capacity-symbols) table in this document is the CU 3808
application-documentation surface for core capacities; it includes the Nano
values for `max_sessions`, `max_connections`, `max_secure_channels`,
`max_subscriptions`, monitored items, publish requests, sampled-item queues,
View/Query continuation points, and address-space sizing. See also
[docs/conformance/documentation.md](conformance/documentation.md) for the
per-profile capacity table.

### Custom type systems and the Exposes Type System facet

If an application defines custom ObjectTypes, VariableTypes, DataTypes,
ReferenceTypes, or structured values whose DataTypes must be discoverable by
clients, the build must enable the OPC UA **Exposes Type System Server** facet
(`MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER`). In current named profiles that
facet is enabled from `embedded` through `full`; `nano` and `micro` stay smaller
unless the application opts into the facet explicitly. Without the facet, a
server may still expose simple instance nodes, but it must not claim support for
custom type-system exposure because clients cannot reliably browse the type
definitions and supertypes behind those custom instances.

### Where the flags live (Kconfig)

Every gated feature is now one `config` symbol in **`/Kconfig`** — its
`bool` prompt, its `depends on` edges, its per-profile `default y if PROFILE_*`
presets, and (increasingly) its OPC Part/§/facet-id help text. A profile is a
`configs/<profile>.defconfig` that just sets the profile choice
(`MUC_OPCUA_PROFILE_<X>=y`); everything else falls out of the per-child
`default y if …` presets, so the additive nano⊆micro⊆embedded⊆standard chain is
data in `/Kconfig` rather than five hand-maintained CMake blocks.

Flags **not** in the Kconfig feature tree — `MUC_OPCUA_ALLOW_HEAP` (forced OFF
for nano/micro/embedded in `CMakeLists.txt` as a memory-model consequence, not a
subtractable facet), `MUC_OPCUA_HAVE_{MBEDTLS,WOLFSSL}`, `MUC_OPCUA_PLATFORM`,
the `MUC_OPCUA_BUILD_*`/LTO/stack-usage knobs, and the CMake-driven
`MUC_OPCUA_CLIENT_PROFILE` axis — stay plain CMake options in
`cmake/MucOpcUaOptions.cmake`. Everything in the Kconfig tree supports the
`-D` override mechanism below.

### Programmatic configuration and autoconf.h

`menuconfig` is optional. The underlying resolver is
`scripts/kconfig/gen_config.py`, which takes a Kconfig file, a base defconfig,
and an optional override fragment. It emits two useful artifacts:

+ `muc_opcua_config.cmake`: used by this repository's CMake build. It emits
  `set(MUC_OPCUA_<SYM> ON/OFF)` values consumed by `src/CMakeLists.txt` for
  source selection and the legacy C alias compile definitions added by CMake.
+ `muc_opcua_autoconf.h`: a raw Kconfig export for non-CMake / external
  consumers. It defines enabled Kconfig symbols, such as
  `MUC_OPCUA_CU_SUBSCRIPTION_BASIC`, and raw capacity names, such as
  `MAX_SESSIONS`. It does **not** generate the legacy C aliases that
  `src/CMakeLists.txt` adds for CMake builds, such as `MUC_OPCUA_SUBSCRIPTIONS`
  or `MU_MAX_SESSIONS`.

Generate a standard profile configuration without any interactive step:

```sh
mkdir -p build/kconfig
python3 scripts/kconfig/gen_config.py \
    Kconfig \
    configs/standard.defconfig \
    build/kconfig/muc_opcua_config.cmake \
    build/kconfig/muc_opcua_autoconf.h
```

Add or subtract individual symbols with an override fragment. The fragment uses
Kconfig syntax and is merged on top of the base defconfig; `depends on` rules
still apply, so dependency-violating requests cascade or are vetoed by Kconfig
instead of creating an invalid header.

```sh
cat > build/kconfig/full-no-pubsub.fragment <<'EOF'
# MUC_OPCUA_CU_PUBSUB is not set
EOF

python3 scripts/kconfig/gen_config.py \
    Kconfig \
    configs/full.defconfig \
    build/kconfig/muc_opcua_config.cmake \
    build/kconfig/muc_opcua_autoconf.h \
    build/kconfig/full-no-pubsub.fragment
```

For a downstream build that does not call this repository's CMake, do not rely
on `muc_opcua_autoconf.h` alone unless the downstream build consumes the raw
Kconfig symbol names directly. Either map the raw Kconfig symbols and capacities
to the public C aliases used by the source tree, or provide an equivalent
project configuration header before any `muc_opcua` header or source includes
`muc_opcua/config.h`.

For example, a downstream alias header can include the generated export and add
the compatibility names needed by non-CMake builds:

```sh
cc -Iinclude -include build/kconfig/muc_opcua_external_config.h ...
```

The generated header is not included automatically by `muc_opcua/config.h` because
many embedded build systems have their own configuration include convention.
The contract is simple: enabled public feature macros must be defined to `1`;
disabled feature macros must be undefined; capacity aliases such as
`MU_MAX_SESSIONS` must be set when overriding defaults.
`include/muc_opcua/features.h` remains the compiler backstop for illegal
hand-written combinations.

For repeatable custom configurations, either commit a minimal defconfig under
`configs/` or pass a saved `.config` through CMake:

```sh
# Produce a minimal defconfig from a resolved .config.
python3 scripts/kconfig/savedefconfig.py \
    Kconfig build/kconfig/.config configs/my-board.defconfig

# Use a saved .config directly in CMake. Keep MUC_OPCUA_PROFILE aligned with
# the saved .config's profile choice so capacity/profile marker macros match.
cmake -S . -B build/my-board \
    -DMUC_OPCUA_PROFILE=embedded \
    -DMUC_OPCUA_KCONFIG_CONFIG=path/to/.config
```

Use `configs/<profile>.defconfig` for named profiles, a committed custom
defconfig for product builds, and `menuconfig` only when you want an interactive
editor for the same data. A saved `.config` overrides the Kconfig input, but
CMake still emits capacity profile markers from `MUC_OPCUA_PROFILE`; pass the
matching profile explicitly when replaying a saved named-profile config.

## Overriding a profile default (subtraction / addition)

### The mechanism

**The profile is a base `.config`; your `-D`s are a fragment merged on top.**
On every configure, `CMakeLists.txt`:

1. picks the base config — `configs/<profile>.defconfig` (or a
   `-DMUC_OPCUA_KCONFIG_CONFIG=<file>` you point it at);
2. collects any `-DMUC_OPCUA_<FLAG>=ON/OFF` you passed this invocation into a
   `kconfig_overrides.config` fragment (it only writes a flag into the fragment
   when the cache entry is actually present, so unspecified flags stay
   profile-derived);
3. runs `scripts/kconfig/gen_config.py Kconfig <base> muc_opcua_config.cmake
   [autoconf.h] <fragment>`, which does `load_config(base)` then
   `load_config(fragment, replace=False)`. kconfiglib resolves `depends on`,
   applies the profile's `default y if PROFILE_*` presets, and — crucially —
   **cascades**: an override that turns a prerequisite off drags its dependents
   off too, so the resolved set is always internally consistent.

Because the base is re-read from the defconfig every configure and the fragment
is rebuilt from the current `-D`s, switching profiles in an existing build dir
re-derives cleanly (no stale flags survive) and adding one `-D` on a reconfigure
changes exactly that flag and its dependents — no hand-rolled "last forced"
bookkeeping needed; Kconfig's declarative resolution gives it for free.

The behavioral contract is verified by `scripts/test_profile_gating.sh` against
live `cmake` configures:

| Scenario | Result |
|---|---|
| `-DMUC_OPCUA_PROFILE=standard` alone | Every standard default, byte-identical to the pre-Kconfig build |
| `-DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF` | Standard minus crypto; `ECC` **cascades off** (it `depends on SECURITY`); everything else still ON |
| `-DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BASE_NODES=OFF` | `BASE_TYPE_SYSTEM`, `DATA_ACCESS`, `NAMESPACES`, `COMPLEX_TYPES` all cascade off; no error |
| Reconfigure the same build dir, same profile, `-DMUC_OPCUA_AUDITING=OFF` only | Only `AUDITING` (and anything depending on it) changes |
| Reconfigure an existing `full` build dir with `-DMUC_OPCUA_PROFILE=nano` | Fully re-derives nano's defaults — no leftover `full` flags survive |
| Switch back to `-DMUC_OPCUA_PROFILE=full` | Fully restores full's defaults |
| `-DMUC_OPCUA_PROFILE=custom` | Nothing preset beyond the always-on core services; every flag is what you `-D`'d |

Run `scripts/test_profile_gating.sh` yourself to see all of the above
demonstrated against live `cmake` configures (it also builds one subtraction
config and checks with `nm` that the dropped feature's symbols are genuinely
absent from the archive, not just flagged off — e.g. `mu_sym_chunk_wrap` gone
when `SECURITY=OFF`).

### More worked examples

Full, but without the optional Redundancy and Reverse Connect facets
(smaller `.text`, same everything else):

```sh
cmake -S . -B build/full-lean \
    -DMUC_OPCUA_PROFILE=full \
    -DMUC_OPCUA_REDUNDANCY=OFF \
    -DMUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER=OFF
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
minimal baseline unless you also `-DMU_MAX_*=<n>` them. The flag reference below
is the copy-paste starting list of symbol names if you want to build up a custom
set incrementally.

## Full flag reference

### Feature / facet flags

Membership below is the resolved `default y if PROFILE_*` matrix from `/Kconfig`
and supports the override mechanism described above. To regenerate a temporary
config, run:

```sh
python3 scripts/kconfig/gen_config.py \
  Kconfig configs/<p>.defconfig \
  /tmp/muc_opcua_config.cmake \
  /tmp/muc_opcua_autoconf.h
```

Since **spec 067** rebased each named profile onto exactly its OPC-namesake's
*mandatory* facet set, `standard` is much leaner than `full` — the two are no
longer the same column. In features `embedded` and `standard` are nearly
identical (the difference is capacity markers, driving `capacities.h`); the many
optional facets live only in `full`.

| Flag | What it builds | nano | micro | embedded | standard | full | Depends on |
|---|---|:-:|:-:|:-:|:-:|:-:|---|
| `MUC_OPCUA_BASE_NODES` | Standard Base Information node set (Server object, ServerStatus, ServerCapabilities) | ✅ | ✅ | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_USER_AUTH` | Username/certificate user identity tokens | ✅ | ✅ | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_SERVICE_REGISTER_NODES` | RegisterNodes/UnregisterNodes | ✅ | ✅ | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_SUBSCRIPTIONS` | Data-change subscription engine (Subscription + MonitoredItem service sets) | | ✅ | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_MULTIPLE_CONNECTIONS` | Multiple concurrent TCP connections / SecureChannels | | ✅ | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_SECURITY` | SecurityPolicy Basic256Sha256 / Aes128_Sha256_RsaOaep / Aes256_Sha256_RsaPss (asym+sym crypto, ~10 KB) | | | ✅ | ✅ | ✅ | |
| `MUC_OPCUA_BASE_TYPE_SYSTEM` | Base Info Type System node subtree | | | ✅ | ✅ | ✅ | `BASE_NODES` |
| `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` | Standard DataChange Subscription 2017 facet additions | | | ✅ | ✅ | ✅ | `SUBSCRIPTIONS` |
| `MUC_OPCUA_STANDARD_PROFILE` | Standard 2017 capacity-minima marker (drives `capacities.h`) | | | | ✅ | ✅ | |
| `MUC_OPCUA_SERVICE_WRITE` | Write service (Value attribute) | | | | | ✅ | |
| `MUC_OPCUA_ECC` | ECC SecurityPolicies `#ECC_curve25519` + `#ECC_nistP256` (optional CU, spec 059) | | | | | ✅ | `SECURITY` |
| `MUC_OPCUA_EVENTS` | Event notifications | | | | | ✅ | `SUBSCRIPTIONS` |
| `MUC_OPCUA_MULTI_CHUNK` | Multi-chunk (continuation) message support | | | | | ✅ | |
| `MUC_OPCUA_EXTENDED_NODEIDS` | GUID / Opaque NodeId formats | | | | | ✅ | |
| `MUC_OPCUA_SERVICE_HISTORY` | Historical Access | | | | | ✅ | |
| `MUC_OPCUA_SERVICE_QUERY` | Query services | | | | | ✅ | |
| `MUC_OPCUA_SERVICE_NODEMANAGEMENT` | Optional NodeManagement service set | | | | | ✅ | |
| `MUC_OPCUA_DYNAMIC_NODES` | Runtime-added address-space nodes | | | | | ✅ | |
| `MUC_OPCUA_PUBSUB` | Publish/Subscribe capabilities | | | | | ✅ | |
| `MUC_OPCUA_CUSTOM_METHODS` | Arbitrary custom Call method dispatch (paired with `METHOD_SERVER`) | | | | | ✅ | |
| `MUC_OPCUA_SERVER_DIAGNOSTICS` | Server diagnostics node set | | | | | ✅ | |
| `MUC_OPCUA_DATA_ACCESS` | Data Access Server Facet (deadband, EURange, AnalogItem metadata) | | | | | ✅ | `BASE_NODES` |
| `MUC_OPCUA_METHOD_SERVER` | Method Server Facet | | | | | ✅ | |
| `MUC_OPCUA_EVENT_FILTER_WHERE` | EventFilter where-clause evaluation engine | | | | | ✅ | `EVENTS && SUBSCRIPTIONS_STANDARD` |
| `MUC_OPCUA_AUDITING` | Auditing Server Facet (audit event types) | | | | | ✅ | `EVENTS` |
| `MUC_OPCUA_COMPLEX_TYPES` | ComplexType Server Facet (custom structs/enums) | | | | | ✅ | `BASE_NODES` |
| `MUC_OPCUA_REDUNDANCY` | Client Redundancy Facet (TransferSubscriptions) | | | | | ✅ | `SUBSCRIPTIONS` |
| `MUC_OPCUA_AGGREGATE_FULL` | Full 42-aggregate set (OPC-10000-13) | | | | | ✅ | `SUBSCRIPTIONS_STANDARD` |
| `MUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER` | Protocol Reverse Connect Server CU 2867 (server-initiated connections) | | | | | ✅ | |
| `MUC_OPCUA_TIME_SYNC` | Security Time Synchronization (timestamp population) | | | | | ✅ | |
| `MUC_OPCUA_NAMESPACES` | Namespaces metadata node (OPC-10000-5 §6.2.10) | | | | | ✅ | `BASE_NODES` |

`MUC_OPCUA_ALLOW_HEAP` is forced `OFF` for `nano`/`micro`/`embedded` in
`CMakeLists.txt` as a memory-model consequence of those tiers — **not** in the
table above because it isn't in the Kconfig feature tree
(see [above](#where-the-flags-live-kconfig)): override it only by choosing
`custom` or editing `CMakeLists.txt`.

### Base services (independent of `MUC_OPCUA_PROFILE`, mostly default ON)

These are Kconfig symbols with an **unconditional `default y`** (not gated on a
profile), because OpenSecureChannel/Session/Read/Browse/Discovery are close to
universal — so every profile, including `custom`, gets them ON, and you subtract
one with `-DMUC_OPCUA_SERVICE_<X>=OFF` like any other flag:

| Flag | What it builds | Default |
|---|---|:-:|
| `MUC_OPCUA_SERVICE_READ` | Read service | ON |
| `MUC_OPCUA_SERVICE_BROWSE` | Browse + BrowseNext + TranslateBrowsePaths | ON |
| `MUC_OPCUA_SERVICE_DISCOVERY` | GetEndpoints/FindServers | ON |

### Additional Kconfig toggles

| Flag | What it does | Default |
|---|---|:-:|
| `MUC_OPCUA_SESSION_TIMEOUT` | Session timeout enforcement (auto-forced ON when `MULTI_CHUNK` or `MULTIPLE_CONNECTIONS` is on) | OFF |
| `MUC_OPCUA_READ_CACHE` | Read value cache (`maxAge` optimization) — a latency/size tradeoff, not a conformance requirement, so it's opt-in independent of profile | OFF |

### Non-feature build settings

| Flag | What it does | Default |
|---|---|:-:|
| `MUC_OPCUA_ALLOW_HEAP` | Permits heap allocation in optional adapters/features (embedded/MCU tiers force OFF: no-heap is a project constitution rule) | ON |
| `MUC_OPCUA_HAVE_MBEDTLS` / `MUC_OPCUA_HAVE_WOLFSSL` | Compile in the mbedTLS / wolfSSL crypto backend (OpenSSL is auto-detected via `find_package`) | OFF |
| `MUC_OPCUA_PLATFORM` | `host`, `external`, `pico`, `arduino-skeleton` — selects the TCP/entropy/time adapters | `host` |
| `MUC_OPCUA_CLIENT_PROFILE` | `none`/`nano`/`standard` — client-side feature tier (`micro`/`embedded`/`full` are planned, not implemented; passing them is a hard `FATAL_ERROR`) | `none` |
| `MUC_OPCUA_BUILD_TESTS` / `_EXAMPLES` / `_FUZZERS` / `_BENCHMARKS` | Build the test/example/fuzzer/benchmark targets | OFF |
| `MUC_OPCUA_SANITIZERS` | Comma-separated sanitizer list (`address,undefined`) added to compile/link flags | empty |

## Dependencies between flags

Dependency edges now live in **one place**: `depends on` clauses in `/Kconfig`
(the migration unified the two lists that used to drift — the CMake
`FEATURE_DEPENDENCIES` table and the `features.h` `#error`s). Kconfig is the
mechanism the Linux/Zephyr/ESP-IDF ecosystems use for exactly this: a symbol
whose `depends on` is unmet cannot be `y`. So when you subtract a prerequisite,
its dependents **cascade off** rather than erroring — the resolved config is
always internally consistent by construction.

Two layers still cooperate, at different points in the pipeline:

1. **Kconfig resolution (primary)**: every edge is a `depends on` in `/Kconfig`
   (e.g. `BASE_TYPE_SYSTEM depends on BASE_NODES`, `ECC depends on SECURITY`,
   `EVENT_FILTER_WHERE depends on EVENTS && SUBSCRIPTIONS_STANDARD`). `gen_config.py`
   resolves them at configure time; a disabled prerequisite drags its dependents
   off. This is the "Depends on" column in the
   [flag table](#feature--facet-flags-in-the-kconfig-tree--support-the-override-mechanism-above).
   Use `menuconfig` to see, before you commit to an override, exactly which
   dependents a candidate change would disable.
2. **C preprocessor, compile-time (backstop)**: the same requirements remain as
   `#error`s in `include/muc_opcua/features.h`, included first from
   `muc_opcua/config.h`. This is *only* a backstop for a build that bypasses
   this project's CMake path entirely (a wrapper feeding raw `-D`s straight to
   the compiler, skipping `gen_config.py`) — the cascade means the normal CMake
   path never trips it. `features.h` stays the normative statement of a
   dependency if the two ever drift, since it's what the compiler enforces
   regardless of how the build was invoked.

None of the named profiles (`nano` through `full`) can produce an invalid
combination on their own — each defconfig's presets satisfy every `depends on`.
Cascades only come into play once you override individual flags or use `custom`.

<!-- BEGIN GENERATED MANIFEST TABLES -->
## Manifest-generated reference tables

The tables below are generated from
`profiles/opcua-profile-manifest.yaml` by
`scripts/profile_manifest/generate.py --outputs build_docs`.
Do not edit between the BEGIN/END markers; run the generator
to refresh.

### Feature symbols

| Kconfig | Item | State | nano | micro | embedded | standard | full | Depends on |
|---------|------|-------|------|-------|----------|----------|------|------------|
| READ_CACHE | read_cache | implemented |  |  |  |  |  |  |
| SECURE_CHANNEL_CRYPTO | secure_channel_crypto | implemented |  | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_FACET_CORE_2022_SERVER | opc_facet_1322 | implemented | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE | opc_cu_2446 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME | opc_cu_2447 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_LOCALTIME | opc_cu_2476 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST | opc_cu_2711 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT | opc_cu_2969 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_OPTIONSET | opc_cu_3127 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME | opc_cu_3198 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES | opc_cu_3560 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER, MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT | opc_cu_4053 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_CURRENCY | opc_cu_5240 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER | opc_facet_1219 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER | opc_facet_1324 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER | opc_facet_1631 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_REVERSE_CONNECT_SERVER | opc_facet_1632 | implemented |  |  |  |  | ✅ |  |
| MUC_OPCUA_FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER | opc_facet_1695 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER | opc_facet_1696 | implemented |  |  |  | ✅ | ✅ |  |
| MUC_OPCUA_FACET_EMBEDDED_DATACHANGE_SUBSCRIPTION_2022_SERVER | opc_facet_2250 | implemented |  | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_BASE_INFO_SERVERTYPE | opc_cu_3189 | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION | opc_cu_5801 | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_SUBSCRIPTION_BASIC | opc_cu_subscription_basic | claimed |  | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_SUBSCRIPTION_STANDARD | opc_cu_subscription_standard | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_BASIC |
| MUC_OPCUA_CU_SECURITY_ECC | opc_cu_security_ecc | claimed |  |  |  |  | ✅ | SECURE_CHANNEL_CRYPTO |
| MUC_OPCUA_CU_EVENTS | opc_cu_events | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_BASIC |
| MUC_OPCUA_CU_DATA_ACCESS | opc_cu_data_access | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_METHOD_SERVER | opc_cu_method_server | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_USER_AUTH | opc_cu_user_auth | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_MULTIPLE_CONNECTIONS | opc_cu_multiple_connections | claimed |  | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_EVENT_FILTER_WHERE | opc_cu_event_filter_where | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_EVENTS, MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| MUC_OPCUA_CU_REDUNDANCY | opc_cu_redundancy | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_BASIC |
| MUC_OPCUA_CU_COMPLEX_TYPES | opc_cu_complex_types | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AUDITING | opc_cu_auditing | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_EVENTS |
| MUC_OPCUA_CU_MULTI_CHUNK | opc_cu_multi_chunk | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_SESSION_TIMEOUT | opc_cu_session_timeout | claimed |  | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_CU_MULTIPLE_CONNECTIONS, MUC_OPCUA_CU_MULTI_CHUNK |
| MUC_OPCUA_CU_TIME_SYNC | opc_cu_time_sync | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_EXTENDED_NODEIDS | opc_cu_extended_nodeids | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_FULL | opc_cu_aggregate_full | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| MUC_OPCUA_CU_PUBSUB | opc_cu_pubsub | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_NAMESPACES | opc_cu_namespaces | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_BASE_INFO_DATATYPES | opc_cu_base_info_datatypes | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE | opc_cu_base_info_argument_type | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_BASE_TYPES | opc_cu_base_info_base_types | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER, MUC_OPCUA_CU_BASE_INFO_DATATYPES |
| MUC_OPCUA_CU_ATTRIBUTE_READ | service_read | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH | service_browse | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS | service_discovery | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_VIEW_REGISTERNODES | service_register_nodes | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE | service_write | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET | service_history | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_1572 | deferred |  |  |  |  |  |  |
| — | opc_cu_1577 | deferred |  |  |  |  |  |  |
| — | opc_cu_1578 | deferred |  |  |  |  |  | MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET |
| — | opc_cu_1579 | deferred |  |  |  |  |  | MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET |
| — | opc_cu_1580 | deferred |  |  |  |  |  | MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET |
| — | opc_cu_1581 | deferred |  |  |  |  |  | MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET |
| — | opc_cu_1710 | deferred |  |  |  |  |  |  |
| MUC_OPCUA_CU_QUERY | service_query | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_NODEMANAGEMENT | service_nodemanagement | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_FACET_UA_TCP_UA_SC_UA_BINARY | opc_facet_837 | implemented | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION | opc_facet_1760 | implemented | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH | opc_cu_2317 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS | opc_cu_2328 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_SESSION_CHANGE_USER | opc_cu_2400 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP | opc_cu_2936 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE | opc_cu_3147 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS | opc_cu_3192 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_VIEW_BASIC_2 | opc_cu_3530 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS | opc_cu_3983 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_SESSION_GENERAL_SERVICE | opc_cu_session_general_service | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_DISCOVERY_REGISTER | opc_cu_2271 | claimed |  |  |  | ✅ | ✅ |  |
| MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS | opc_cu_5592 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE | opc_cu_key_credential_service | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION, MUC_OPCUA_CU_METHOD_SERVER |
| MUC_OPCUA_CU_USER_ROLE_MANAGEMENT | opc_cu_user_role_management | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION, MUC_OPCUA_CU_METHOD_SERVER |
| MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT | opc_cu_certificate_management | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION, MUC_OPCUA_CU_METHOD_SERVER |
| MUC_OPCUA_CU_ALARMS_CONDITIONS | opc_cu_alarms_conditions | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_EVENTS, MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_AVERAGE | opc_cu_2375 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_COUNT | opc_cu_2958 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_DELTA | opc_cu_2256 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_DELTABOUNDS | opc_cu_2194 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_DURATIONBAD | opc_cu_2954 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_DURATIONGOOD | opc_cu_3105 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_DURATIONINSTATEZERO | opc_cu_2998 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_END | opc_cu_2743 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_INTERPOLATIVE | opc_cu_2754 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MAXIMUM | opc_cu_2381 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MAXIMUM2 | opc_cu_2166 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MINIMUM | opc_cu_2376 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MINIMUM2 | opc_cu_2302 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_PERCENTBAD | opc_cu_3010 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_PERCENTGOOD | opc_cu_3048 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_RANGE | opc_cu_2377 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_START | opc_cu_3108 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_TIMEAVERAGE | opc_cu_3075 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_TIMEAVERAGE2 | opc_cu_3126 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_TOTAL | opc_cu_3062 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_TOTAL2 | opc_cu_2184 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_WORSTQUALITY | opc_cu_2201 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_WORSTQUALITY2 | opc_cu_2408 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MINIMUMACTUALTIME | opc_cu_2974 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MAXIMUMACTUALTIME | opc_cu_3130 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MINIMUMACTUALTIME2 | opc_cu_2952 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_MAXIMUMACTUALTIME2 | opc_cu_2941 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_RANGE2 | opc_cu_3047 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_DURATIONINSTATENONZERO | opc_cu_3144 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_NUMBEROFTRANSITIONS | opc_cu_3099 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_STARTBOUND | opc_cu_2330 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_ENDBOUND | opc_cu_2207 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_STANDARDDEVIATIONSAMPLE | opc_cu_2358 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_VARIANCESAMPLE | opc_cu_2281 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_STANDARDDEVIATIONPOPULATION | opc_cu_2955 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_SUBSCRIPTION_VARIANCEPOPULATION | opc_cu_2178 | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AGGREGATE_STANDARDDEVIATIONPOPULATION | opc_cu_3162 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_INTERPOLATIVE | opc_cu_3159 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MAXIMUMACTUALTIME2 | opc_cu_3101 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_DURATIONGOOD | opc_cu_3085 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_END | opc_cu_3061 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_WORSTQUALITY | opc_cu_3055 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_TOTAL | opc_cu_3032 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MAXIMUMACTUALTIME | opc_cu_3018 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_RANGE | opc_cu_3011 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_STANDARDDEVIATIONSAMPLE | opc_cu_3006 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_AVERAGE | opc_cu_2996 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_NUMBEROFTRANSITIONS | opc_cu_2985 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_PERCENTBAD | opc_cu_2975 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MAXIMUM | opc_cu_2962 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_VARIANCESAMPLE | opc_cu_2960 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_VARIANCEPOPULATION | opc_cu_2948 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER | opc_cu_2867 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_REVERSE_CONNECT_SERVER |
| MUC_OPCUA_CU_AGGREGATE_MINIMUMACTUALTIME | opc_cu_2759 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_RANGE2 | opc_cu_2730 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_WORSTQUALITY2 | opc_cu_2384 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MINIMUM2 | opc_cu_2382 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_DELTABOUNDS | opc_cu_2350 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MINIMUM | opc_cu_2346 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_START | opc_cu_2339 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_DELTA | opc_cu_2335 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_DURATIONBAD | opc_cu_2314 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_TIMEAVERAGE | opc_cu_2305 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_PERCENTGOOD | opc_cu_2303 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_ENDBOUND | opc_cu_2282 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_TIMEAVERAGE2 | opc_cu_2273 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_STARTBOUND | opc_cu_2267 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_COUNT | opc_cu_2263 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_DURATIONINSTATENONZERO | opc_cu_2223 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_DURATIONINSTATEZERO | opc_cu_2220 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_TOTAL2 | opc_cu_2210 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MAXIMUM2 | opc_cu_2188 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_CU_AGGREGATE_MINIMUMACTUALTIME2 | opc_cu_2175 | claimed |  |  |  |  |  |  |
| MUC_OPCUA_MDNS_DISCOVERY | mdns_discovery | implemented |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_USER_TOKEN_JWT | cu_user_token_jwt | implemented |  |  |  |  | ✅ | MUC_OPCUA_CU_USER_AUTH |
| MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL | cu_certificate_manager_pull | implemented |  |  |  |  | ✅ | MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT, MUC_OPCUA_CU_METHOD_SERVER, MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION |
| MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER | cu_authorization_service_server | implemented |  |  |  |  |  | MUC_OPCUA_CU_USER_TOKEN_JWT, MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION |

### Capacity symbols

| Kconfig | Capacity | nano | micro | embedded | standard | full | Override |
|---------|----------|------|-------|----------|----------|------|----------|
| MAX_SESSIONS | max_sessions | 2 | 2 | 2 | 50 | 100 | MU_MAX_SESSIONS |
| MAX_CONNECTIONS | max_connections | 1 | 2 | 4 | 50 | 100 | MU_MAX_CONNECTIONS |
| MAX_SUBSCRIPTIONS | max_subscriptions | 2 | 2 | 2 | 50 | 100 | MU_MAX_SUBSCRIPTIONS |
| MAX_MONITORED_ITEMS | max_monitored_items | 8 | 8 | 100 | 1000 | 2000 | MU_MAX_MONITORED_ITEMS |
| MAX_PUBLISH_REQUESTS | max_publish_requests | 4 | 4 | 5 | 50 | 100 | MU_MAX_PUBLISH_REQUESTS |
| MONITORED_QUEUE_DEPTH | monitored_queue_depth | 1 | 1 | 2 | 5 | 5 | MU_MONITORED_QUEUE_DEPTH |
| MAX_ARRAY_LENGTH | max_array_length | 512 | 512 | 2048 | 8192 | 8192 | MU_MAX_ARRAY_LENGTH |
| MAX_TRIGGER_LINKS | max_trigger_links | 4 | 4 | 4 | 4 | 4 | MU_MAX_TRIGGER_LINKS |
| MAX_WHERE_ELEMENTS | max_where_elements | 8 | 8 | 8 | 8 | 8 | MU_MAX_WHERE_ELEMENTS |
| MAX_WHERE_OPERANDS | max_where_operands | 16 | 16 | 16 | 16 | 16 | MU_MAX_WHERE_OPERANDS |
| WHERE_BLOB_BYTES | where_blob_bytes | 64 | 64 | 64 | 64 | 64 | MU_WHERE_BLOB_BYTES |
| MAX_ADDRESS_SPACE_NODES | max_address_space_nodes | 64 | 64 | 512 | 512 | 512 | MU_MAX_ADDRESS_SPACE_NODES |
| MAX_DYNAMIC_NODES | max_dynamic_nodes | 32 | 32 | 32 | 32 | 32 | MU_MAX_DYNAMIC_NODES |
| MAX_DYNAMIC_REFERENCES | max_dynamic_references | 64 | 64 | 64 | 64 | 64 | MU_MAX_DYNAMIC_REFERENCES |
| MAX_DYNAMIC_BROWSE_NAME_LENGTH | max_dynamic_browse_name_length | 64 | 64 | 64 | 64 | 64 | MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH |
| MAX_DYNAMIC_DISPLAY_NAME_LENGTH | max_dynamic_display_name_length | 64 | 64 | 64 | 64 | 64 | MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH |
| MAX_DYNAMIC_STRING_NODEID_LENGTH | max_dynamic_string_nodeid_length | 64 | 64 | 64 | 64 | 64 | MU_MAX_DYNAMIC_STRING_NODEID_LENGTH |
| MAX_QUERY_CONTINUATION_POINTS | max_query_continuation_points | 2 | 2 | 2 | 2 | 2 | MU_MAX_QUERY_CONTINUATION_POINTS |
| MAX_CONDITIONS | max_conditions | 10 | 10 | 10 | 10 | 10 | MU_MAX_CONDITIONS |
| MAX_SECURE_CHANNELS | max_secure_channels | 1 | 2 | 4 | 50 | 100 | MU_MAX_SECURE_CHANNELS |
| MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH | max_dynamic_reference_string_nodeid_length | 64 | 64 | 64 | 64 | 64 | MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH |

### Unavailable OPC items in Kconfig

The following OPC items are tracked in the manifest but are NOT implemented. They appear in the generated `Kconfig` as visible `comment` directives so they show up in `menuconfig` for roadmap awareness, but they carry no config symbol and cannot be selected, toggled, or set in `.config`. This makes the full OPC feature surface visible to developers without implying any implementation claim.

| Item | OPC reference | State | Notes |
|------|---------------|-------|-------|
| opc_file_server_facet | OPC-10000-20 File Server Facet | unimplemented | File Server Facet not implemented; defer until there is demand. |
| opc_json_encoding | OPC-10000-6 §5.3 JSON Encoding | unimplemented | JSON encoding not implemented; only UA-Binary encoding is supported. |
| opc_xml_encoding | OPC-10000-6 §5.4 XML Encoding | unimplemented | XML encoding not implemented; only UA-Binary encoding is supported. |
| opc_https_transport | OPC-10000-7 HTTPS Transport | unimplemented | HTTPS transport not implemented; only opc.tcp transport is supported. |
| opc_websocket_transport | OPC-10000-7 WebSocket Transport | unimplemented | WebSocket transport not implemented; only opc.tcp transport is supported. |
| opc_monitor_items_500 | OPC-10000-4 §5.13.2 Monitor Items 500 | documented | Satisfied by project-level CU opc_cu_subscription_standard; support for 500+ monitored items is stubbed as it requires substantial memory allocation. See src/cu/core_2022_server/monitor_items_500/stub.c.disabled. |
| opc_monitor_minqueuesize_05 | OPC-10000-4 §5.13.2 Monitor MinQueueSize_05 | documented | Satisfied by project-level CU opc_cu_subscription_standard; MinQueueSize_05 tuning is stubbed. See src/cu/core_2022_server/monitor_minqueuesize_05/stub.c.disabled. |
| opc_facet_1029 | OPC-10000-7 §4.2 | unimplemented | GDS AliasName Server Facet not implemented; GDS infrastructure is not planned. |
| opc_facet_1636 | OPC-10000-7 §4.2 | unimplemented | AliasName Server Facet not implemented; AliasName feature is deferred. |
| opc_facet_1637 | OPC-10000-7 §4.2 | unimplemented | AliasName Aggregating Server Facet not implemented; AliasName feature is deferred. |
| opc_cu_2600 |  | documented | Support at least one Security Policy. Support of SecurityPolicy None is recommended for testing and compatibility reasons even if the UA Server supports a more secure policy. |
| opc_cu_2809 |  | documented | Support setting the NonatomicRead and NonatomicWrite flags in the AccessLevelEx Attribute for Variable Nodes to indicate whether Read or Write operations can be performed in atomic manner. If the flags are set to '1', atomicity cannot be assured. |
| opc_cu_2820 |  | documented | Support setting the WriteFullArrayOnly flag in the AccessLevelEx Attribute for Variable Nodes of non-scalar data types to indicate whether write operations for an array can be performed with an IndexRange. |
| opc_cu_3184 |  | documented | Satisfied by project-level CU opc_cu_core_structure_2; Root/Objects/Server base structure with ServerArray/NamespaceArray/ServerStatus/ServiceLevel/ServerCapabilities exposed in base_nodes.c. |
| opc_cu_3186 |  | documented | Satisfied by project-level CU opc_cu_core_views_folder; Views Object entry point exposed in base_nodes.c. |
| opc_cu_3545 |  | documented | Satisfied by project-level CU opc_cu_namespace_metadata; NamespaceMetadataType exposed + namespace metadata for static-NodeId namespaces. |
| opc_cu_3554 |  | documented | Satisfied by project-level CU opc_cu_address_space_base; supports Object/ObjectType/Variable/VariableType/ReferenceType/DataType NodeClasses with Attributes and References. |
| opc_cu_3808 |  | documented | Documented (spec 078): the core capacities (SecureChannels/Sessions/ContinuationPoints/Subscriptions/PublishRequests/MonitoredItems/queue depth/retransmission) are specified in docs/integration-guide.md §2.2.1 and capacities.h, and discoverable at runtime via the ServerCapabilities/OperationLimits nodes. Documentation CU (no code). The application documentation shall specify the core OPC UA related capacities. This includes the number of supported SecureChannels, Sessions, and Continuation Points for the View Services. If Subscriptions are supported, it shall also include capacity information for Subscriptions and Publish requests, MonitoredItems, retransmission queue, and the queue for sampled MonitoredItems. (Documentation complete; no code change needed.) |
| opc_cu_3912 |  | documented | Satisfied by project-level CU opc_cu_server_capabilities_2; ServerProfileArray/LocaleIdArray/MinSupportedSampleRate/MaxBrowseContinuationPoints/MaxArrayLength/MaxStringLength/MaxByteStringLength/MaxSessions exposed in base_nodes.c. |
| opc_cu_4237 |  | documented | Support setting the NonVolatile and Constant flags in the AccessLevelEx Attribute for Variable Nodes to indicate whether persistent storage is supported. |
| opc_cu_2231 |  | documented | Push Model for Certificate and TrustList Management. ServerConfigurationType in base_nodes.c, UpdateCertificate+ApplyChanges Method stubs with adapter interface. The server accepts certificate pushes from an external GDS/agent; integrator provides storage adapter. |
| opc_cu_2423 |  | documented | Exposes the RationalNumberType and RationalNumber, all their supertypes and for the DataType the Encoding Objects in the AddressSpace. |
| opc_cu_2481 |  | documented | Exposes the NormalizedString DataType and all its supertypes in the AddressSpace |
| opc_cu_2482 |  | documented | Exposes the DecimalString DataType and all its supertypes in the AddressSpace |
| opc_cu_2483 |  | documented | Reconciled (spec 079): DurationString(12879)/TimeString(12880)/DateString(12881) exposed as subtypes of String(12) in base_nodes.c; test_type_system. Satisfied by opc_cu_base_info_datatypes. Exposes the DurationString, TimeString, and DateString DataTypes and all their supertypes in the AddressSpace |
| opc_cu_2484 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2485 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2490 |  | documented | Standard OPC UA defining type/reference automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2491 |  | documented | Standard OPC UA defining type/reference automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2500 |  | documented | Satisfied by project-level CU opc_cu_5592; EUInformation DataType (887) is defined in base_nodes.c and exposed in the AddressSpace alongside its encoding objects and supertype (Structure, 22). |
| opc_cu_2512 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2513 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2514 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2516 |  | documented | Standard OPC UA defining type/reference automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2517 |  | documented | Standard OPC UA defining type/reference automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2518 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_2536 |  | documented | Exposes the ContentFilter DataType and its encoding Objects and all its supertypes in the AddressSpace |
| opc_cu_2928 |  | documented | Supports an absolute Deadband filter as a DataChangeFilter for numeric data types. |
| opc_cu_2940 |  | documented | The Server supports obtaining subscription information via GetMonitoredItems Method on the Server object. |
| opc_cu_2963 |  | documented | Support the following MonitoredItem Services: CreateMonitoredItems, ModifyMonitoredItems, DeleteMonitoredItems and SetMonitoringMode. |
| opc_cu_3146 |  | documented | Support the SetTriggering Service to create and/or delete triggering links for a triggering item. |
| opc_cu_3185 |  | documented | Reconciled (spec 080b): the core type-system Folder Nodes Types(86)/ObjectTypes(88)/DataTypes(90)/VariableTypes(89)/ReferenceTypes(91) are exposed in base_nodes.c and asserted by test_type_system. Satisfied by opc_cu_base_info_base_types. Exposes entry points into the type system in the AddressSpace. Specifically, these are the Folder Nodes: Types, ObjectTypes, DataTypes, VariableTypes, and ReferenceTypes. |
| opc_cu_3188 |  | documented | Reconciled (spec 080b): the full base OPC UA type system is exposed in base_nodes.c and asserted by test_type_system -- all built-in/abstract DataTypes with supertype closure (primitives re-parented under Integer(27)/UInteger(28)/Number(26)), the base Object/Variable/ReferenceTypes, ModellingRuleType(77) + its ModellingRule Objects (78/80/83/11508/11510), and EnumValueType(7594)/Union(12756) with EnumValueType Encoding Objects (7616/8251). Satisfied by opc_cu_base_info_base_types. Supports type information of the base OPC UA concepts, like build-in DataTypes, base Object- and VariableTypes, and base ReferenceTypes. Includes the Encoding Objects for the DataTypes that are not Build-in or abstract DataTypes. Exposes the ObjectTypes BaseObjectType, FolderType, DataTypeEncodingType and ModellingRuleType in the AddressSpace. Exposes the VariableTypes BaseVariableType, PropertyType, and BaseDataVariableType in the AddressSpace. Exposes the DataTypes BaseDataType Boolean, ByteString, DateTime, DataValue, DiagnosticsInfo, Enumeration, ExpandedNodeId, Guid, LocalizedText, NodeId, Number, QualifiedName, String, Structure, XmlElement, Integer, UInteger, Double, Float, Sbyte, Int16, Int32, Int64, Byte, Uint16, Uint32, Uint64, StatusCode, UtcTime, Duration, NumericRange, EnumValueType, and Union and their Encoding Objects in the AddressSpace. Exposes the ReferenceTypes References, HierarchicalReferences, NonHierarchicalReferences, HasChild, Organizes, HasModellingRule, HasTypeDefinition, HasEncoding, Aggregates, HasSubtype, HasComponent, and HasProperty in the AddressSpace Exposes the Objects Optional, Mandatory, OptionalPlaceholder, MandatoryPlaceholder and ExposesItsArrayin the AddressSpace. |
| opc_cu_3196 |  | documented | Satisfied by project-level CU opc_cu_subscription_basic. |
| opc_cu_3207 |  | documented | Satisfied by project-level CU opc_cu_3127; OptionSetType (11487) is defined in base_nodes.c and exposed in the AddressSpace alongside its encoding objects and supertypes. |
| opc_cu_3214 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3532 |  | documented | Support at least 2 queue entries for MonitoredItems. Servers often will adapt the queue size to the number of currently monitored Items. It is expected that Servers support the documented queue capacity for at least one third of the supported MonitoredItems. |
| opc_cu_3544 |  | documented | Support the standard Method ResendData to get the latest value of the monitored items of a Subscription. |
| opc_cu_3547 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3550 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3551 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3641 |  | documented | Reconciled (spec 080a): Argument(i=296, subtype of Structure) + its DefaultBinary(298)/DefaultXml(297) Encoding Objects (DataTypeEncodingType 76) exposed in base_nodes.c; test_type_system. Satisfied by opc_cu_base_info_argument_type. Exposes the Argument DataType, its Encoding Objects and all its supertypes in the AddressSpace. |
| opc_cu_3644 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3747 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3748 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3749 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3750 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3751 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3752 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3753 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3754 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3755 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3756 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3757 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3758 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3759 |  | documented | Standard OPC UA ReferenceType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_3911 |  | documented | Reconciled (spec 073): ServerCapabilities exposes AggregateFunctions(2997), MaxSubscriptions(24096), MaxMonitoredItems(24097), MaxSubscriptionsPerSession(24098), MaxMonitoredItemsPerSubscription(24104) + existing MaxMonitoredItemsPerCall(11714) in base_nodes.c (both address-space tables), advertised values == enforced MU_INTERN_* caps; test_operation_limits::test_subscription_capability_nodes_resolve. Satisfied by opc_cu_subscription_basic. Exposes AggregateFunctions, MaxSubscriptions, MaxMonitoredItems, MaxSubscriptionsPerSession and MaxMonitoredItemsPerSubscription of the ServerCapabilities Object as well as MaxMonitoredItemsPerCall of the OperationLimits Object. |
| opc_cu_3922 |  | documented | Reconciled (spec 073): mu_server_signal_semantic_change (public API) latches per-MonitoredItem; the next DataChange Notification sets StatusCode bit 14 (0x4000, SemanticsChanged) then the one-shot latch clears (notification.c + deadband.c, OPC-10000-4 §7.38.1); test_subscriptions::test_publish_semantics_changed_bit (E2E emit->decode). Satisfied by opc_cu_subscription_basic. Supports setting the SemanticsChanged Bit in the statusCode when a semantic change occurs, such as a change in the engineering unit associated with the Value Attribute. |
| opc_cu_3996 |  | documented | Standard OPC UA defining type/reference automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_4052 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_4054 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_4055 |  | documented | Reconciled (spec 073): MaxMonitoredItemsQueueSize(31916) exposed on ServerCapabilities in base_nodes.c, advertised value == MU_INTERN_MONITORED_QUEUE_DEPTH; test_operation_limits::test_subscription_capability_nodes_resolve. Satisfied by opc_cu_subscription_basic. Exposes MaxMonitoredItemsQueueSize of the ServerCapabilities Object. |
| opc_cu_4426 |  | documented | Reconciled (spec 079): Decimal(i=50) + its supertype Number(i=26) exposed in base_nodes.c type-system table with HasSubtype closure to BaseDataType; test_type_system. Satisfied by opc_cu_base_info_datatypes. Exposes the DataType Decimal and all its supertypes in the AddressSpace. |
| opc_cu_5207 |  | documented | Support at least 2 MonitoredItems per Subscription where the size of each MonitoredItem is at least equal to size of Double. |
| opc_cu_5208 |  | documented | Reconciled (spec 073): MonitoredItem IndexRange is parsed at create (subscription_helpers.c), rejected with Bad_IndexRangeInvalid when malformed, and applied to array samples via apply_numeric_index_range (read_attribute.c) in read_monitored_item_value; test_monitored_index_range (single element, slice, whole-array, out-of-bounds). Satisfied by opc_cu_subscription_basic. Support creation of MonitoredItems for Attribute value changes. This includes support of the IndexRange to select a single element or a range of elements when the Attribute value is an array. This ConformanceUnit does not require queuing when multiple value changes occur during a "publish period". I.e. the latest change will be sent in the Notification. |
| opc_cu_5868 |  | documented | Standard OPC UA DataType automatically exposed through the type system (see opc_cu_base_info_datatypes / opc_cu_base_info_base_types). No dedicated CU implementation file is required. |
| opc_cu_custom_methods | OPC-10000-4 §5.11 Core 2022 Server Facet | documented | Project-level Conformance Unit mirroring the legacy 'custom_methods' item; emits Kconfig symbol MUC_OPCUA_CU_CUSTOM_METHODS matching the #ifdef guard renamed in Task 3 of the CU-aligned code reorganisation plan. |
| opc_cu_diagnostics | OPC-10000-5 §6.3 Core 2022 Server Facet | documented | Project-level Conformance Unit mirroring the legacy 'server_diagnostics' item; emits Kconfig symbol MUC_OPCUA_CU_DIAGNOSTICS matching the #ifdef guard renamed in Task 3 of the CU-aligned code reorganisation plan. |
| opc_cu_dynamic_nodes | OPC-10000-3 Core 2022 Server Facet | documented | Project-level Conformance Unit mirroring the legacy 'dynamic_nodes' item; emits Kconfig symbol MUC_OPCUA_CU_DYNAMIC_NODES matching the #ifdef guard renamed in Task 3 of the CU-aligned code reorganisation plan. |
| opc_cu_aggregate_interpolative | OPC-10000-13 §4.2.2.3 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_average | OPC-10000-13 §4.2.2.4 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_time_average | OPC-10000-13 §4.2.2.5 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_time_average_2 | OPC-10000-13 §4.2.2.6 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_total | OPC-10000-13 §4.2.2.7 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_total_2 | OPC-10000-13 §4.2.2.8 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_minimum | OPC-10000-13 §4.2.2.9 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_maximum | OPC-10000-13 §4.2.2.10 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_range | OPC-10000-13 §4.2.2.13 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_minimum_2 | OPC-10000-13 §4.2.2.14 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_maximum_2 | OPC-10000-13 §4.2.2.15 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_count | OPC-10000-13 §4.2.2.19 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_duration_state_zero | OPC-10000-13 §4.2.2.20 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_start | OPC-10000-13 §4.2.2.23 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_end | OPC-10000-13 §4.2.2.24 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_delta | OPC-10000-13 §4.2.2.25 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_delta_bounds | OPC-10000-13 §4.2.2.28 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_duration_good | OPC-10000-13 §4.2.2.29 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_duration_bad | OPC-10000-13 §4.2.2.30 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_percent_good | OPC-10000-13 §4.2.2.31 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_percent_bad | OPC-10000-13 §4.2.2.32 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_worst_quality | OPC-10000-13 §4.2.2.33 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_worst_quality_2 | OPC-10000-13 §4.2.2.34 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_annotation_count | OPC-10000-13 §4.2.2.35 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_min_actual_time | OPC-10000-13 §4.2.2.11 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_max_actual_time | OPC-10000-13 §4.2.2.12 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_min_actual_time_2 | OPC-10000-13 §4.2.2.16 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_max_actual_time_2 | OPC-10000-13 §4.2.2.17 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_range_2 | OPC-10000-13 §4.2.2.18 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_duration_state_nonzero | OPC-10000-13 §4.2.2.21 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_number_transitions | OPC-10000-13 §4.2.2.22 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_start_bound | OPC-10000-13 §4.2.2.26 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_end_bound | OPC-10000-13 §4.2.2.27 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_std_dev_sample | OPC-10000-13 §4.2.2.36 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_variance_sample | OPC-10000-13 §4.2.2.37 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_std_dev_population | OPC-10000-13 §4.2.2.38 Core 2022 Server Facet | documented |  |
| opc_cu_aggregate_variance_population | OPC-10000-13 §4.2.2.39 Core 2022 Server Facet | documented |  |
| opc_cu_1571 | OPC-10000-11 | documented | HistoryRead service with ReadRawModifiedDetails. Continuation points, pagination, timestamp filtering, and returnBounds implemented. Satisfied by service_history (MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET). |
| opc_cu_1573 | OPC-10000-11 | documented | HistoryUpdate service with UpdateDataDetails. performInsertReplace field decoded and dispatched via history_adapter.update_data callback. Satisfied by service_history (MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET). |
| opc_cu_1574 | OPC-10000-11 | documented | HistoryUpdate Insert (performInsertReplace=1). Insert rejects existing-timestamp collision; replace requires pre-existing timestamp. Satisfied by service_history (MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET). |
| opc_cu_1575 | OPC-10000-11 | documented | HistoryUpdate Replace (performInsertReplace=2). Replace updates existing-timestamp entry; insert new entries via replace. Satisfied by service_history (MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET). |
| opc_cu_1576 | OPC-10000-11 | documented | HistoryUpdate service with DeleteRawModifiedDetails. isDeleteModified, startTime, endTime decoded and dispatched via history_adapter.delete_raw_modified callback. Satisfied by service_history (MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET). |
| opc_cu_2264 | OPC-10000-11 | documented | Replace single values in history. Satisfied by Historical Data Replace (opc_cu_1575) via service_history. |
| opc_cu_2185 | OPC-10000-11 | documented | Historical Access Structured Data Insert deferred; depends on opc_cu_1710. |
| opc_cu_2332 | OPC-10000-11 | documented | Historical Access Structured Data Read Raw deferred; depends on opc_cu_1710. |
| opc_facet_2242 | OPC-10000-7 §4.2 | unimplemented | LogObject Facet not implemented; no external log-object support planned. |
| opc_facet_2322 | OPC-10000-7 §4.2 | unimplemented | AliasName Configuration Facet not implemented; AliasName feature is deferred. |
| opc_facet_2323 | OPC-10000-7 §4.2 | unimplemented | AliasName Server PubSub Publisher Facet not implemented; AliasName+PubSub deferred. |
| opc_cu_2352 | OPC-10000-4 §5.5.2 | documented | Support the FindServers Service only for itself. |
| opc_cu_2389 | OPC-10000-4 §5.11.4 | documented | Supports writing to values to one or more Attributes of one or more Nodes. Implemented as Value Attribute writes only; non-Value Attribute writes remain rejected. |
| opc_cu_2407 |  | documented | Satisfied by project-level CU opc_cu_user_role_management; basic role-based access control is available, but full security administration (CRUD on user/role objects) is stubbed. See src/cu/core_2022_server/security_administration/stub.c.disabled. |
| opc_cu_2478 |  | documented | Application supports time synchronization via features of a standard operating system. \| Satisfied by the same time-sync infrastructure as CU 5793; the time adapter provides the OS clock. Stub file exists but is not needed — the base time sync CU covers all sync source variants. |
| opc_cu_2479 |  | documented | Application supports time synchronization via the Precision Time Protocol (PTP). |
| opc_cu_2480 |  | documented | Application supports time synchronization via the features described in IEEE 802.1AS. |
| opc_cu_2786 |  | documented | Application supports time synchronization via the Network Time Protocol (NTP). |
| opc_cu_2808 |  | documented | Stub callback interface for role-based access control. The callback is invoked before each service handler; NULL callback allows all. |
| opc_cu_2823 |  | documented | Satisfied by project-level CU opc_cu_user_auth; the ActivateSession handler in activate_session.c validates user identity tokens and returns Bad_IdentityTokenInvalid for invalid tokens per OPC-10000-4. |
| opc_cu_3072 |  | documented | Supports the Read Service to read one or more Attributes of one or more Nodes. This includes support of the IndexRange parameter to read a single element or a range of elements when the Attribute value is an array. |
| opc_cu_3073 |  | documented | Support the RegisterNodes and UnregisterNodes Services as a way to optimize access to repeatedly used Nodes in the Server's OPC UA AddressSpace. |
| opc_cu_3125 |  | documented | Satisfied by opc_cu_user_auth; server supports X.509 certificate-based user authentication (test_user_auth_certificate). |
| opc_cu_3143 |  | documented | Reconciled (spec 073): on Publish-queue overflow handle_publish evicts the OLDEST parked request and answers it with Bad_TooManyPublishRequests, then parks the incoming request (publish_request_evict_oldest, publish_due.c; OPC-10000-4 §5.14.5.1); test_subscriptions_capacity::test_publish_queue_overflow_evicts_oldest_and_parks_newest. Satisfied by opc_cu_subscription_basic. If the maximum supported number of PublishRequests has been queued and a new PublishRequest arrives, the "oldest" PublishRequest has to be discarded by returning the proper error. |
| opc_cu_3175 |  | documented | Satisfied by project-level CU opc_cu_session_base; CreateSession/ActivateSession/CloseSession with correct parameter handling including SecurityMode=None null signatures. |
| opc_cu_3534 |  | documented | Server supports at least 2 Subscriptions in a single Session. |
| opc_cu_3535 |  | documented | Support a retransmission queue of sent NotificationMessages and the Republish Service. See UA Part 4 for the required size of the retransmission queue. muc-opcua: Republish is implemented and tested; the retransmission store holds the single most-recent NotificationMessage per subscription (profile-targeting minimal capacity), which may not meet CTT multi-message republish depth. |
| opc_cu_3536 |  | documented | Reconciled (spec 078): username/password identity tokens with per-policy (endpoint/UserTokenPolicy) password encryption are decrypted+verified in activate_session.c handle_activate_username; test_user_auth_encrypted, test_user_auth_plaintext, test_user_auth_secure_e2e. Satisfied by opc_cu_user_auth. The Server supports User Name/Password combination(s). The token will be encrypted as required by the security policy of the User Token Policy or by the security policy of the endpoint. |
| opc_cu_3645 |  | documented | Satisfied by opc_cu_user_auth; server accepts unencrypted username/password tokens (test_user_auth_plaintext). |
| opc_cu_3727 |  | documented | Support the following Subscription Services: CreateSubscription, ModifySubscription, DeleteSubscriptions, Publish, Republish and SetPublishingMode. |
| opc_cu_3802 |  | documented | Supports configuration of the acceptable clock skew. |
| opc_cu_3913 |  | documented | Support at least 2 Publish Service requests per Session. |
| opc_cu_3985 |  | documented | Satisfied by project-level CU opc_cu_session_general_service; authentication-token validation, requestHandle echo, timeoutHint respect in service_dispatch. |
| opc_cu_5505 |  | documented | Satisfied by project-level CU opc_cu_time_sync; UA-based time synchronisation is stubbed in favour of OS/NTP-based sync. See src/cu/core_2022_server/time_sync_ua_based_support/stub.c.disabled. |
| opc_cu_5793 |  | documented | Support at least one of the optional ConformanceUnits for time synchronization mechanisms in the Security Time Synchronization Facet. The application documentation shall specify which synchronization mechanisms with which profiles are supported. |
| opc_cu_protocol_ua_tcp | OPC-10000-6 §7.1 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_ua_binary_encoding | OPC-10000-6 §5 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_ua_secure_conversation | OPC-10000-6 §6 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_address_space_base | OPC-10000-3 §4 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_session_base | OPC-10000-4 §5.6 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_core_structure_2 | OPC-10000-3 §4 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_core_views_folder | OPC-10000-3 §4 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_server_capabilities_2 | OPC-10000-3 §4 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_namespace_metadata | OPC-10000-3 §4 | documented | Already-implemented CU added to manifest as claimed so the feature is tracked, gated by its Kconfig symbol, and covered by the claim/test map. |
| opc_cu_2318 |  | documented | Reconciled (spec 073): server clamps requested MonitoredItem QueueSize to the compiled max (subscription_monitor.c); tested by test_subscriptions_capacity. Satisfied by subscription_standard. |
| opc_cu_2515 |  | documented | Reconciled (spec 073): address-space-triggered events are queued per subscription and delivered in Publish (notification.c mu_server_trigger_event); tested by test_event_notifications. Satisfied by opc_cu_events. |
| opc_cu_3150 |  | documented | Reconciled (spec 073): MonitoredItem on the EventNotifier attribute delivers EventFieldLists end-to-end (notification.c); tested by test_event_notifications. Satisfied by opc_cu_events. |
| opc_cu_4030 |  | documented | Reconciled (spec 073): combined SELECT + WHERE event filter with operator support and unsupported-operator rejection (filter_reader.c + event_filter.c); tested E2E by test_event_notifications. Satisfied by opc_cu_event_filter_where. |
| opc_cu_2380 |  | documented | Reconciled (spec 073): AddNodes service (nodemanagement/dispatch_node_mgmt.c handle_add_nodes -> mu_add_nodes_process); tested by test_node_management (AddNodes decode/encode, duplicate-NodeId). Satisfied by service_nodemanagement. |
| opc_cu_2394 |  | documented | Reconciled (spec 073): DeleteNodes service (handle_delete_nodes -> mu_delete_nodes_process); tested by test_node_management. Satisfied by service_nodemanagement. |
| opc_cu_2939 |  | documented | Reconciled (spec 073): AddReferences service (handle_add_references -> mu_add_references_process); tested by test_node_management. Satisfied by service_nodemanagement. |
| opc_cu_3153 |  | documented | Reconciled (spec 073): DeleteReferences service (handle_delete_references -> mu_delete_references_process); tested by test_node_management (incl. Bad_NotFound). Satisfied by service_nodemanagement. |
| opc_cu_3194 |  | documented | Reconciled (spec 074): the Server Object (i=2253) exposes a readable EventNotifier attribute (SubscribeToEvents) so a client can discover it as an event source; read_attribute.c + base_nodes.c, tested by test_read_service::test_read_service_eventnotifier. |
| opc_cu_2422 |  | documented | AuditOpenSecureChannelEvent (i=2060) emitted on OpenSecureChannel (osc_handler.c) and observable via the Server EventNotifier. Satisfied by opc_cu_auditing. |
| opc_cu_3968 |  | documented | Reconciled (spec 074): the server emits + delivers AuditEvents for CreateSession/ActivateSession/Write (audited service subset); test_event_notifications. Satisfied by opc_cu_auditing. |
| opc_cu_3228 |  | documented | AuditWriteUpdateEvent (i=2100) emitted per attribute write (attribute_handler.c) and observable via the Server EventNotifier. Satisfied by opc_cu_auditing. |
| opc_cu_3224 | OPC-10000-5 | documented | Auditing infrastructure exists (spec 074). AuditNodeManagementEvent generation is wired through the auditing event emitter. |
| opc_cu_3230 | OPC-10000-5 | documented | Auditing infrastructure exists (spec 074). AuditUpdateMethodEvent generation is wired through the auditing event emitter. |
| opc_cu_3763 | OPC-10000-9 §5.10 | documented | AuditConditionEnableEventType (i=2803) emitted for condition state changes (set_active). AuditConditionAcknowledgeEventType (i=8944) emitted for Acknowledge method. Satisfied by opc_cu_auditing. |
| opc_cu_3764 | OPC-10000-9 §5.10.5 | documented | AuditConditionRespondEventType (i=8927) emitted for DialogCondition Respond method. Satisfied by opc_cu_auditing. |
| opc_cu_3766 | OPC-10000-9 §5.10.7 | documented | AuditConditionConfirmEventType (i=8961) emitted for Confirm method. Satisfied by opc_cu_auditing. |
| opc_cu_3767 | OPC-10000-9 §5.10.8 | documented | Requires A & C Shelving State feature (not yet implemented). Deferred until shelving support is added. |
| opc_cu_3768 | OPC-10000-9 §5.10.9 | documented | Requires A & C Suppression State feature (not yet implemented). Deferred until suppression support is added. |
| opc_cu_2190 | OPC-10000-4 §5.6.5 | documented | Cancel service stub responds with Count=0. Embedded server processes requests synchronously. |
| opc_cu_2863 | OPC-10000-7 §6.5 | documented | Imported from OPC profile REST API 2026-07-15. Capability satisfied via the secure-conversation layer, which implements Basic256Sha256, Aes128_Sha256_RsaOaep, and Aes256_Sha256_RsaPss; Kconfig gates per-profile inclusion. |
| opc_cu_3170 | OPC-10000-4 §5.4.6 | documented | RegisterServer2 is implemented alongside RegisterServer; satisfied by the same service handler. |
| opc_cu_3721 | OPC-10000-7 §6.5 | documented | Imported from OPC profile REST API 2026-07-15. Optional. Capability satisfied by MUC_OPCUA_CU_SECURITY_ECC (spec 059); Kconfig gates per-profile inclusion (built in full). |
| opc_cu_3923 | OPC-10000-4 §5.6 | documented | Imported from OPC profile REST API 2026-07-15. Capability satisfied by MUC_OPCUA_CU_MULTIPLE_CONNECTIONS (multiple parallel sessions); Kconfig gates per-profile inclusion. |
| opc_cu_3080 |  | documented | An application, when installed, has a default ApplicationInstanceCertificate that is valid. The default ApplicationInstanceCertificate shall either be created as part of the installation or installation instructions explicitly describe the process to create and apply a default ApplicationInstanceCertificate to the application. (Documentation complete; no code change needed.) |
| opc_cu_3201 |  | documented | No custom types are required by the standard profiles; the built-in type system covers all required types. |
| opc_cu_5814 |  | documented | The server supports anonymous authentication and SecurityPolicy None (spec 072); anonymous identity is accepted during ActivateSession. No application instance certificate is required in the SecurityPolicy None path. |
| opc_cu_2921 | OPC-10000-9 §5.10 | documented | AlarmConditionType and state tracking. Satisfied by opc_cu_alarms_conditions. |
| opc_cu_2927 | OPC-10000-9 §5.7 | documented | AcknowledgeableConditionType with Acknowledge/Confirm method dispatch. Satisfied by opc_cu_alarms_conditions. |
| opc_cu_2189 | OPC-10000-9 §5.9 | documented | ConditionClasses type nodes deferred. Satisfied by opc_cu_alarms_conditions. |
| opc_cu_2726 | OPC-10000-9 §5.10.3 | documented | FirstInGroupAlarmType deferred. Satisfied by opc_cu_alarms_conditions. |
| opc_cu_2852 | OPC-10000-9 §5.9.11 | documented | ConditionSubClassType deferred. Satisfied by opc_cu_alarms_conditions. |
| opc_cu_2879 | OPC-10000-9 §5.10.4 | documented | ReAlarmType/OffNormalAlarmType deferred. Satisfied by opc_cu_alarms_conditions. |
| opc_cu_2361 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2399 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2426 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2474 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2772 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2776 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2831 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2984 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2988 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3112 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3323 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3324 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3325 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3326 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3327 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3328 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3565 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3566 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3567 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3568 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3569 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_3786 |  | documented | Reconciled (spec 079): satisfied by Data Access (opc_cu_data_access). |
| opc_cu_2489 |  | documented | Reconciled: satisfied by service_nodemanagement. |
| opc_cu_2649 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_2747 |  | documented | Reconciled: satisfied by opc_cu_base_info_base_types. |
| opc_cu_2813 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_2814 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_2822 |  | documented | Reconciled: satisfied by opc_cu_base_info_base_types. |
| opc_cu_2978 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3199 |  | documented | Reconciled: satisfied by opc_cu_base_info_base_types. |
| opc_cu_3206 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3210 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3211 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3546 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3549 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3810 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3811 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3812 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_3813 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_4427 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_5578 |  | documented | Reconciled: satisfied by opc_cu_base_info_datatypes. |
| opc_cu_5941 |  | documented | Spec CU 5941: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5940 |  | documented | Spec CU 5940: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5937 |  | documented | Spec CU 5937: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5875 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_5874 |  | documented | Spec CU 5874: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5873 |  | documented | Spec CU 5873: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5869 |  | documented | Spec CU 5869: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5813 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_5812 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_5810 |  | documented | Satisfied by user authentication implementation; Kerberos-specific Windows negotiation deferred. |
| opc_cu_5809 |  | documented | Satisfied by user authentication implementation including JWT (IssuedToken) token support. |
| opc_cu_5808 |  | documented | Spec CU 5808: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5807 |  | documented | Spec CU 5807: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5806 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_5797 |  | documented | Spec CU 5797: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5796 |  | documented | Spec CU 5796: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5795 |  | documented | Spec CU 5795: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5791 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_5776 |  | documented | Spec CU 5776: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5775 |  | documented | Spec CU 5775: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5664 |  | documented | Spec CU 5664: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5663 |  | documented | Spec CU 5663: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5662 |  | documented | Spec CU 5662: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5661 |  | documented | Spec CU 5661: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5660 |  | documented | Spec CU 5660: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5659 |  | documented | Spec CU 5659: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5658 |  | documented | Spec CU 5658: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5656 |  | documented | Spec CU 5656: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5655 |  | documented | Spec CU 5655: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5654 |  | documented | Spec CU 5654: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5653 |  | documented | Spec CU 5653: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5652 |  | documented | Spec CU 5652: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5567 |  | documented | Spec CU 5567: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5566 |  | documented | Spec CU 5566: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5565 |  | documented | Spec CU 5565: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5564 |  | documented | Spec CU 5564: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5563 |  | documented | Spec CU 5563: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5562 |  | documented | Spec CU 5562: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5561 |  | documented | Spec CU 5561: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5560 |  | documented | Spec CU 5560: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5559 |  | documented | Spec CU 5559: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5558 |  | documented | Spec CU 5558: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5557 |  | documented | Spec CU 5557: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5556 |  | documented | Spec CU 5556: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5555 |  | documented | Spec CU 5555: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5554 |  | documented | Spec CU 5554: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5553 |  | documented | Spec CU 5553: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5552 |  | documented | Spec CU 5552: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5551 |  | documented | Spec CU 5551: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5550 |  | documented | Spec CU 5550: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5549 |  | documented | Spec CU 5549: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5548 |  | documented | Spec CU 5548: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5547 |  | documented | Spec CU 5547: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5546 |  | documented | Spec CU 5546: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5545 |  | documented | Spec CU 5545: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5544 |  | documented | Spec CU 5544: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5543 |  | documented | Spec CU 5543: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5542 |  | documented | Spec CU 5542: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5541 |  | documented | Spec CU 5541: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5540 |  | documented | Spec CU 5540: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5539 |  | documented | Spec CU 5539: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5538 |  | documented | Spec CU 5538: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5537 |  | documented | Spec CU 5537: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5536 |  | documented | Spec CU 5536: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5535 |  | documented | Spec CU 5535: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5534 |  | documented | Spec CU 5534: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5533 |  | documented | Spec CU 5533: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5532 |  | documented | Spec CU 5532: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5531 |  | documented | Spec CU 5531: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5530 |  | documented | Spec CU 5530: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5529 |  | documented | Spec CU 5529: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5528 |  | documented | Spec CU 5528: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5527 |  | documented | Spec CU 5527: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5526 |  | documented | Spec CU 5526: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5525 |  | documented | Spec CU 5525: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5524 |  | documented | Spec CU 5524: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5523 |  | documented | Spec CU 5523: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5522 |  | documented | Spec CU 5522: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5521 |  | documented | Spec CU 5521: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5520 |  | documented | Spec CU 5520: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5519 |  | documented | Spec CU 5519: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5518 |  | documented | Spec CU 5518: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5517 |  | documented | Spec CU 5517: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5516 |  | documented | Spec CU 5516: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5515 |  | documented | Spec CU 5515: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5514 |  | documented | Spec CU 5514: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5513 |  | documented | Spec CU 5513: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5512 |  | documented | Spec CU 5512: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5511 |  | documented | Spec CU 5511: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5510 |  | documented | Spec CU 5510: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5303 |  | documented | Spec CU 5303: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5302 |  | documented | Spec CU 5302: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5301 |  | documented | Spec CU 5301: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5293 |  | documented | Satisfied by KeyCredential service implementation. |
| opc_cu_5292 |  | documented | Spec CU 5292: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5277 |  | documented | Satisfied by role management implementation. |
| opc_cu_5276 |  | documented | Satisfied by role management implementation. |
| opc_cu_5275 |  | documented | Satisfied by role management implementation. |
| opc_cu_5274 |  | documented | Satisfied by role management implementation. |
| opc_cu_5250 |  | documented | Spec CU 5250: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5249 |  | documented | Spec CU 5249: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5248 |  | documented | Spec CU 5248: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5242 |  | documented | Spec CU 5242: tracked in manifest; not yet claimed or implemented. |
| opc_cu_5213 |  | documented | Spec CU 5213: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4957 |  | documented | Satisfied by user authentication implementation. |
| opc_cu_4505 |  | documented | Satisfied by role management and user auth infrastructure. |
| opc_cu_4503 |  | documented | Spec CU 4503: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4502 |  | documented | Spec CU 4502: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4501 |  | documented | Spec CU 4501: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4500 |  | documented | Spec CU 4500: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4467 |  | documented | Spec CU 4467: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4466 |  | documented | Spec CU 4466: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4465 |  | documented | Spec CU 4465: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4464 |  | documented | Spec CU 4464: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4463 |  | documented | Spec CU 4463: tracked in manifest; not yet claimed or implemented. |
| opc_cu_4428 |  | documented | Spec CU 4428: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3994 |  | documented | Spec CU 3994: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3979 |  | documented | Satisfied by auditing implementation. |
| opc_cu_3969 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_3965 |  | documented | Reconciled: Readable UserAccessLevel attribute on Variable nodes (OPC-10000-3 §5.5.9). Backed by read_attribute.c unconditionally. |
| opc_cu_3941 |  | documented | Reconciled: DataTypeDefinition attribute on DataType nodes (OPC-10000-5 §12.2.12.3). Backed by base_nodes.c DataType nodes and read_attribute.c attribute handling. |
| opc_cu_3928 |  | documented | Satisfied by user authentication implementation (Anonymous token support). |
| opc_cu_3820 |  | documented | Satisfied by user authentication implementation; Kerberos Windows negotiation deferred. |
| opc_cu_3779 |  | documented | Spec CU 3779: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3778 |  | documented | Spec CU 3778: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3777 |  | documented | Spec CU 3777: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3776 |  | documented | Spec CU 3776: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3775 |  | documented | Spec CU 3775: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3774 |  | documented | Spec CU 3774: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3773 |  | documented | Spec CU 3773: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3772 |  | documented | Spec CU 3772: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3771 |  | documented | Spec CU 3771: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3770 |  | documented | Spec CU 3770: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3765 |  | documented | Spec CU 3765: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3762 |  | documented | Spec CU 3762: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3761 |  | documented | Spec CU 3761: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3760 |  | documented | Spec CU 3760: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3642 |  | documented | Spec CU 3642: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3605 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_3586 |  | documented | Satisfied by role management implementation. |
| opc_cu_3584 |  | documented | Spec CU 3584: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3582 |  | documented | Satisfied by certificate management implementation. |
| opc_cu_3581 |  | documented | Spec CU 3581: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3577 |  | documented | Spec CU 3577: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3576 |  | documented | Spec CU 3576: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3574 |  | documented | Spec CU 3574: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3572 |  | documented | Spec CU 3572: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3571 |  | documented | Spec CU 3571: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3562 |  | documented | Reconciled: Method metadata with HasArgumentDescription/HasOptionalInputArgumentDescription. Backed by read_attribute.c and base_nodes.c method nodes. |
| opc_cu_3542 |  | documented | Satisfied by role management implementation. |
| opc_cu_3541 |  | documented | Satisfied by role management implementation. |
| opc_cu_3540 |  | documented | Satisfied by role management implementation. |
| opc_cu_3539 |  | documented | Satisfied by role management implementation. |
| opc_cu_3538 |  | documented | Satisfied by role management implementation. |
| opc_cu_3525 |  | documented | Reconciled: URI-based dictionary references. Backed by base_nodes.c type-system exposure and dictionary entries. |
| opc_cu_3524 |  | documented | Reconciled: IRDI-based dictionary references. Backed by base_nodes.c type-system exposure and dictionary entries. |
| opc_cu_3226 |  | documented | Satisfied by auditing implementation. |
| opc_cu_3213 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_3203 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_3197 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_3182 |  | documented | Satisfied by role management implementation. |
| opc_cu_3171 |  | documented | Spec CU 3171: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3165 |  | documented | Spec CU 3165: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3142 |  | documented | Spec CU 3142: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3137 |  | documented | Spec CU 3137: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3121 |  | documented | Spec CU 3121: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3107 |  | documented | Spec CU 3107: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3098 |  | documented | Spec CU 3098: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3084 |  | documented | Spec CU 3084: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3083 |  | documented | Spec CU 3083: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3081 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_3064 |  | documented | Reconciled: EventNotifier attribute on Object/View nodes (OPC-10000-3 §5.4.6). Backed by mu_node_t.event_notifier field and read_attribute.c. |
| opc_cu_3060 |  | documented | Spec CU 3060: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3053 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_3049 |  | documented | Spec CU 3049: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3043 |  | documented | Spec CU 3043: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3027 |  | documented | Spec CU 3027: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3026 |  | documented | Reconciled: Multi-level UserWriteMask propagation from TypeDefinition nodes. Backed by read_attribute.c UserWriteMask attribute read. |
| opc_cu_3020 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_3015 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_3004 |  | documented | Spec CU 3004: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3001 |  | documented | Spec CU 3001: tracked in manifest; not yet claimed or implemented. |
| opc_cu_3000 |  | documented | Spec CU 3000: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2993 |  | documented | Spec CU 2993: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2991 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2965 |  | documented | Spec CU 2965: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2957 |  | documented | Spec CU 2957: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2951 |  | documented | Spec CU 2951: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2950 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2947 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2946 |  | documented | Spec CU 2946: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2943 |  | documented | Satisfied by Historical Access Server Facet; event history deferred. |
| opc_cu_2937 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2929 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2918 |  | documented | Reconciled: Source hierarchy browsing via HasProperty/HasComponent references in base_nodes.c. Browse service resolves these forward/reverse. |
| opc_cu_2902 |  | documented | Spec CU 2902: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2897 |  | documented | Spec CU 2897: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2896 |  | documented | Spec CU 2896: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2893 |  | documented | Spec CU 2893: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2881 |  | documented | Spec CU 2881: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2877 |  | documented | Spec CU 2877: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2873 |  | documented | Satisfied by role management implementation. |
| opc_cu_2871 |  | documented | Spec CU 2871: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2861 |  | documented | Spec CU 2861: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2845 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2818 |  | documented | Spec CU 2818: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2817 |  | documented | Satisfied by user authentication implementation including JWT (IssuedToken) token support. |
| opc_cu_2811 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2806 |  | documented | Satisfied by role management implementation. |
| opc_cu_2802 |  | documented | Satisfied by role management implementation. |
| opc_cu_2785 |  | documented | Spec CU 2785: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2781 |  | documented | Reconciled: Readable WriteMask attribute on Variable nodes. Backed by read_attribute.c unconditionally. |
| opc_cu_2777 |  | documented | Spec CU 2777: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2746 |  | documented | Spec CU 2746: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2740 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2709 |  | documented | Spec CU 2709: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2705 |  | documented | Spec CU 2705: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2664 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2629 |  | documented | Spec CU 2629: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2539 |  | documented | Reconciled: Dictionary entry nodes in the AddressSpace. Backed by base_nodes.c type-system exposure. |
| opc_cu_2527 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2526 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2488 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2487 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2486 |  | documented | Claimed: minimal type stubs and capability markers in place. |
| opc_cu_2454 |  | documented | Spec CU 2454: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2453 |  | documented | Spec CU 2453: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2450 |  | documented | Spec CU 2450: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2449 |  | documented | Spec CU 2449: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2448 |  | documented | Spec CU 2448: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2391 |  | documented | Spec CU 2391: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2390 |  | documented | Spec CU 2390: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2383 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2379 |  | documented | Spec CU 2379: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2362 |  | documented | Reconciled: Method nodes with proper NodeClass and executable attributes (OPC-10000-3 §5.7). Backed by read_attribute.c Executable/UserExecutable handling and base_nodes.c method nodes. |
| opc_cu_2354 |  | documented | Spec CU 2354: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2353 |  | documented | Spec CU 2353: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2345 |  | documented | Spec CU 2345: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2343 |  | documented | Spec CU 2343: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2338 |  | documented | Spec CU 2338: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2333 |  | documented | Spec CU 2333: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2323 |  | documented | Spec CU 2323: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2319 |  | documented | Satisfied by certificate management implementation. |
| opc_cu_2315 |  | documented | Spec CU 2315: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2309 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2291 |  | documented | Spec CU 2291: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2289 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2276 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2275 |  | documented | Spec CU 2275: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2258 |  | documented | Spec CU 2258: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2239 |  | documented | Spec CU 2239: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2236 |  | documented | Satisfied by certificate management implementation. |
| opc_cu_2233 |  | documented | Spec CU 2233: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2232 |  | documented | Spec CU 2232: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2224 |  | documented | Satisfied by Historical Access Server Facet. |
| opc_cu_2203 |  | documented | Spec CU 2203: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2202 |  | documented | Spec CU 2202: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2180 |  | documented | Spec CU 2180: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2165 |  | documented | Spec CU 2165: tracked in manifest; not yet claimed or implemented. |
| opc_cu_2163 |  | documented | Reconciled: Readable UserWriteMask attribute on Variable nodes. Backed by read_attribute.c unconditionally. |
| opc_cu_aliasname | OPC-10000-7 | documented |  |
| opc_cu_scheduler | OPC-10000-7 | documented |  |
<!-- END GENERATED MANIFEST TABLES -->

## Manifest regeneration and validation

All generated files (Kconfig, defconfigs, `capacities.h`, claim map, roadmap,
and the tables above) derive from a single source of truth:
`profiles/opcua-profile-manifest.yaml`. The generator and validator live in
`scripts/profile_manifest/`.

### Regenerating generated files

After editing the manifest, regenerate every downstream artifact in one
command:

```sh
python3 scripts/profile_manifest/generate.py \
    --manifest profiles/opcua-profile-manifest.yaml \
    --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
```

Individual outputs can be regenerated by listing only the ones you need
(e.g. `--outputs kconfig`). The generator is deterministic — running it
twice produces byte-identical files.

### Validating manifest integrity

Run all validation checks in one command:

```sh
python3 scripts/profile_manifest/validate.py --all
```

This runs:

| Check | What it verifies |
|-------|-----------------|
| Manifest validation | Schema integrity, required keys, valid states, profile/dependency/capacity completeness |
| Generated drift check | Committed Kconfig, defconfigs, `capacities.h`, claim map, roadmap, and build-docs tables match generator output byte-for-byte |
| Kconfig parse check | The generated Kconfig parses cleanly under kconfiglib for every profile defconfig |
| Unimplemented-item availability | At least one unimplemented OPC item is visible in Kconfig (as a `comment` directive) but carries no selectable symbol |
| Capacity compatibility | Kconfig capacity int symbols resolve to manifest-declared defaults for every profile |
| Claim/test map validation | Every claimed manifest item has backing tests |

Individual checks are also available via `--manifest-only`, `--check-generated`,
`--check-capacities`, and `--check-claims` flags.

Exit code 0 means all checks passed; exit code 1 means at least one check
failed (errors are printed to stdout).

### Unavailable OPC items in Kconfig

Unimplemented OPC items appear in the generated `Kconfig` as visible `comment`
directives (e.g. `comment "File Server Facet (NOT IMPLEMENTED) [OPC-10000-20]"`).
They show up in `menuconfig` for roadmap awareness but carry **no config
symbol** — they cannot be selected, toggled, or set in `.config`. This makes
the full OPC feature surface visible to developers without implying any
implementation claim.


## Verifying gating behavior

- `scripts/test_profile_gating.sh` — runs the scenarios in the table
  [above](#the-mechanism) against live `cmake` configures and asserts the
  resolved feature values in the generated `muc_opcua_config.cmake` (they are no
  longer CMake cache vars), including the dependency **cascade** (not error) and
  an `nm`-based check that a subtracted feature's code is truly absent from the
  archive. Run it directly; it's not wired into CTest because each scenario is a
  full `cmake` configure and that's too slow for the unit/integration suite.
- `scripts/kconfig/check_baseline.py` — the byte-identity acid test: asserts
  each profile's Kconfig-resolved flag set equals the pre-migration baseline, so
  the migration stays behavior-neutral.
- `cmake --build <dir> --target menuconfig` — browse/edit the whole tree with
  live dependency and help-text feedback; `--target savedefconfig` exports the
  minimal defconfig for a config you arrived at interactively.
- [`scripts/measure_size.sh`](../scripts/measure_size.sh) — cross-compiles
  each named profile for ARM Cortex-M0+ and reports `.text`/`.data`/`.bss`.
  Use it to size any subtraction/addition you make (e.g. confirm "standard
  minus encryption" actually saves the ~10 KB the `MUC_OPCUA_SECURITY` option
  doc string promises).
- [docs/size/feature-size-ledger.md](size/feature-size-ledger.md) — the
  historical record of what each feature costs, per profile.
