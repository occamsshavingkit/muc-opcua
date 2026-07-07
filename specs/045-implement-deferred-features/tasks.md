# Tasks: Implement Deferred Features (D1-D4)

**Feature**: 045-implement-deferred-features
**Input**: Design documents from `specs/045-implement-deferred-features/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Mandatory for protocol code (D1 encode/decode, D2 dispatch, D3 accumulate/publish).
D4 (scripting) has no tests.

**Organization**: Tasks grouped by user story to enable independent implementation
and testing. Protocol tests and fixtures appear before the implementation tasks they
validate.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4)
- Include exact file paths in descriptions
- Include OPC UA part/section references in protocol task descriptions

## Path Conventions

- **Public API**: `include/muc_opcua/`
- **Core protocol**: `src/core/`, `src/encoding/`, `src/services/`
- **Address space**: `src/address_space/`
- **Security interfaces**: `src/security/`
- **Platform adapters**: `platform/pico/`, `platform/arduino/`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/fixtures/`
- **Scripts**: `scripts/`

---

## Phase 1: Setup (Baseline Verification)

**Purpose**: Verify current CI state before any changes

- [ ] T001 Verify baseline — `ctest --output-on-failure` passes on full profile in `build/`
- [ ] T002 [P] Verify baseline — `ctest --output-on-failure` passes on micro profile in `build_micro/`
- [ ] T003 [P] Verify baseline — `ctest --output-on-failure` passes on standard profile in `build_standard/`
- [ ] T004 [P] Catalog current `#if MUC_OPCUA_COMPLEX_TYPES`, `#if MUC_OPCUA_AUDITING`, `#if MUC_OPCUA_AGGREGATE_FULL` usage with `grep -rn "MUC_OPCUA_COMPLEX_TYPES\|MUC_OPCUA_AUDITING\|MUC_OPCUA_AGGREGATE_FULL" src/ include/`

---

## Phase 2: Foundational — Fix CMake Compile Definition Gates

**Purpose**: Fix missing `target_compile_definitions` so feature flags actually compile
into C code. This is a blocking prerequisite for D1, D2, and D3.

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [ ] T005 Audit `src/CMakeLists.txt` for all `MUC_OPCUA_*` options that lack `target_compile_definitions` — grep for `option(MUC_OPCUA_*` in `CMakeLists.txt` and verify each has a corresponding `target_compile_definitions` block in `src/CMakeLists.txt`
- [ ] T006 Add `target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_COMPLEX_TYPES=1)` gated on `if(MUC_OPCUA_COMPLEX_TYPES)` in `src/CMakeLists.txt`
- [ ] T007 [P] Add `target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_AUDITING=1)` gated on `if(MUC_OPCUA_AUDITING)` in `src/CMakeLists.txt`
- [ ] T008 [P] Add `target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_AGGREGATE_FULL=1)` gated on `if(MUC_OPCUA_AGGREGATE_FULL)` in `src/CMakeLists.txt` (if missing)
- [ ] T009 Verify all profiles build and tests pass after CMake fixes: `for p in nano micro embedded standard; do cmake -S . -B build_$p -DMUC_OPCUA_PROFILE=$p && cmake --build build_$p && ctest --test-dir build_$p --output-on-failure; done`

**Checkpoint**: CMake gates working — feature flags actually compile into C code.

---

## Phase 3: User Story 2 — Audit Event Callback Dispatch (Priority: P2)

**Goal**: Implement `mu_audit_callback_t` typedef, callback registration API
(`mu_server_set_audit_callback`, `mu_server_add_audit_callback`), and dispatch from
`mu_raise_audit_event` (OPC-10000-5 §6.5.2, §6.5.3).

**Independent Test**: `ctest -R test_audit_events` — callback receives event with
correct fields, multiple callbacks fire in order, NULL-safety, auditing-disabled gate.

### Tests for User Story 2

- [ ] T010 [P] [US2] Add callback-dispatch round-trip test in `tests/unit/test_audit_events.c`: register a callback, call `mu_raise_audit_event` with a valid event, verify callback received the event with populated `action_timestamp`, `server_id`, `client_audit_entry_id`, and correct `event_type`-specific union fields (OPC-10000-5 §6.5.3)
- [ ] T011 [P] [US2] Add multiple-callback ordering test in `tests/unit/test_audit_events.c`: register 3 callbacks via `set` + `add` + `add`, verify all 3 fire in registration order
- [ ] T012 [P] [US2] Add auditing-disabled gate test in `tests/unit/test_audit_events.c`: register callback, set `auditing_enabled=false`, raise event, verify callback NOT invoked (OPC-10000-5 §6.5)
- [ ] T013 [P] [US2] Add `mu_server_add_audit_callback` overflow test in `tests/unit/test_audit_events.c`: register ≥5 callbacks, verify 5th returns `MU_STATUS_BAD_OUTOFMEMORY`
- [ ] T014 [US2] Verify new tests FAIL against current no-op `mu_raise_audit_event` (test-first gate)

### Implementation for User Story 2

- [ ] T015 [US2] Add `mu_audit_callback_t` typedef in `include/muc_opcua/services/audit.h`: `typedef void (*mu_audit_callback_t)(struct mu_server *, const mu_audit_event_t *, void *)`
- [ ] T016 [US2] Add `#define MU_MAX_AUDIT_CALLBACKS 4` and callback storage array (`audit_callbacks[MU_MAX_AUDIT_CALLBACKS]` with `callback` + `context` + `audit_callback_count`) in `struct mu_server` in `src/core/server_internal.h`, gated on `#if MUC_OPCUA_AUDITING` (follow pattern from `registered_methods`)
- [ ] T017 [US2] Declare `mu_server_set_audit_callback` and `mu_server_add_audit_callback` in `include/muc_opcua/server.h`, gated on `#ifdef MUC_OPCUA_AUDITING`
- [ ] T018 [US2] Implement `mu_server_set_audit_callback`: clear `audit_callback_count=0`, if `callback != NULL` register at position 0, in `src/services/audit_events.c`
- [ ] T019 [US2] Implement `mu_server_add_audit_callback`: NULL-check → count check → append → return `MU_STATUS_GOOD`/`MU_STATUS_BAD_OUTOFMEMORY`/`MU_STATUS_BAD_ARGUMENTSMISSING`, in `src/services/audit_events.c`
- [ ] T020 [US2] Replace `mu_raise_audit_event` no-op body in `src/services/audit_events.c` with: NULL-check (server+event) → `auditing_enabled` check → populate `action_timestamp` → iterate `audit_callbacks[]` → call each callback
- [ ] T021 [US2] Populate `action_timestamp` in `mu_raise_audit_event` using `mu_time_now_ms()` (if available via server) or 0 as timestamp placeholder
- [ ] T022 [US2] Run `ctest -R test_audit_events` on full and standard profiles — all tests pass, including existing NULL-safety test (`test_raise_audit_event_null_safety` from spec 043) and valid-input no-crash test (`test_raise_audit_event_valid_input`)

**Checkpoint**: Audit callback dispatch independently testable. Existing NULL-safety
and constant tests still pass.

---

## Phase 4: User Story 1 — Complex Type Binary Encode/Decode (Priority: P1)

**Goal**: Implement `mu_binary_encode_struct`, `mu_binary_decode_struct`,
`mu_binary_encode_enum`, `mu_binary_decode_enum` in `src/encoding/binary_complex.c`,
plus type lookup helpers. (OPC-10000-6 §5.2.2.12, §5.2.2.9, §5.4.1)

**Independent Test**: `ctest -R test_complex_types` — round-trip encode→decode→deep-equal
for scalar fields, optional fields, nested structures, arrays, empty structures,
malformed input.

### Tests for User Story 1

- [ ] T023 [P] [US1] Add round-trip test for scalar fields in `tests/unit/test_complex_types.c`: register a structure with ≥8 built-in scalar types (Boolean, SByte, Byte, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float, Double, String) as required fields, encode, decode, verify all field values preserved (OPC-10000-6 §5.2.2.12)
- [ ] T024 [P] [US1] Add round-trip test for optional fields with EncodingMask in `tests/unit/test_complex_types.c`: register structure with `structure_type=MU_STRUCTURE_TYPE_OPTIONAL`, set some optional fields, encode, decode, verify only fields with set EncodingMask bits are present (OPC-10000-6 §5.2.2.12)
- [ ] T025 [P] [US1] Add round-trip test for nested structures in `tests/unit/test_complex_types.c`: register outer + inner structures, encode with populated nested field, decode, verify all nesting levels preserved (OPC-10000-6 §5.4.1 — structure body = field concatenation including nested structures)
- [ ] T026 [P] [US1] Add round-trip test for array fields in `tests/unit/test_complex_types.c`: register structure with Int32 array field (`value_rank=1`), encode [1, 2, 3], decode, verify array length and values preserved (OPC-10000-6 §5.2.2.1 — array encoding length prefix + elements)
- [ ] T027 [P] [US1] Add empty-structure round-trip test in `tests/unit/test_complex_types.c`: register structure with `field_count=0`, encode zero-field body, decode, verify success with empty output (OPC-10000-6 §5.2.2.12)
- [ ] T028 [P] [US1] Add malformed-input tests in `tests/unit/test_complex_types.c`: truncated data → `MU_STATUS_BAD_DECODINGERROR`, encoding mask bit set but field absent → `MU_STATUS_BAD_DECODINGERROR` (OPC-10000-6 §5.4.1 — decoder validation)
- [ ] T029 [US1] Verify all new complex type tests FAIL against dead stub (test-first gate)

### Implementation for User Story 1

- [ ] T030 [US1] Add `mu_binary_encode_struct` declaration in `include/muc_opcua/encoding.h`, gated on `#if MUC_OPCUA_COMPLEX_TYPES`
- [ ] T031 [US1] Add `mu_binary_decode_struct` declaration in `include/muc_opcua/encoding.h`
- [ ] T032 [US1] Add `mu_find_structure_definition` and `mu_find_enum_definition` declarations in `include/muc_opcua/address_space/complex_types.h`
- [ ] T033 [US1] Implement `mu_find_structure_definition` in `src/address_space/complex_types.c`: linear scan of `server->complex_types.structures[]` matching `type_id` NodeId, return definition pointer or NULL
- [ ] T034 [US1] Implement `mu_find_enum_definition` in `src/address_space/complex_types.c`: same pattern for `server->complex_types.enums[]`
- [ ] T035 [US1] Implement `mu_binary_encode_struct` in `src/encoding/binary_complex.c`: iterate `def->fields`, for each field dispatch to `mu_binary_write_{type}` based on field `data_type` NodeId, handle optional fields via EncodingMask, handle arrays with length prefix, recurse for nested structures (OPC-10000-6 §5.4.1)
- [ ] T036 [US1] Implement `mu_binary_decode_struct` in `src/encoding/binary_complex.c`: same iteration pattern using `mu_binary_read_{type}`, validate truncation after each field, return `MU_STATUS_BAD_DECODINGERROR` on malformed input (OPC-10000-6 §5.4.1)
- [ ] T037 [US1] Implement `mu_binary_encode_enum` in `src/encoding/binary_complex.c`: wrap `mu_binary_write_int32` (OPC-10000-6 §5.2.2.9)
- [ ] T038 [US1] Implement `mu_binary_decode_enum` in `src/encoding/binary_complex.c`: wrap `mu_binary_read_int32` (OPC-10000-6 §5.2.2.9)
- [ ] T039 [US1] Add field-type dispatch table mapping from OPC UA built-in type NodeIds to `mu_binary_write_*`/`mu_binary_read_*` functions, used by encode_struct/decode_struct
- [ ] T040 [US1] Remove dead stub `muc_opcua_complex_types_placeholder()` from `src/encoding/binary_complex.c`
- [ ] T041 [US1] Run `ctest -R test_complex_types` on full and standard profiles — all tests pass, existing registration tests still pass

**Checkpoint**: Complex type encode/decode independently testable with round-trip
verification for scalar, optional, nested, array, and malformed-input cases.

---

## Phase 5: User Story 3 — Remaining Aggregate Functions (Priority: P3)

**Goal**: Implement 21 new primary aggregate functions from OPC-10000-13 (2 functions — MaximumActualTime/MinimumActualTime — are already implemented via the existing Maximum/Minimum with IDs 2347/2346), expand
accumulator union, add accumulate/publish branches, expand filter reader whitelist
when `MUC_OPCUA_AGGREGATE_FULL` is ON.

**Independent Test**: `ctest -R test_aggregate_full` — each implemented aggregate
function has ≥1 test with known-input/expected-output vectors. Existing
aggregate tests (Average, Minimum, Maximum) still pass.

### Tests for User Story 3

- [ ] T042 [P] [US3] Add Count aggregate test in `tests/unit/test_aggregate_full.c`: accumulate 5 numeric samples, publish, verify Count = 5 and type is Int64 (OPC-10000-13 §4.2.2.3)
- [ ] T043 [P] [US3] Add Range aggregate test in `tests/unit/test_aggregate_full.c`: accumulate [1.0, 5.0, 3.0], publish, verify Range = 4.0 (max − min) (OPC-10000-13 §4.2.2.11)
- [ ] T044 [P] [US3] Add DurationGood aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples with Good/Bad/Good statuses at t=0, t=10, t=30, publish at t=100, verify DurationGood = 80ms (0-10 + 30-100) (OPC-10000-13 §4.2.2.6)
- [ ] T045 [P] [US3] Add DurationBad aggregate test in `tests/unit/test_aggregate_full.c`: accumulate Bad/Good/Bad samples at t=0, t=20, t=50, publish at t=100, verify DurationBad = 70ms (0-20 + 50-100) (OPC-10000-13 §4.2.2.7)
- [ ] T046 [P] [US3] Add PercentGood/PercentBad aggregate test in `tests/unit/test_aggregate_full.c`: accumulate 4 Good + 1 Bad, publish, verify PercentGood = 80.0, PercentBad = 20.0 (OPC-10000-13 §4.2.2.12, §4.2.2.13)
- [ ] T047 [P] [US3] Add Start/End aggregate tests in `tests/unit/test_aggregate_full.c`: accumulate [10.0, 20.0, 30.0], publish, verify Start = 10.0, End = 30.0 (OPC-10000-13 §4.2.2.14, §4.2.2.5)
- [ ] T048 [P] [US3] Add TimeAverage aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples at known timestamps, publish, verify time-weighted average matches expected (OPC-10000-13 §4.2.2.16)
- [ ] T049 [P] [US3] Add Total aggregate test in `tests/unit/test_aggregate_full.c`: accumulate [5.0, 3.0, 2.0], publish, verify Total = 10.0 (OPC-10000-13 §4.2.2.18)
- [ ] T050 [P] [US3] Add Delta aggregate test in `tests/unit/test_aggregate_full.c`: accumulate [10.0, 15.0, 12.0], publish, verify Delta = 2.0 (end − start) (OPC-10000-13 §4.2.2.4)
- [ ] T051 [P] [US3] Add WorstQuality aggregate test in `tests/unit/test_aggregate_full.c`: accumulate Good → Uncertain → Bad statuses, publish, verify WorstQuality = Bad (OPC-10000-13 §4.2.2.20)
- [ ] T052 [P] [US3] Add Interpolative aggregate test in `tests/unit/test_aggregate_full.c`: verify interpolated value at processing-interval boundary matches expected linear interpolation (OPC-10000-13 §4.2.2.8)
- [ ] T052a [P] [US3] Add DurationInStateZero aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples with status transitions through zero-state, publish, verify duration in zero-state matches expected (OPC-10000-13 §4.2.2)
- [ ] T052b [P] [US3] Add DeltaBounds aggregate test in `tests/unit/test_aggregate_full.c`: accumulate [10.0, 18.0, 12.0], publish, verify DeltaBounds = {delta: 2.0, bounds: {low: 10.0, high: 18.0}} (OPC-10000-13 §4.2.2)
- [ ] T052c [P] [US3] Add Maximum2/Minimum2 aggregate tests in `tests/unit/test_aggregate_full.c`: accumulate [3.0, 7.0, 2.0], publish, verify Maximum2 = 7.0 without timestamp, Minimum2 = 2.0 without timestamp (simple extrema vs timed variants) (OPC-10000-13 §4.2.2)
- [ ] T052d [P] [US3] Verify existing Maximum/Minimum tests (`test_maximum_aggregate_direct` and `test_minimum_aggregate_direct` from spec 043 in `tests/unit/test_aggregate_full.c`) already cover MaximumActualTime/MinimumActualTime behavior — aggregate IDs 2347/2346 already implemented with timestamp-preserving publish, no new tests needed (OPC-10000-13 §4.2.2)
- [ ] T052e [P] [US3] Add TimeAverage2 aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples, publish, verify TimeAverage2 uses simple average (not time-weighted) of sample values (OPC-10000-13 §4.2.2.17)
- [ ] T052f [P] [US3] Add Total2 aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples, publish, verify Total2 uses time-weighted sum vs simple sum (OPC-10000-13 §4.2.2.19)
- [ ] T052g [P] [US3] Add WorstQuality2 aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples with mixed discrete/analog qualities, publish, verify WorstQuality2 evaluates per OPC UA quality rules (OPC-10000-13 §4.2.2.21)
- [ ] T052h [P] [US3] Add AnnotationCount aggregate test in `tests/unit/test_aggregate_full.c`: accumulate samples with annotations, publish, verify AnnotationCount = number of samples containing annotations (OPC-10000-13 §4.2.2.2)
- [ ] T053 [US3] Verify all new aggregate tests FAIL against current code (only 3 aggregates whitelisted, filter rejects others — test-first gate via direct API bypass)

### Implementation for User Story 3

- [ ] T054 [US3] Add 21 new aggregate type ID constants to `include/muc_opcua/opcua_ids.h`: `MU_ID_AGGREGATETYPE_COUNT` (2351), `DELTA` (2359), `DURATION_GOOD` (2360), `DURATION_BAD` (2361), `DURATION_IN_STATE_ZERO` (2363), `END` (2357), `INTERPOLATIVE` (2341), `MAXIMUM_2` (11287), `MINIMUM_2` (11288), `PERCENT_GOOD` (2365), `PERCENT_BAD` (2366), `RANGE` (2350), `START` (2358), `TIME_AVERAGE` (2343), `TIME_AVERAGE_2` (11286), `TOTAL` (2344), `TOTAL_2` (11308), `WORST_QUALITY` (2367), `WORST_QUALITY_2` (11292), `ANNOTATION_COUNT` (2352), `DELTA_BOUNDS` (11509) — note: MaximumActualTime (2346) and MinimumActualTime (2347) already exist as `MU_ID_AGGREGATETYPE_MAXIMUM`/`MU_ID_AGGREGATETYPE_MINIMUM` (OPC-10000-13 §4.2.2)
- [ ] T055 [US3] Expand accumulator union in `mu_aggregate_state_t` in `src/services/subscription.h`, gated on `#if MUC_OPCUA_AGGREGATE_FULL`: add `count`, `range`, `duration`, `percent`, `endpoint`, `timeavg`, `total`, `worstq`, `interp` members per data-model.md
- [ ] T056 [US3] Add Count accumulate/publish branches in `src/services/subscription_aggregate.c`: accumulate increments count, publish outputs Int64, gated on `#if MUC_OPCUA_AGGREGATE_FULL` (OPC-10000-13 §4.2.2.3)
- [ ] T057 [P] [US3] Add Range accumulate/publish branches: accumulate tracks min_val + max_val, publish computes (max − min) as Double (OPC-10000-13 §4.2.2.11)
- [ ] T058 [P] [US3] Add DurationGood/Bad/InStateZero accumulate/publish branches: accumulate tracks start-time and running-total, publish outputs duration in ms as Double (OPC-10000-13 §4.2.2)
- [ ] T059 [P] [US3] Add PercentGood/PercentBad accumulate/publish branches: accumulate counts Good/Bad samples, publish outputs percentage as Double (OPC-10000-13 §4.2.2.12-13)
- [ ] T060 [P] [US3] Add Start/End accumulate/publish branches: Start captures first sample only, End captures last (overwrite) (OPC-10000-13 §4.2.2.5, §4.2.2.14)
- [ ] T061 [P] [US3] Add TimeAverage/TimeAverage2 accumulate/publish branches: accumulate time-weighted sum using time deltas between samples, publish weighted average (OPC-10000-13 §4.2.2.16-17)
- [ ] T062 [P] [US3] Add Total/Total2 accumulate/publish branches: accumulate running sum, publish total (OPC-10000-13 §4.2.2.18-19)
- [ ] T063 [P] [US3] Add Delta accumulate/publish branches: tracks first_val and last_val, publish computes last − first (OPC-10000-13 §4.2.2.4)
- [ ] T064 [P] [US3] Add WorstQuality/WorstQuality2 accumulate/publish branches: tracks worse status across samples per OPC UA status severity (OPC-10000-13 §4.2.2.20-21)
- [ ] T065 [P] [US3] Add Interpolative accumulate/publish branches: track prev_val + prev_time, at publish interpolate to processing-interval boundary (OPC-10000-13 §4.2.2.8)
- [ ] T065a [P] [US3] Add DeltaBounds accumulate/publish branches: tracks first/last val + min/max bounds, publish computes delta with bounding range (OPC-10000-13 §4.2.2)
- [ ] T065b [P] [US3] Add Maximum2/Minimum2 accumulate/publish branches: track simple extreme values without timestamp (same accumulator as Maximum/Minimum but publish as plain Double without time) (OPC-10000-13 §4.2.2)
- [ ] T065c [P] [US3] Verify existing Maximum/Minimum accumulate/publish branches (`monitored_item_accumulate_aggregate` / `monitored_item_publish_aggregate` for MINIMUM/MAXIMUM cases in `src/services/subscription_aggregate.c`) already serve as MaximumActualTime/MinimumActualTime — publish preserves original variant type + timestamp, no new branches needed (OPC-10000-13 §4.2.2)
- [ ] T065d [P] [US3] Add AnnotationCount accumulate/publish branches: count samples that have non-null annotations, publish as Int64 (OPC-10000-13 §4.2.2.2)
- [ ] T066 [US3] Expand filter reader whitelist in `src/core/service_dispatch/filter_reader.c`: when `MUC_OPCUA_AGGREGATE_FULL` is defined, replace 3-way equality check with a lookup or range check accepting all 24 aggregate type IDs defined (3 existing + 21 new); when not defined, keep existing 3-type whitelist (OPC-10000-4 §7.22)
- [ ] T067 [US3] Run `ctest -R test_aggregate_full` and `ctest -R test_aggregate` on full and standard profiles — all tests pass, existing 3-function tests unchanged

**Checkpoint**: All 21 new aggregate functions independently testable (plus 2 existing). Existing
Average/Minimum/Maximum tests still pass. Bounding variants remain deferred.

---

## Phase 6: User Story 4 — Binary Size Measurement Tooling (Priority: P4)

**Goal**: Update `scripts/measure_size.sh` to produce linked-ELF measurements (not
just archives), add JSON output, add LTO comparison, add graceful toolchain-absence
handling.

**Independent Test**: Run `scripts/measure_size.sh standard` and verify output
includes linked-ELF sizes with `.text`/`.data`/`.bss` breakdown and JSON report.

### Implementation for User Story 4

- [ ] T068 [US4] Create `scripts/size_measure.ld` — minimal Cortex-M0+ linker script with Flash at `0x10000000` (256K), RAM at `0x20000000` (256K), standard sections (`.text`, `.rodata`, `.data`, `.bss`), stack at end of RAM
- [ ] T069 [US4] Create `scripts/size_measure_startup.c` — minimal startup: vector table (stack pointer + Reset_Handler), `Reset_Handler` that zeroes `.bss`, copies `.data`, calls `main()`, loops forever
- [ ] T070 [US4] Create `scripts/size_measure_main.c` — minimal `main()` that calls `mu_server_init()` to force linker to pull in library code (so `--gc-sections` can strip unreferenced symbols)
- [ ] T071 [US4] Modify `scripts/measure_size.sh`: after building each profile archive, also compile + link an ELF using `size_measure.ld` + `size_measure_startup.c` + `size_measure_main.c` + `libmuc_opcua.a`, then run `arm-none-eabi-size` on the ELF
- [ ] T072 [US4] Add JSON output to `scripts/measure_size.sh`: write `build/size-arm/size-report.json` with per-profile records containing `profile`, `archive` (`.text`/`.data`/`.bss`/`.dec`), `elf` (same fields), `elf_path`, and `timestamp`
- [ ] T073 [US4] Add `--lto` flag to `scripts/measure_size.sh`: rebuild with `-flto -ffat-lto-objects` in `CMAKE_C_FLAGS` (bypassing `CheckIPOSupported` cross-compile failure), produce `lto_elf` measurements in addition to `elf`, report delta
- [ ] T074 [US4] Change toolchain-absence handling in `scripts/measure_size.sh`: replace `exit 127` with `echo "warning: arm-none-eabi-gcc not found; skipping size measurement" >&2; exit 0` for graceful CI skip
- [ ] T075 [US4] Run `bash scripts/measure_size.sh standard` and verify: linked-ELF output in table, JSON file produced, archive size ≥ ELF size (dead-code elimination visible)

**Checkpoint**: Size measurement tooling produces accurate linked-ELF sizes with
JSON output. Gracefully handles missing cross-compiler.

---

## Phase 7: Polish & Cross-Cutting Validation

**Purpose**: Cross-cutting validation before completion

- [ ] T076 Run clang-format on all modified files in `src/`, `include/`, `tests/`
- [ ] T077 Run cppcheck — zero new warnings introduced on `src/` and `tests/`
- [ ] T078 Run `ctest --output-on-failure` on full profile — all tests pass, zero regressions
- [ ] T079 [P] Run `ctest --output-on-failure` on micro profile — all tests pass, zero regressions
- [ ] T080 [P] Run `ctest --output-on-failure` on standard profile — all tests pass, zero regressions
- [ ] T081 [P] Run `ctest --output-on-failure` on embedded profile — all tests pass, zero regressions
- [ ] T082 Verify `grep -rn "STUB:" tests/` returns zero matches across the 5 files touched by specs 043-044 (already clean, verify no regressions)
- [ ] T083 Run `bash scripts/measure_size.sh all` and record linked-ELF sizes against budget in plan.md
- [ ] T084 Update `TODO.md`: mark D1, D2, D3, D4 as complete, update deferred-items list

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — verify baseline first
- **Phase 2 (Foundational)**: Depends on Phase 1 — CMake gate fix blocks D1, D2, D3
- **Phase 3 (US2 — Audit)**: Depends on Phase 2 — uses `MUC_OPCUA_AUDITING` gate
- **Phase 4 (US1 — Complex Types)**: Depends on Phase 2 — uses `MUC_OPCUA_COMPLEX_TYPES` gate
- **Phase 5 (US3 — Aggregates)**: Depends on Phase 2 — uses `MUC_OPCUA_AGGREGATE_FULL` gate
- **Phase 6 (US4 — Size Measurement)**: Depends on Phase 1 only (independent scripting)
- **Phase 7 (Polish)**: Depends on all prior phases

### Within Each User Story

- Tests before implementation (test-first protocol gate)
- Public API declarations before implementation
- Encoder/decoder before integration tests
- StatusCode mapping before error-path tests

### Parallel Opportunities

- T002, T003, T004 can run in parallel after T001
- T006, T007, T008 can run in parallel (different `if` blocks in same CMakeLists.txt)
- T010-T013 (US2 tests) can run in parallel (different test functions)
- T023-T028 (US1 tests) can run in parallel (different test functions)
- T042-T052h (US3 tests) can run in parallel (different test functions)
- T057-T065d (US3 aggregate function branches) can run in parallel (different `if/else` branches in same file — consider grouping into batches of 4-5 to avoid merge conflicts)
- T065a, T065b, T065c, T065d can run in parallel with T057-T064 (different functions in same file)
- T068-T070 (US4 linker/startup files) can run in parallel (different files)
- **US2, US1, US3, US4 can proceed in parallel after Phase 2** (different files, no shared state)
- T079, T080, T081 (profile tests) can run in parallel

### Suggested Execution Order

```
T001 → T002,T003,T004 (phase 1)
  └── T005 → T006,T007,T008 → T009 (phase 2, foundation)
        ├── T010-T022 (phase 3, US2 — audit)
        ├── T023-T041 (phase 4, US1 — complex types)
        ├── T042-T053  → T054-T065d → T066 → T067 (phase 5, US3 — aggregates)
        └── T068-T075 (phase 6, US4 — size measurement, can start after phase 1)
T076-T084 (phase 7, polish)
```

---

## Task Summary

| Phase | Story | Count | Description |
|-------|-------|-------|-------------|
| Setup | — | 4 | Baseline verification, gate catalog |
| Foundational | — | 5 | Fix CMake compile definition gates |
| US2 | P2 | 13 | Audit callback dispatch (5 tests + 8 impl) |
| US1 | P1 | 19 | Complex type encode/decode (7 tests + 12 impl) |
| US3 | P3 | 37 | Aggregate functions (19 tests + 18 impl) |
| US4 | P4 | 8 | Size measurement tooling (0 tests + 8 impl) |
| Polish | — | 9 | Cross-cutting validation |
| **Total** | | **95** | |

## Implementation Strategy

**MVP (Minimum Viable)**: Phase 2 (CMake fix) + Phase 4 (US1 — complex types) alone
delivers the highest-priority feature. Complex type encode/decode unblocks real-world
OPC UA structure serialization.

**Incremental**: Add US2 (audit) next — callback dispatch is small and self-contained.
Then US3 (aggregate functions — 21 new functions + 2 existing, largest phase). US4 (size measurement)
is independent scripting and can be done at any time after Phase 1.

**Deferred explicitly**: Bounding variant aggregate functions, CTT compliance,
service-handler audit event emission, PubSub complex type integration, mDNS discovery (F1).
