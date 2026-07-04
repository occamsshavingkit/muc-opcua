# Quickstart: Audit Follow-Up

**Feature**: 030-audit-followup
**Date**: 2026-07-04

## Prerequisites

- CMake ≥ 3.13, GCC/Clang, OpenSSL dev headers
- Clean build from `main` post-029 merge

## Baseline

```bash
scripts/measure_size.sh all > /tmp/pre-030-size.txt
ctest --test-dir build --output-on-failure > /tmp/pre-030-ctest.txt
```

## Build and Test

```bash
# Configure
cmake -S . -B build -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host \
  -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON

# Build
cmake --build build -j$(nproc)

# Test
ctest --test-dir build --output-on-failure

# Verify nonce fail-closed (T1)
ctest --test-dir build -R test_dispatch_services

# Verify zeroize (T7)
valgrind --leak-check=full build/tests/unit/test_password_decrypt_zeroize

# Verify channel IDs unique (T17)
ctest --test-dir build -R test_secure_channel

# Verify cert check (T44)
ctest --test-dir build -R test_server_config

# Size check
scripts/measure_size.sh all
```

## Expected Results

- 100% tests pass (105 tests)
- `.text` within +3% of 029 baseline
- `.data` = 0, `.bss` = 0
- ARM nano/micro compile clean
