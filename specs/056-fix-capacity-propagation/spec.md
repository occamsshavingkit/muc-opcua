# Spec 056: Fix Capacity Flag Propagation

**Status**: Draft | **Branch**: `056-fix-capacity-propagation`
**Type**: Bug fix (build system) — profile gating

## Motivation

A capacity macro only scales per-profile if it appears in **both**:

1. the profile block's `set(<MACRO> <val> CACHE STRING ... FORCE)` in `CMakeLists.txt`, and
2. the propagation `foreach` in `src/CMakeLists.txt:264-280`.

Anything missing from either list is silently pinned to its `config.h` `#ifndef`
default for **every** profile — no error, no warning. This was verified at the
compiler boundary (`flags.make`), not inferred.

### Confirmed defects

| # | Defect | Evidence |
|---|--------|----------|
| 1 | `MU_MAX_CONNECTIONS` / `MU_MAX_SECURE_CHANNELS` are in **neither** list. Frozen at the `config.h` default (4 when `MUC_OPCUA_MULTIPLE_CONNECTIONS`, else 1) for every profile. `full` advertises **100 sessions but only 4 transport connections**; `standard` 50 sessions / 4 connections. | `full`/`standard`/`embedded` `flags.make` contain no `MU_MAX_CONNECTIONS`; `config.h:82-101` |
| 3 | `embedded` never `set()`s `MU_MAX_SESSIONS`, so it falls through to the `config.h` default `2`. Correct **only by coincidence** that the default equals embedded's intended value — a silent-fallthrough trap for any future default change. | `embedded` `flags.make` has no `MU_MAX_SESSIONS`; `CMakeLists.txt:110-118` |

(Defect "2" — the `MU_MAX_ADDRESS_SPACE_NODES` / `MU_MAX_DYNAMIC_*` family being
frozen — is **out of scope**: those are intentionally fixed-by-default today and
scaling them is a separate design question. See Out of Scope.)

## Requirements

### FR-001: Propagate connection capacity per profile

`MU_MAX_CONNECTIONS` is added to the `src/CMakeLists.txt` propagation `foreach`
and set explicitly in each profile block. `MU_MAX_SECURE_CHANNELS` is **not**
propagated separately — `config.h` derives it as `MU_MAX_SECURE_CHANNELS =
MU_MAX_CONNECTIONS`, preserving the documented 1:1 invariant with a single knob.

Per-profile connection counts:

| Profile | connections (= secure channels) | sessions | rationale |
|---------|--------------------------------|----------|-----------|
| nano | 1 | 2 | single transport, no multi-connect |
| micro | 1 | 2 | unchanged |
| embedded | 4 | 2 | unchanged (multi-connect default) |
| standard | 8 | 50 | scaled: ~6:1 sessions:channel |
| full | 16 | 100 | scaled: ~6:1 sessions:channel |

Connections are transport/secure-channel level; clients multiplex multiple
sessions over one channel, so connections scale sub-linearly with sessions.
Each connection costs ≈ one `MU_MIN_CHUNK_SIZE` receive buffer, so the count is
kept conservative to bound per-profile storage.

### FR-002: Eliminate silent default-fallthrough (robustness)

Every profile block explicitly `set()`s every capacity it depends on — including
`embedded`'s `MU_MAX_SESSIONS=2` — so no profile relies on a `config.h` default
matching its intent by coincidence. nano remains the one documented
"defaults apply" profile.

### FR-003: Compile-time regression guard

`tests/unit/test_capacity_propagation.c` static-asserts, using the profile marker
macros that already reach the build:

- `MU_MAX_SECURE_CHANNELS == MU_MAX_CONNECTIONS` (invariant, all profiles)
- Standard-and-up (`MUC_OPCUA_STANDARD_PROFILE`): `MU_MAX_CONNECTIONS >= 8`
  (fails today at 4 — this is the failing test that proves defect 1)
- Embedded (`MUC_OPCUA_EMBEDDED_PROFILE` && !standard): `MU_MAX_CONNECTIONS >= 4`

The test compiles into the existing per-profile ctest matrix, so a future
propagation gap breaks the build for the affected profile.

## Out of Scope

- Scaling `MU_MAX_ADDRESS_SPACE_NODES` / `MU_MAX_DYNAMIC_*` per profile (separate
  design decision; they are intentionally fixed today).
- Changing session/subscription/monitored-item counts (already propagate correctly).
- Runtime connection-pool behavior changes.

## Files Changed

| File | Change |
|------|--------|
| `CMakeLists.txt` | Each profile block sets `MU_MAX_CONNECTIONS` (+ `embedded` sets `MU_MAX_SESSIONS=2`) |
| `src/CMakeLists.txt` | Add `MU_MAX_CONNECTIONS` to the capacity `foreach` |
| `tests/unit/test_capacity_propagation.c` | NEW — static-assert regression guard |
| `tests/CMakeLists.txt` | Register the new test |
| `README.md` / size ledger | Document per-profile connection counts |

## Size Impact

`standard` +4 connections, `full` +12 connections vs today's 4. Per-connection
storage ≈ `MU_MIN_CHUNK_SIZE` (8192 B) receive buffer plus `sizeof(mu_connection_t)`
bookkeeping. Measured empirically before/after via `MU_SERVER_STORAGE_BYTES` and
recorded in the size ledger. nano/micro/embedded storage unchanged (0 delta).
