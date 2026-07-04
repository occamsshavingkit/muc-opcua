# Quickstart: Five-Lens Audit Findings Cleanup

**Feature**: 029-fix-audit-findings
**Date**: 2026-07-04

## Prerequisites

- CMake ≥ 3.13, GCC/Clang, OpenSSL dev headers (host crypto adapter)
- `scripts/measure_size.sh` for baseline comparison
- Existing `ctest` suite passing on `main`

## Baseline Capture

Before starting any fix, record the pre-change baseline:

```bash
scripts/measure_size.sh all > /tmp/pre-029-size.txt
ctest --test-dir build > /tmp/pre-029-ctest.txt
```

## Verification Per Story

### US1 — Critical Security Fixes (T1, T2)

```bash
# Build full profile with tests
make test-full

# Run the new test (T1: nonce fail-closed)
ctest --test-dir build -R nonce_fail_closed

# Verify password zeroization (T2): valgrind
valgrind --leak-check=full --track-origins=yes \
  build/tests/unit/test_activate_session_decrypt
```

### US2 — Correctness Fixes (T3, T8, T9, T10, T11, T27, T29)

```bash
# Build full profile with tests
make test-full

# Run all new correctness tests
ctest --test-dir build -R "browse_limit|delete_target_refs|session_create|write_validation|value_source_types|deadband_none|encoding_mask_invalid"

# Verify existing Browse tests still pass (T3 regression)
ctest --test-dir build -R "browse_service|browse_limits"
```

### US3 — Hot-Path Optimization (T4, T5, T6, T24)

```bash
# Build and measure size
scripts/measure_size.sh full

# Verify -- math.h should NOT appear in link map
arm-none-eabi-nm build/full/libmuc_opcua.a | grep -c 'fabs'  # should be 0

# Run subscription tests (identical wire output)
ctest --test-dir build -R "subscription|deadband_integer"

# Run hot-path benchmark
ctest --test-dir build -R "hotpath_benchmark"
```

### US4 — Tier 2 Hardening (T7, T12, T13, T14, T16, T17, T18)

```bash
# OAEP explicit params (T7): handshake still works
ctest --test-dir build -R "server_handshake_secure|secure_handshake_modern"

# Channel ID entropy (T17): verify channel_id != 1 after fix
ctest --test-dir build -R "secure_channel"

# mbedTLS PSS signature length (T18): negative test
ctest --test-dir build -R "pss_verify"
```

### US5 — Cleanup (T15-T42)

```bash
# Const-correctness (T26): verify tables in .rodata
arm-none-eabi-objdump -h build/embedded/libmuc_opcua.a | grep rodata

# All new tests green
ctest --test-dir build

# Size within gates
scripts/measure_size.sh all > /tmp/post-029-size.txt
diff /tmp/pre-029-size.txt /tmp/post-029-size.txt
```

## Global Gate Verification

```bash
# All four profiles + ASAN/UBSan
make test-nano && make test-micro && make test-embedded && make test-full
scripts/check_build_matrix.sh
scripts/measure_size.sh all

# Verify: .text ≤ +3%, data+bss ≤ +5%, no heap, all green
```

## Rollback

Each fix is independent; revert the offending file change and re-run `ctest`. The `math.h` removal (T5) is the only change that affects the link — all other changes are local to their source files.

## Pre-Change Baseline (captured 2026-07-04)

### Size (scripts/measure_size.sh all)
| profile | .text | .data | .bss |
|---------|-------|-------|------|
| nano | 16,436 | 0 | 0 |
| micro | 23,839 | 0 | 0 |
| embedded | 44,100 | 0 | 0 |
| full | 52,822 | 0 | 0 |

### Tests (ctest --test-dir build)
Pre-existing failures: test_user_auth_secure_e2e (Not Run - requires OpenSSL), test_event_notifications (known flaky). All other tests green.
