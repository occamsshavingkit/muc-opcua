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

| Profile | What it targets | Roughly |
|---|---|---|
| `nano` | Nano Embedded Device 2017 Server Profile | Read/Browse/discovery only, no heap, no security |
| `micro` | Micro Embedded Device 2017 Server Profile | + data-change subscriptions, multiple sessions/connections |
| `embedded` | Embedded 2017 UA Server Profile | + Security (Basic256Sha256), base node set, Standard DataChange additions |
| `standard` | Standard UA Server 2017 | Embedded-level feature surface plus the Standard profile marker and standard capacity minima |
| `full` | Not a distinct OPC UA profile | Standard profile/capacity family plus optional services/facets (History, Query, NodeManagement, PubSub, Data Access, Method Server, Auditing, Complex Types, Redundancy, full Aggregates, Reverse Connect, ECC) |
| `custom` | You hand-pick every flag | Nothing is preset beyond the always-on core services (Read/Browse/Discovery); every feature stays OFF unless you `-D` it |

Each named profile (everything except `custom`) is a
`configs/<profile>.defconfig` selecting the profile choice; the resolved feature
set falls out of the per-child `default y if PROFILE_*` presets in `/Kconfig`.
Profiles are additive through `standard`; `full` then enables the optional
services/facets used for integration and development coverage. Any profile can
be further adjusted by the override mechanism above.

**Capacities are separate from feature gating** and are not covered by this
doc: session/connection/subscription/array-length limits resolve through a
default → profile → user cascade in `include/muc_opcua/capacities.h`, keyed
off the `MUC_OPCUA_PROFILE_{MICRO,EMBEDDED,STANDARD,FULL}` choice symbols
(the Kconfig profile tier, still emitted as compile defs).
Override any capacity directly with `-DMU_MAX_*=<n>` regardless of profile.
See [docs/conformance/documentation.md](conformance/documentation.md) for the
per-profile capacity table.

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

| Output | Used by | Purpose |
|---|---|---|
| `muc_opcua_config.cmake` | This repository's CMake build | `set(MUC_OPCUA_<SYM> ON/OFF)` values consumed by `src/CMakeLists.txt` for source selection. |
| `muc_opcua_autoconf.h` | Non-CMake / external consumers | `#define MUC_OPCUA_<SYM> 1` for enabled symbols; disabled symbols are left undefined, which matches this codebase's `#ifdef` and `#if` feature tests. |

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
cat > build/kconfig/standard-no-crypto.fragment <<'EOF'
# MUC_OPCUA_SECURITY is not set
# MUC_OPCUA_ECC is not set
EOF

python3 scripts/kconfig/gen_config.py \
    Kconfig \
    configs/standard.defconfig \
    build/kconfig/muc_opcua_config.cmake \
    build/kconfig/muc_opcua_autoconf.h \
    build/kconfig/standard-no-crypto.fragment
```

For a downstream build that does not call this repository's CMake, force-include
the generated header before any `muc_opcua` header or source includes
`muc_opcua/config.h`:

```sh
cc -Iinclude -include build/kconfig/muc_opcua_autoconf.h ...
```

The generated header is not included automatically by `muc_opcua/config.h` because
many embedded build systems have their own configuration include convention. The
contract is simple: enabled feature macros must be defined to `1`; disabled
feature macros must be undefined. `include/muc_opcua/features.h` remains the
compiler backstop for illegal hand-written combinations.

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

### Feature / facet flags (in the Kconfig tree — support the override mechanism above)

Membership below is the resolved `default y if PROFILE_*` matrix from `/Kconfig`
(regenerate a temporary config with `python3 scripts/kconfig/gen_config.py Kconfig configs/<p>.defconfig /tmp/muc_opcua_config.cmake /tmp/muc_opcua_autoconf.h`).
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
