# Quickstart: Implement Deferred Features (D1-D4)

**Feature**: 045-implement-deferred-features
**Date**: 2026-07-06

## Prerequisites

```bash
cd /path/to/micro-opcua
cmake -S . -B build -DMUC_OPCUA_PROFILE=standard
cd build && cmake --build . && ctest --output-on-failure
```

All 4 build profiles must pass before starting. Fix any pre-existing failures.

## CMake Gate Fix (Prerequisite for D1, D2, D3)

Edit `src/CMakeLists.txt` to add missing `target_compile_definitions` for:
- `MUC_OPCUA_COMPLEX_TYPES`
- `MUC_OPCUA_AUDITING`
- `MUC_OPCUA_AGGREGATE_FULL` (already may exist)
- Other profile-gated features that are missing

Pattern:
```cmake
if(MUC_OPCUA_COMPLEX_TYPES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_COMPLEX_TYPES=1)
endif()
```

## Implementation Order

### 1. CMake Gate Fix (shared prerequisite)
### 2. D2 — Audit Events (simplest, most self-contained)
### 3. D1 — Complex Types (depends on CMake fix, larger)
### 4. D3 — Aggregate Functions (largest, depends on CMake fix)
### 5. D4 — Binary Size Measurement (independent script change)

## D2 Quickstart: Audit Callbacks

```
1. Add MUC_OPCUA_AUDITING target_compile_definitions to src/CMakeLists.txt
2. Add mu_audit_callback_t typedef to include/muc_opcua/services/audit.h
3. Add MU_MAX_AUDIT_CALLBACKS + callback array to src/core/server_internal.h
4. Declare mu_server_set/add_audit_callback in include/muc_opcua/server.h
5. Replace no-op body in src/services/audit_events.c:
   - mu_raise_audit_event: NULL-check, auditing_enabled check, iterate callbacks
   - mu_server_set_audit_callback: clear array, set callback[0]
   - mu_server_add_audit_callback: append to array
6. Add tests to tests/unit/test_audit_events.c:
   - Callback receives event with populated fields
   - Multiple callbacks fire in registration order
   - NULL safety (no crash)
   - Auditing disabled → no dispatch

Verify: ctest -R test_audit_events
```

## D1 Quickstart: Complex Types

```
1. Add MUC_OPCUA_COMPLEX_TYPES target_compile_definitions to src/CMakeLists.txt
2. Implement mu_binary_encode_struct in src/encoding/binary_complex.c:
   - Iterate fields in def->fields order
   - Dispatch scalar encode by field data_type
   - Handle optional fields (EncodingMask)
   - Handle arrays (length prefix + elements)
   - Recurse for nested structures
3. Implement mu_binary_decode_struct:
   - Same iteration pattern, dispatch to mu_binary_read_*
   - Validate truncation after each field
4. Implement mu_binary_encode/decode_enum (Int32 wrapper)
5. Add mu_find_structure/enum_definition lookup to src/address_space/complex_types.c
6. Add tests to tests/unit/test_complex_types.c:
   - Round-trip scalar fields (≥8 OPC UA built-in types)
   - Round-trip optional fields with EncodingMask
   - Round-trip nested structures
   - Round-trip arrays
   - Empty structure (zero fields)
   - Malformed input → Bad_DecodingError

Verify: ctest -R test_complex_types
```

## D3 Quickstart: Aggregate Functions

```
1. Add 39 aggregate type ID constants to include/muc_opcua/opcua_ids.h
2. Expand accumulator union in src/services/subscription.h
3. Add accumulate branches in src/services/subscription_aggregate.c:
   - Count: ++accumulator.count.value
   - DurationGood/Bad/InStateZero: track start time + running total
   - PercentGood/Bad: count by status code
   - Range: track min + max
   - Start: capture first value
   - End: capture last value (overwrite)
   - Delta: capture first + last
   - TimeAverage: weighted sum by time delta
   - Total/Total2: running sum
   - WorstQuality/WorstQuality2: track worst status
   - Interpolative: capture prev value+time, compute interpolated
   - AnnotationCount: ++count
4. Add publish branches for each type
5. Expand filter_reader.c whitelist for MUC_OPCUA_AGGREGATE_FULL
6. Add tests to tests/unit/test_aggregate_full.c:
   - ≥1 test per aggregate function with known inputs/outputs
   - Zero-sample fallback
   - Non-numeric skip
   - Processing interval boundary

Verify: ctest -R test_aggregate_full && ctest -R test_aggregate
```

## D4 Quickstart: Size Measurement

```
1. Create scripts/size_measure.ld (minimal Cortex-M0+ linker script)
2. Create scripts/size_measure_startup.c (minimal vector table + startup)
3. Modify scripts/measure_size.sh:
   a. Add ELF linking step after archive build (using size_measure.ld + startup)
   b. Add JSON output file
   c. Add --lto flag for LTO comparison
   d. Change toolchain absence from exit 127 to warning + graceful skip
4. Run, verify output, update docs/size/

Verify: bash scripts/measure_size.sh standard --lto
```

## Final Verification

```bash
# Build all profiles
for profile in nano micro embedded standard; do
    cmake -S . -B build_$profile -DMUC_OPCUA_PROFILE=$profile
    cmake --build build_$profile
done

# Run tests on all profiles
for profile in nano micro embedded standard; do
    cmake --build build_$profile
    ctest --test-dir build_$profile --output-on-failure
done

# Verify no STUB markers remain (should already be clean from spec 043/044)
grep -rn "STUB:" tests/

# Run size measurement
bash scripts/measure_size.sh all
```
