# Contract: `features.h` defaults + dependency guards

**Finding**: 9 (custom profile silently half-wires features) · **Spec**: FR-012 ·
**Constitution**: §VI Toolchain Discipline.

## Location & inclusion

- New header `include/muc_opcua/features.h`.
- Included at the **top** of `include/muc_opcua/config.h` (which is already
  transitively included everywhere), so every translation unit — including direct
  `-D` consumers that bypass CMake — sees the defaults and guards.

## Part A — safe defaults

For every `MUC_OPCUA_*` service/feature gate, provide:

```c
#ifndef MUC_OPCUA_SERVICE_READ
#define MUC_OPCUA_SERVICE_READ 0     /* default OFF unless a profile/CMake enables it */
#endif
```

The default value MUST reproduce today's behavior for a consumer who sets nothing
(i.e. match what the absence of the macro currently implies at each `#if` site).
The exact default per gate is confirmed against the existing `#if` usage during
implementation so no preset's wired-service set changes.

## Part B — illegal-combination `#error`s

Emit a descriptive compile error for each unsatisfiable dependency. Initial
matrix (final set confirmed against existing gates):

```c
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD && !MUC_OPCUA_SUBSCRIPTIONS
#error "MUC_OPCUA_SUBSCRIPTIONS_STANDARD requires MUC_OPCUA_SUBSCRIPTIONS"
#endif
#if MUC_OPCUA_EVENTS && !MUC_OPCUA_SUBSCRIPTIONS
#error "MUC_OPCUA_EVENTS requires MUC_OPCUA_SUBSCRIPTIONS"
#endif
#if MUC_OPCUA_EMBEDDED_PROFILE && !MUC_OPCUA_SECURITY
#error "Embedded profile requires MUC_OPCUA_SECURITY"
#endif
/* ...plus any other dependency implied by the service #if gates... */
```

## Invariants

- All four shipped presets (Nano/Micro/Embedded/full) compile **unchanged** and
  wire the same services as before (regression: build all four, diff wired
  services / size figures).
- At least one deliberately-illegal custom combination fails to compile with the
  descriptive message (build-negative test).

## Tests

- Build matrix: each preset builds green (existing `check_build_matrix.sh`).
- Build-negative: a custom config enabling `SUBSCRIPTIONS_STANDARD` without
  `SUBSCRIPTIONS` MUST fail with the cited `#error` (new CI check / documented
  manual check).
