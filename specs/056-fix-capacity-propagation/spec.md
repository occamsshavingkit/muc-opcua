# Spec 056: Capacity Resolution Redesign

**Status**: Implemented | **Branch**: `056-fix-capacity-propagation`
**Type**: Bug fix + refactor (build system / profile gating)

## Motivation

Per-profile capacity flags did not reliably reach the compiler. A capacity only
scaled if it appeared in **both** a profile block's `set(...)` in `CMakeLists.txt`
**and** the fan-out `foreach` in `src/CMakeLists.txt`; miss either and it silently
pinned to a `config.h` default for every profile — no error, no warning. Verified
at the compiler boundary (`flags.make`):

- `MU_MAX_CONNECTIONS` / `MU_MAX_SECURE_CHANNELS` reached **no** profile, so `full`
  advertised 100 sessions but only **4** transport connections; `standard` 50/4.
- `embedded`'s `MU_MAX_SESSIONS` was correct only by coincidence with the default.

Root causes were structural: capacities were defined in **≥4 places** (`config.h`
`#ifndef` defaults, `src/services/subscription.h`, the CMake presets + fan-out,
and `scripts/measure_size.sh`), and **no per-profile compile-time marker existed**
(`embedded`'s was CMake-only; `standard`/`full` collided on
`MUC_OPCUA_STANDARD_PROFILE`; `nano`/`micro` had none), so `config.h` could not
tell which profile it was compiling for.

## Design — a single three-stage cascade

`include/muc_opcua/capacities.h` is the single source of truth. For every capacity
it resolves an internal `MU_INTERN_*` macro through:

1. **DEFAULT** — an unconditional baseline (the `standard` values). Always runs, so
   the internal macro is guaranteed defined and silent fallthrough is impossible.
2. **PROFILE** — if a profile is declared, redefine to that profile's value.
   `src/CMakeLists.txt` emits exactly one `MUC_OPCUA_PROFILE_<NAME>` marker.
3. **USER** — if the integrator defined the public `MU_MAX_*` knob (e.g.
   `-DMU_MAX_SESSIONS=32`), redefine to it. The public knob wins, with or without
   a profile.

Precedence **default < profile < user**. **All code compiles off `MU_INTERN_*`.**
The public `MU_MAX_*` macros are pure input, read only in stage 3 — they remain
the documented `-D` override knob. Adding or retuning a capacity is a one-line
edit in one file; there is no second list to keep in sync.

### Per-profile capacity values

Connections follow the rule **connections ≥ sessions**: a profile advertising N
concurrent sessions must accept N independent SecureChannels, because two
independent clients each hold their own channel (`profile-micro.md`). `micro`
therefore enables `MUC_OPCUA_MULTIPLE_CONNECTIONS` and gets 2 connections; `nano`
stays single-connection (its profile is single-client).

| cap | baseline | nano | micro | embedded | standard | full |
|-----|---------:|-----:|------:|---------:|---------:|-----:|
| MAX_SESSIONS | 50 | 2 | 2 | 2 | 50 | 100 |
| MAX_CONNECTIONS | 50 | 1 | 2 | 4 | 50 | 100 |
| MAX_SUBSCRIPTIONS | 50 | 2 | 2 | 2 | 50 | 100 |
| MAX_MONITORED_ITEMS | 1000 | 8 | 8 | 100 | 1000 | 2000 |
| MAX_PUBLISH_REQUESTS | 50 | 4 | 4 | 5 | 50 | 100 |
| MONITORED_QUEUE_DEPTH | 2 | 1 | 1 | 2 | 2 | 2 |
| MAX_TRIGGER_LINKS | 4 | 4 | 4 | 4 | 4 | 4 |

`MU_INTERN_MAX_SECURE_CHANNELS` tracks `MU_INTERN_MAX_CONNECTIONS` 1:1.
Profile-invariant capacities (address-space, dynamic-node, conditions, query
continuation points) go through stages 1+3 only. Baseline == standard, so the
`standard` profile needs no stage-2 block.

## Requirements

- **FR-001**: `capacities.h` resolves every capacity via default<profile<user;
  `MU_INTERN_*` is always defined.
- **FR-002**: `src/CMakeLists.txt` emits one `MUC_OPCUA_PROFILE_<NAME>` marker
  (nano/micro/embedded/standard/full); `custom` emits none. Unknown profile is a
  configure error.
- **FR-003**: All library/app/test code compiles off `MU_INTERN_*`. Public
  `MU_MAX_*` is read only in the cascade and stays the documented `-D` knob.
- **FR-004**: A public `-DMU_MAX_*=n` overrides the profile preset (stage 3).
- **FR-005**: `micro` enables `MUC_OPCUA_MULTIPLE_CONNECTIONS` (2 connections).
- **FR-006**: `tests/unit/test_capacity_propagation.c` asserts, per profile, the
  exact `MU_INTERN_*` values, the `channels == connections` invariant, and
  `connections ≥ sessions` for multi-connection builds; plus a stage-3 override
  guard.

## Verification

- Per-profile ctest matrix green: nano 89 / micro 97 / embedded 117 / standard
  125 / full 125.
- Preprocessor probe confirms: no-profile → standard baseline; each profile →
  its column; `-DMU_MAX_CONNECTIONS=7` on `standard` → 7 (and channels → 7);
  `-DMU_MAX_SESSIONS=9` with no profile → 9 over the baseline.

## Size impact

Code (`.text`, ARM `-Os`) is flat — capacity *values* add no code: nano 17790 (=),
embedded 53675 (=), standard 67353 (=), full 67373 (=); **micro +620 B** for the
multi-connection accept path it now requires. The cost is caller RAM
(`MU_SERVER_STORAGE_BYTES`), from the connection pool (conns × 9520 B):

| profile | storage before | storage after | Δ |
|---------|---------------:|--------------:|--:|
| nano | 1,408 | 1,408 | 0 |
| micro | 11,680 | 30,720 | +19,040 |
| embedded | 127,272 | 127,272 | 0 |
| standard | 741,300 | 1,178,364 | +437,064 |
| full | 1,387,500 | 2,300,564 | +913,064 |

The standard/full growth is the direct consequence of connections=sessions and is
tunable in one place (the connection column of `capacities.h`, or per-build with
`-DMU_MAX_CONNECTIONS=n`).

## Files Changed

| File | Change |
|------|--------|
| `include/muc_opcua/capacities.h` | NEW — the three-stage cascade / single source of truth |
| `include/muc_opcua/config.h` | Include capacities.h; drop `MU_MAX_*` capacity defaults; storage math → `MU_INTERN_*` |
| `src/services/subscription.h` | Drop the 5 duplicate cap definers; consume `MU_INTERN_*` |
| `CMakeLists.txt` | Profile blocks set only features; `micro` gains multi-connection; capacity `set()`s removed |
| `src/CMakeLists.txt` | Emit `MUC_OPCUA_PROFILE_<NAME>`; remove capacity fan-out |
| `scripts/measure_size.sh` | Profile is the only capacity input |
| ~40 code files | `MU_MAX_* → MU_INTERN_MAX_*` (src/include/tests/examples/platform) |
| `tests/unit/test_capacity_propagation.c` | Rewritten around `MU_INTERN_*` + profile markers |
| `docs/traceability/files-to-sections.md` | Register `capacities.h` |
| `docs/size/feature-size-ledger.md`, `README.md`, `docs/api-reference.md` | Document the change |
