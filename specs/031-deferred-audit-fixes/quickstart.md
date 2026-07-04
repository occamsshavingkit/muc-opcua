# Quickstart: Deferred Audit Findings

**Feature**: 031-deferred-audit-fixes
**Date**: 2026-07-04

## How to Verify Each Fix

### US1 — Subscription hot-path (T4, T5/T24, T6)

```bash
# Verify <math.h> removed
grep -n 'math\.h' src/services/subscription.c
# Expected: no output

# Verify fabs removed
grep -n 'fabs' src/services/subscription.c
# Expected: no output

# Verify bitmap-based scan compiles
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target muc_opcua

# Run subscription tests
ctest -R subscription -V
# Expected: all pass

# Profile size check (nano/micro)
cmake --build . --target size_nano size_micro
# Expected: net ROM reduction ~12 KB
```

### US2 — Subscription correctness (T27, T31)

```bash
# Run deadband NONE test
ctest -R deadband -V
# Expected: passes; NONE deadband reports only on value change

# Run publish timer test
ctest -R publish_timer -V
# Expected: passes; loop terminates even with 0-interval

# Full subscription test sweep
ctest -R subscrip -V
# Expected: all pass, no regressions
```

### US3 — Service dispatch correctness/security (T8, T12, T13)

```bash
# Verify session ordering (T8): create session, force encode fail
ctest -R create_session -V

# Verify cert token ifdef (T12): compile with/without MUC_OPCUA_SECURITY
cmake -DMUC_OPCUA_SECURITY=OFF ..
cmake --build . --target muc_opcua
# Expected: compiles; cert token path excluded

# Verify nonce zeroize (T13): run with valgrind
valgrind --leak-check=full --track-origins=yes ctest -R activate
# Expected: no "uninitialized value" from nonce stack; no leaks

# Full service dispatch test sweep
ctest -R dispatch_services -V
# Expected: all pass
```

### US4 — Service dispatch performance (T23, T25)

```bash
# Verify dispatch lookup (T23): benchmark comparison count
ctest -R benchmark -V
# Expected: dispatch comparisons reduced from ~15 to ≤5 per request

# Verify profile URI cache (T25): audit profile parse count
# (manual: count calls to profile URI parser in GetEndpoints)
# Expected: parsed once per distinct URI, not N*endpoints

# Full service dispatch test sweep
ctest -R dispatch -V
# Expected: all pass
```

### US5 — Base nodes guard consolidation (T28)

```bash
# Verify single #ifdef block
grep -c '#if' src/address_space/base_nodes.c
# Expected: significant reduction from ~35 to 1-2 guards

# Compile all profiles
cmake --build . --target muc_opcua_nano muc_opcua_micro muc_opcua_embedded
# Expected: all compile; no linker errors

# Size check: no change
cmake --build . --target size_all
# Expected: identical ROM/RAM to before
```

### US6 — MISRA style fixes

```bash
# Verify braces in platform adapters
# Count single-statement bodies without braces (MISRA 15.6 pattern)
# Expected: 0 in host_crypto, mbedtls, wolfssl adapters

# Verify type fixes in subscription.c
grep -n 'size_t.*0u\|0u.*size_t' src/services/subscription.c
# Expected: no output

# Run full test suite to verify no behavioral changes
ctest -V
# Expected: 105/105 pass
```

### Full Regression

```bash
# Complete regression on host
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_SECURITY=ON ..
cmake --build .
ctest -V
# Expected: 105/105 pass

# ASAN + UBSan
cmake -DMUC_OPCUA_SANITIZE=ON ..
cmake --build .
ctest -V
# Expected: 105/105 pass, sanitizers clean

# All profiles compile
cmake --build . --target all_profiles
# Expected: nano, micro, embedded, full all compile
```
