# Research: Implement Deferred Features (D1-D4)

**Feature**: 045-implement-deferred-features
**Date**: 2026-07-06
**Phase**: 0 — Outline & Research

## D1: Complex Type Binary Encode/Decode

### Decision 1: Encoder/Decoder Function Signatures

Follow existing codebase conventions for `mu_binary_encode_*` / `mu_binary_decode_*`:

```c
opcua_statuscode_t mu_binary_encode_struct(mu_binary_writer_t *writer,
    const mu_structure_definition_t *def, const void *field_values);
opcua_statuscode_t mu_binary_decode_struct(mu_binary_reader_t *reader,
    const mu_structure_definition_t *def, void *field_values);
opcua_statuscode_t mu_binary_encode_enum(mu_binary_writer_t *writer, opcua_int32_t value);
opcua_statuscode_t mu_binary_decode_enum(mu_binary_reader_t *reader, opcua_int32_t *value);
```

**Rationale**: Matches existing `mu_binary_write_<type>(writer, const <type> *)` /
`mu_binary_read_<type>(reader, <type> *)` pattern used throughout the codebase.

**Alternatives considered**: Passing `mu_server *` to access type registry — rejected
because encoder/decoder should be stateless against the server. Lookup is done before
calling encode/decode.

### Decision 2: Field Value Layout

Fields are passed as `void *` with implicit packed-struct layout matching field
definition order. Each field occupies its natural C type size at its position in
the struct.

**Rationale**: Consistent with embedded OPC UA stack conventions (no heap, no
descriptor tables at runtime). Caller manages memory.

**Alternatives considered**:
- Individual field getter/setter API — too verbose for embedded use
- Descriptor-based dynamic access — requires runtime type info tables, too large

### Decision 3: CMake Gate Fix Required

`MUC_OPCUA_COMPLEX_TYPES` is set as a CMake option and profile preset, but
`target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_COMPLEX_TYPES=1)` is
missing from `src/CMakeLists.txt`.

**Fix**: Add to `src/CMakeLists.txt`:
```cmake
if(MUC_OPCUA_COMPLEX_TYPES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_COMPLEX_TYPES=1)
endif()
```

### Decision 4: Type Lookup Helper

Add `mu_find_structure_definition` / `mu_find_enum_definition` to
`src/address_space/complex_types.c` for O(1) lookup by NodeId from the
in-server parallel arrays.

**Rationale**: Encoder caller needs to find the definition by NodeId when
receiving a structure-bearing ExtensionObject.

---

## D2: Audit Event Callback Dispatch

### Decision 5: Callback Storage Pattern

Use Pattern B (runtime multi-callback array, matching custom methods):

```c
#define MU_MAX_AUDIT_CALLBACKS 4
struct {
    mu_audit_callback_t callback;
    void *context;
} audit_callbacks[MU_MAX_AUDIT_CALLBACKS];
size_t audit_callback_count;
```

**Rationale**: Consistent with `mu_server_register_method_callback` pattern.
Audit events are a hot-path notification mechanism where multiple consumers
may exist (logging, alerting, metrics).

**Alternatives considered**:
- Config-time single callback (Pattern A) — rejected because audit consumers
  are additive (log + forward + alert), not single-replacement
- Dynamic linked list — rejected for embedded (no heap)

### Decision 6: Callback API

```c
typedef void (*mu_audit_callback_t)(struct mu_server *server, const mu_audit_event_t *event, void *context);

void mu_server_set_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context);
opcua_statuscode_t mu_server_add_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context);
```

**Rationale**: `set` replaces any existing (simple case), `add` appends
(multi-consumer). `void *context` matches runtime callback pattern.

### Decision 7: CMake Gate Fix Required

Same issue as D1 — `MUC_OPCUA_AUDITING` is never compiled into C code.

**Fix**: Add to `src/CMakeLists.txt`:
```cmake
if(MUC_OPCUA_AUDITING)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_AUDITING=1)
endif()
```

### Decision 8: Audit Event Emission Deferred

Per spec scope boundaries, `mu_raise_audit_event` calls from service handlers
(e.g., `handle_create_session` → `mu_raise_audit_create_session`) are OUT of
scope for this feature. Only the dispatch mechanism is implemented.

**Rationale**: Each service handler integration requires verifying the correct
event fields at each call site, which is a non-trivial effort across 4+ handlers.

---

## D3: Remaining Aggregate Functions

### Decision 9: Aggregate Function Implementation Pattern

Follow the existing `if/else if` chain pattern in both `monitored_item_accumulate_aggregate`
and `monitored_item_publish_aggregate`. Add new union members to `mu_aggregate_state_t`
for functions requiring per-interval state (Count needs `opcua_uint32_t count`,
Range needs `min_val + max_val` pair, Duration needs `start_time + running_total`, etc.).

**Rationale**: The existing pattern is clean, compact, and testable. A switch dispatch
table would be more maintainable but adds ROM overhead for the table itself.

**Alternatives considered**:
- Per-aggregate-type function pointers (vtable) — too much flash overhead for 42 functions
- Unified algorithm framework — over-abstracted, each aggregate has unique computation

### Decision 10: Accumulator Union Expansion

The current union has 3 members (`avg`, `min`, `max`). New types need:

| Function | Union Member | Fields |
|----------|-------------|--------|
| Count | `count` | `opcua_uint32_t value` |
| Range | `range` | `min_val`, `max_val` (both `mu_variant_t`) |
| Duration* | `duration` | `start_time_ms`, `running_total_ms` (both `opcua_uint64_t`) |
| Percent* | `percent` | `good_count`, `bad_count` (both `opcua_uint32_t`) |
| Start/End | `endpoint` | `first_val`, `last_val` (both `mu_variant_t`) |
| TimeAverage | `timeavg` | `weighted_sum` (double), `duration_ms` (uint64) |
| Total/Total2 | `total` | `running_total` (double) |
| Delta | `delta` | `first_val`, `last_val` (both `mu_variant_t`) |
| WorstQuality* | `worstq` | `worst_status` (opcua_statuscode_t) |
| AnnotationCount | `annotationc` | `count` (uint32) |
| Interpolative | `interp` | `prev_val`, `prev_time` (variant + uint64) |

**Rationale**: Each union member is 8-16 bytes. With ~12 members, the union grows
from ~16 to ~32 bytes (largest member dictates size).

**Alternatives considered**:
- Single generic "extended_state" void pointer — rejected (heap allocation required)
- `opcua_variant_t` array for all state — rejected (unbounded, requires heap)

### Decision 11: 23 Primary Functions, Bounding Variants Deferred

Per spec scope, implement 23 primary aggregate functions. Bounding variants
(BoundingValues, StartBound, EndBound, etc.) are deferred.

**Rationale**: Bounding variants compose primary functions + bounding logic.
Implementing primaries first enables straightforward bounding variant composition
in a follow-up.

### Decision 12: Filter Reader Expansion

Replace the 3-way equality check in `filter_reader.c` with a range check or
a lookup when `MUC_OPCUA_AGGREGATE_FULL` is enabled.

---

## D4: Binary Size Measurement Tooling

### Decision 13: Minimal Linker Script Approach

Create `scripts/size_measure.ld` targeting Cortex-M0+ with standard memory map.
Create `scripts/size_measure_startup.c` with minimal vector table.

**Rationale**: Avoids dependency on Pico SDK for measurement purposes. Simple
enough to maintain (<50 lines each). The linked ELF enables `--gc-sections`
and LTO to actually fire.

**Alternatives considered**:
- Using Pico SDK for ELF linking — too heavy (requires SDK download, complex
  CMake setup for just measuring binary size)
- Using `arm-none-eabi-ld` directly with `--just-symbols` — doesn't give
  realistic section sizes

### Decision 14: JSON Output Format

Single JSON file at `build/size-arm/size-report.json` with per-profile records
including both archive and ELF measurements for comparison.

**Rationale**: Machine-readable for CI dashboards and automated regression detection.

### Decision 15: LTO Comparison

Add `--lto` flag to `measure_size.sh` that rebuilds with `-flto -ffat-lto-objects`
in CFLAGS (bypassing CMake `CheckIPOSupported` which fails for cross-compile) and
reports both LTO-ON and LTO-OFF ELF sizes.

### Decision 16: Graceful Toolchain Absence

Change `measure_size.sh` from `exit 127` on missing compiler to `echo "warning: ..."` + `exit 0`.

**Rationale**: CI machines may not have `arm-none-eabi-gcc`. The script should
not fail the build, just gracefully skip.
