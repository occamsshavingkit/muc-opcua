# Quickstart: Split Monolithic Source Files

**Feature**: 032-split-source-modules
**Date**: 2026-07-04

## Verify Before Starting

```bash
# Capture pre-split size baseline
BUILD_ROOT=build/size-032-baseline scripts/measure_size.sh all

# Confirm all tests pass before touching files
ctest -V
# Expected: 108/108 pass
```

## How to Verify After Each Module

### After subscription.c split (US1)

```bash
# Build and test
cmake --build build -j$(nproc)
ctest -V
# Expected: 108/108 pass

# Size check: should match baseline within link-order noise
BUILD_ROOT=build/size-032-us1 scripts/measure_size.sh all
# Compare with build/size-032-baseline

# Verify subscription.c is deleted
ls src/services/subscription.c && echo "FAIL: still exists" || echo "OK: removed"

# Verify new files exist
ls src/services/subscription_monitor.c src/services/subscription_publish.c src/services/subscription_aggregate.c
```

### After service_dispatch.c split (US2)

```bash
cmake --build build -j$(nproc)
ctest -V
# Expected: 108/108 pass

BUILD_ROOT=build/size-032-us2 scripts/measure_size.sh all

# Verify service_dispatch.c is under 1000 lines
wc -l src/core/service_dispatch.c
# Expected: <1000

# Verify new dispatch files exist
ls src/core/dispatch_session.c src/core/dispatch_discovery.c \
   src/core/dispatch_attribute.c src/core/dispatch_view.c \
   src/core/dispatch_subscription.c src/core/dispatch_node_mgmt.c \
   src/core/dispatch_method.c
```

### Full regression

```bash
# All profiles
cmake --build build -j$(nproc) && ctest -V

# ASAN + UBSan
cmake -B build-san -DMUC_OPCUA_SANITIZE=ON -DMUC_OPCUA_BUILD_TESTS=ON && \
  cmake --build build-san -j$(nproc) && ctest --test-dir build-san

# ARM size matrix
BUILD_ROOT=build/size-032-final scripts/measure_size.sh all

# clang-format
clang-format --dry-run src/services/subscription_*.c src/core/dispatch_*.c src/core/service_dispatch.c
```
