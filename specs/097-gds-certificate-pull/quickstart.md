# Validation Quickstart: GDS Certificate Pull Management

## Prerequisites

- Host toolchain: gcc/clang, cmake ≥ 3.15, make/ninja
- Unity test framework (pulled via FetchContent)

## Build & Test

```bash
# Full profile + certificate manager + tests
cmake -B b_cm -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build b_cm -j$(nproc)
ctest --test-dir b_cm -R test_certificate_manager --output-on-failure

# Nano compile-out verification
cmake -B b_cm_nano -DMUC_OPCUA_PROFILE=nano -DMUC_OPCUA_BUILD_TESTS=OFF
cmake --build b_cm_nano -j$(nproc)
# Verify: nm b_cm_nano/lib/libmuc_opcua.a | grep -i cert_manager → empty

# Size report
cmake -B b_cm_full -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=OFF
cmake --build b_cm_full -j$(nproc)
size b_cm_full/lib/libmuc_opcua.a | grep cert_manager
```

## BrowseName Length Check

```bash
python3 scripts/check_browsename_lengths.py
```

## Expected Outcomes

1. `test_certificate_manager` suite passes with 8-10 tests
2. Nano build produces zero Certificate Manager symbols
3. BrowseName length constants match declared array sizes
4. Full-profile .text contribution ≤3.0 KB

## Run Single Test

```bash
./b_cm/tests/unit/test_certificate_manager
```
