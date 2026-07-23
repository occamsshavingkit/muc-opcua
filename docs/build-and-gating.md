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
| `MUC_OPCUA_REVERSE_CONNECT` | Reverse Connect Facet (server-initiated connections) | | | | | ✅ | |
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
| — | opc_cu_2600 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST | opc_cu_2711 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY | opc_cu_2809 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ADDRESS_SPACE_FULL_ARRAY_ONLY | opc_cu_2820 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT | opc_cu_2969 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_OPTIONSET | opc_cu_3127 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME | opc_cu_3198 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES | opc_cu_3560 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER, MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT | opc_cu_4053 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT | opc_cu_4237 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_CURRENCY | opc_cu_5240 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER | opc_facet_1219 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER | opc_facet_1324 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER | opc_facet_1631 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER | opc_facet_1695 | implemented |  |  | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER | opc_facet_1696 | implemented |  |  |  | ✅ | ✅ |  |
| MUC_OPCUA_FACET_EMBEDDED_DATACHANGE_SUBSCRIPTION_2022_SERVER | opc_facet_2250 | implemented |  | ✅ | ✅ | ✅ | ✅ |  |
| OPC_CU_2231 | opc_cu_2231 | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER |
| OPC_CU_2423 | opc_cu_2423 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| OPC_CU_2481 | opc_cu_2481 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| OPC_CU_2482 | opc_cu_2482 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| — | opc_cu_2483 | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_SERVERTYPE | opc_cu_3189 | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION | opc_cu_5801 | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER |
| MUC_OPCUA_CU_SUBSCRIPTION_BASIC | opc_cu_subscription_basic | claimed |  | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_SUBSCRIPTION_STANDARD | opc_cu_subscription_standard | claimed |  |  | ✅ | ✅ | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_BASIC |
| MUC_OPCUA_CU_SECURITY_ECC | opc_cu_security_ecc | claimed |  |  |  |  | ✅ | SECURE_CHANNEL_CRYPTO |
| MUC_OPCUA_CU_EVENTS | opc_cu_events | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_BASIC |
| MUC_OPCUA_CU_DATA_ACCESS | opc_cu_data_access | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_METHOD_SERVER | opc_cu_method_server | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_CUSTOM_METHODS | opc_cu_custom_methods | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_USER_AUTH | opc_cu_user_auth | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_MULTIPLE_CONNECTIONS | opc_cu_multiple_connections | claimed |  | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_EVENT_FILTER_WHERE | opc_cu_event_filter_where | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_EVENTS, MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| MUC_OPCUA_CU_REDUNDANCY | opc_cu_redundancy | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_BASIC |
| MUC_OPCUA_CU_DIAGNOSTICS | opc_cu_diagnostics | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_COMPLEX_TYPES | opc_cu_complex_types | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_AUDITING | opc_cu_auditing | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_EVENTS |
| MUC_OPCUA_CU_DYNAMIC_NODES | opc_cu_dynamic_nodes | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_MULTI_CHUNK | opc_cu_multi_chunk | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_SESSION_TIMEOUT | opc_cu_session_timeout | claimed |  | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_CU_MULTIPLE_CONNECTIONS, MUC_OPCUA_CU_MULTI_CHUNK |
| MUC_OPCUA_CU_TIME_SYNC | opc_cu_time_sync | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_EXTENDED_NODEIDS | opc_cu_extended_nodeids | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_aggregate_interpolative | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_average | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_time_average | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_time_average_2 | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_total | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_total_2 | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_minimum | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_maximum | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_range | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_minimum_2 | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_maximum_2 | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_count | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_duration_state_zero | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_start | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_end | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_delta | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_delta_bounds | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_duration_good | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_duration_bad | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_percent_good | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_percent_bad | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_worst_quality | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_worst_quality_2 | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| — | opc_cu_aggregate_annotation_count | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| MUC_OPCUA_CU_AGGREGATE_FULL | opc_cu_aggregate_full | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD |
| MUC_OPCUA_CU_PUBSUB | opc_cu_pubsub | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_REVERSE_CONNECT | opc_cu_reverse_connect | claimed |  |  |  |  | ✅ |  |
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
| — | opc_cu_2185 | deferred |  |  |  |  |  |  |
| — | opc_cu_2332 | deferred |  |  |  |  |  |  |
| MUC_OPCUA_CU_QUERY | service_query | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_CU_NODEMANAGEMENT | service_nodemanagement | claimed |  |  |  |  | ✅ |  |
| MUC_OPCUA_FACET_UA_TCP_UA_SC_UA_BINARY | opc_facet_837 | implemented | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION | opc_facet_1760 | implemented | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH | opc_cu_2317 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS | opc_cu_2328 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF | opc_cu_2352 | implemented | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES | opc_cu_2389 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_SESSION_CHANGE_USER | opc_cu_2400 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| — | opc_cu_2478 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION |
| MUC_OPCUA_CU_TIME_SYNC_IEEE_1588_PTP | opc_cu_2479 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION |
| MUC_OPCUA_CU_TIME_SYNC_IEEE_802_1AS | opc_cu_2480 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION |
| — | opc_cu_2786 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION |
| MUC_OPCUA_CU_SECURITY_ROLE_SERVER_AUTHORIZATION | opc_cu_2808 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP | opc_cu_2936 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE | opc_cu_3147 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS | opc_cu_3192 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_VIEW_BASIC_2 | opc_cu_3530 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS | opc_cu_3983 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_PROTOCOL_UA_TCP | opc_cu_protocol_ua_tcp | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_UA_TCP_UA_SC_UA_BINARY |
| MUC_OPCUA_CU_UA_BINARY_ENCODING | opc_cu_ua_binary_encoding | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_UA_TCP_UA_SC_UA_BINARY |
| MUC_OPCUA_CU_UA_SECURE_CONVERSATION | opc_cu_ua_secure_conversation | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_UA_TCP_UA_SC_UA_BINARY |
| MUC_OPCUA_CU_ADDRESS_SPACE_BASE | opc_cu_address_space_base | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_SESSION_BASE | opc_cu_session_base | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_CORE_STRUCTURE_2 | opc_cu_core_structure_2 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_CORE_VIEWS_FOLDER | opc_cu_core_views_folder | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_SERVER_CAPABILITIES_2 | opc_cu_server_capabilities_2 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_SESSION_GENERAL_SERVICE | opc_cu_session_general_service | claimed | ✅ | ✅ | ✅ | ✅ | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_NAMESPACE_METADATA | opc_cu_namespace_metadata | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| — | opc_cu_2515 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3194 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_2422 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3228 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3224 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3230 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3763 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3764 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3766 | claimed |  |  |  |  | ✅ |  |
| — | opc_cu_3767 | deferred |  |  |  |  |  |  |
| — | opc_cu_3768 | deferred |  |  |  |  |  |  |
| MUC_OPCUA_CU_SESSION_CANCEL | opc_cu_2190 | claimed |  |  |  | ✅ | ✅ |  |
| MUC_OPCUA_CU_DISCOVERY_REGISTER | opc_cu_2271 | claimed |  |  |  | ✅ | ✅ |  |
| OPC_CU_3080 | opc_cu_3080 | claimed | ✅ | ✅ | ✅ | ✅ | ✅ |  |
| MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS | opc_cu_5592 | claimed |  |  |  |  | ✅ | MUC_OPCUA_FACET_CORE_2022_SERVER |
| MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE | opc_cu_key_credential_service | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION, MUC_OPCUA_CU_METHOD_SERVER |
| MUC_OPCUA_CU_USER_ROLE_MANAGEMENT | opc_cu_user_role_management | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION, MUC_OPCUA_CU_METHOD_SERVER |
| MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT | opc_cu_certificate_management | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION, MUC_OPCUA_CU_METHOD_SERVER |
| MUC_OPCUA_CU_ALARMS_CONDITIONS | opc_cu_alarms_conditions | claimed |  |  |  |  | ✅ | MUC_OPCUA_CU_EVENTS, MUC_OPCUA_FACET_CORE_2022_SERVER |
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
| opc_facet_1029 | OPC-10000-7 §4.2 | unimplemented | GDS AliasName Server Facet not implemented; GDS infrastructure is not planned. |
| opc_facet_1636 | OPC-10000-7 §4.2 | unimplemented | AliasName Server Facet not implemented; AliasName feature is deferred. |
| opc_facet_1637 | OPC-10000-7 §4.2 | unimplemented | AliasName Aggregating Server Facet not implemented; AliasName feature is deferred. |
| opc_facet_2242 | OPC-10000-7 §4.2 | unimplemented | LogObject Facet not implemented; no external log-object support planned. |
| opc_facet_2322 | OPC-10000-7 §4.2 | unimplemented | AliasName Configuration Facet not implemented; AliasName feature is deferred. |
| opc_facet_2323 | OPC-10000-7 §4.2 | unimplemented | AliasName Server PubSub Publisher Facet not implemented; AliasName+PubSub deferred. |
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
