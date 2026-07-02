# Contract: Per-profile build + test execution

**Feature**: 028 · **FR**: FR-001 · **Constitution**: IV (Correctness Gates), VI (Toolchain).

## Build/run contract

| Invocation | Required result |
|-----------|-----------------|
| `cmake -S . -B build/<p> -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PROFILE=<p>` for `p ∈ {nano,micro,embedded,full}` | Configures with that profile's feature macros; the "force full" rule does NOT override an explicit profile. |
| `cmake --build build/<p>` | Builds the library + the tests that compile in that profile. |
| `ctest --test-dir build/<p>` | Runs the tests that survive that profile's `RUN_TEST` gates; passes. |
| `make test-nano \| test-micro \| test-embedded \| test-full` | Convenience wrappers for the above. |
| CI | A job per profile builds + `ctest`s; a profile that fails to build fails the job (no skip). |

## Gate-liveness proof (acceptance)

- A test gated to a feature macro (e.g. a SUBSCRIPTIONS-only test) MUST run in a
  profile that has the macro (micro+) and MUST be absent from a profile that
  doesn't (nano). Demonstrate by test count / presence differing across profiles.
- The default `-DMUC_OPCUA_BUILD_TESTS=ON` with no explicit profile still resolves
  to `full` (existing `make test` unchanged).

## Non-goals

- No per-profile ARM cross-run for behavior (host build suffices); existing ARM
  size jobs unchanged.
