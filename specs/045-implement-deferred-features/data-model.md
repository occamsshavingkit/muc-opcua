# Data Model: Implement Deferred Features (D1-D4)

**Feature**: 045-implement-deferred-features
**Date**: 2026-07-06

## D1: Complex Type Binary Encode/Decode

### Entity: `mu_structure_definition_t` (existing, expanded)

Describes a custom OPC UA Structure DataType. Stored in server's `complex_types` parallel arrays.
Encoder/decoder use this as a schema to drive serialization.

| Field | Type | Description |
|-------|------|-------------|
| `default_encoding_id` | `mu_nodeid_t` | DefaultBinary encoding NodeId |
| `base_data_type` | `mu_nodeid_t` | Parent DataType NodeId in type hierarchy |
| `structure_type` | `opcua_uint32_t` | `MU_STRUCTURE_TYPE_STRUCTURE` (0) or `MU_STRUCTURE_TYPE_OPTIONAL` (1) |
| `fields` | `const mu_structure_field_t *` | Pointer to field descriptor array |
| `field_count` | `opcua_uint16_t` | Number of fields |

### Entity: `mu_structure_field_t` (existing, unchanged)

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const char *` | Field name |
| `description` | `mu_localized_text_t` | Locale + text description |
| `data_type` | `mu_nodeid_t` | NodeId of the field's data type |
| `value_rank` | `opcua_int32_t` | -1 = scalar, 0 = any, 1+ = array dimensions |
| `array_dimensions` | `opcua_uint32_t *` | Dimension sizes (when rank >= 1) |
| `max_string_length` | `opcua_uint32_t` | Max string length for string fields |
| `is_optional` | `opcua_boolean_t` | If true, field only encoded when EncodingMask bit set |

### Entity: `mu_enum_definition_t` (existing, unchanged)

| Field | Type | Description |
|-------|------|-------------|
| `fields` | `const mu_enum_field_t *` | Enumeration value descriptors |
| `field_count` | `opcua_uint16_t` | Number of values |

### Encoder State Machine

The encoder processes fields sequentially in definition order:

```
for each field in def->fields:
    if field.is_optional:
        check encoding_mask bit
        if not set: skip field
    encode field value according to field.data_type
        scalar → mu_binary_write_{type}(writer, &value)
        array  → mu_binary_write_uint32(writer, length)
                 for each element: mu_binary_write_{type}(writer, &value[i])
        nested struct → recursive mu_binary_encode_struct(writer, nested_def, nested_values)
```

### Decoder State Machine

```
for each field in def->fields:
    if field.is_optional:
        check encoding_mask bit
        if not set: zero field in output, skip decode
    decode field according to field.data_type
        scalar → mu_binary_read_{type}(reader, &value)
        array  → mu_binary_read_uint32(reader, &length)
                 for each element: mu_binary_read_{type}(reader, &value[i])
        nested struct → recursive mu_binary_decode_struct(reader, nested_def, nested_values)
```

---

## D2: Audit Event Callback Dispatch

### Entity: `mu_audit_callback_t` (NEW)

```c
typedef void (*mu_audit_callback_t)(struct mu_server *server,
    const mu_audit_event_t *event, void *context);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `server` | `struct mu_server *` | Server that raised the event |
| `event` | `const mu_audit_event_t *` | Read-only event struct |
| `context` | `void *` | Opaque user context from registration |

### Entity: `mu_audit_event_t` (existing, expanded)

| Field | Type | Description |
|-------|------|-------------|
| `event_type` | `opcua_uint32_t` | Discriminator (OPEN_SECURE_CHANNEL=0, CREATE_SESSION=1, ACTIVATE_SESSION=2, WRITE_UPDATE=3) |
| `action_timestamp` | `opcua_datetime_t` | Time the event was raised (populated by dispatch) |
| `status` | `opcua_boolean_t` | Operation success/failure |
| `server_id` | `mu_string_t` | Server identifier |
| `client_audit_entry_id` | `mu_string_t` | Client-provided audit correlation ID |
| `client_user_id` | `mu_string_t` | Authenticated user identity |
| `specific` | anonymous union | Event-type-specific payload (see below) |

**Union members** (unchanged):
- `open_channel`: `{ mu_string_t secure_channel_id; }`
- `create_session`: `{ mu_nodeid_t session_id; }`
- `activate_session`: `{ mu_nodeid_t session_id; mu_string_t user_name; }`
- `write_update`: `{ mu_nodeid_t node_id; mu_variant_t old_value; mu_variant_t new_value; }`

### Callback Storage in `struct mu_server` (NEW)

```c
#if MUC_OPCUA_AUDITING
#define MU_MAX_AUDIT_CALLBACKS 4
    struct {
        mu_audit_callback_t callback;
        void *context;
    } audit_callbacks[MU_MAX_AUDIT_CALLBACKS];
    size_t audit_callback_count;
#endif
```

### Dispatch Flow

```
mu_raise_audit_event(server, event):
    if server == NULL or event == NULL: return
    if !server->config.auditing_enabled: return
    populate event->action_timestamp = current_time()
    for each callback in server->audit_callbacks:
        callback(server, event, callback.context)
```

### Registration API

| API | Behavior |
|-----|----------|
| `mu_server_set_audit_callback(server, cb, ctx)` | Clears all existing callbacks, registers `cb` at position 0. If `cb == NULL`, clears all. |
| `mu_server_add_audit_callback(server, cb, ctx)` | Appends `cb` to callback array. Returns `MU_STATUS_BAD_OUTOFMEMORY` if full (4 max). Returns `MU_STATUS_BAD_ARGUMENTSMISSING` if `cb == NULL`. |

---

## D3: Aggregate Functions

### Entity: `mu_aggregate_state_t` (existing, expanded)

Per-monitored-item state for aggregate computation. The `accumulator` union is expanded.

| Field | Type | Description |
|-------|------|-------------|
| `processing_interval` | `opcua_double_t` | Milliseconds between publish events |
| `last_calculation` | `opcua_datetime_t` | Timestamp of last aggregate calculation |
| `accumulator` | union | Per-aggregate-type state (see below) |
| `aggregate_type` | `opcua_uint32_t` | One of `MU_ID_AGGREGATETYPE_*` constants |
| `sample_count` | `opcua_uint32_t` | Samples accumulated in current interval |

**Expanded `accumulator` union**:

| Member | Type | Fields | Used By |
|--------|------|--------|---------|
| `avg` | struct | `opcua_double_t sum` | Average (existing) |
| `min` | struct | `mu_variant_t min_val` | Minimum (existing) |
| `max` | struct | `mu_variant_t max_val` | Maximum (existing) |
| `count` | struct | `opcua_uint32_t value` | Count |
| `range` | struct | `mu_variant_t min_val; mu_variant_t max_val` | Range |
| `duration` | struct | `opcua_uint64_t start_ms; opcua_uint64_t running_total_ms` | DurationGood, DurationBad, DurationInStateZero |
| `percent` | struct | `opcua_uint32_t good_count; opcua_uint32_t bad_count` | PercentGood, PercentBad |
| `endpoint` | struct | `mu_variant_t first_val; mu_variant_t last_val` | Start, End, Delta |
| `timeavg` | struct | `opcua_double_t weighted_sum; opcua_uint64_t duration_ms` | TimeAverage, TimeAverage2 |
| `total` | struct | `opcua_double_t running_total` | Total, Total2 |
| `worstq` | struct | `opcua_statuscode_t worst_status` | WorstQuality, WorstQuality2 |
| `interp` | struct | `mu_variant_t prev_val; opcua_uint64_t prev_time_ms` | Interpolative |

### Aggregate Type ID Constants (NEW, all 39)

| Constant | Value | Function |
|----------|-------|----------|
| `MU_ID_AGGREGATETYPE_COUNT` | 2351 | Count |
| `MU_ID_AGGREGATETYPE_DELTA` | 2359 | Delta |
| `MU_ID_AGGREGATETYPE_DURATION_GOOD` | 2360 | DurationGood |
| `MU_ID_AGGREGATETYPE_DURATION_BAD` | 2361 | DurationBad |
| `MU_ID_AGGREGATETYPE_DURATION_IN_STATE_ZERO` | 2363 | DurationInStateZero |
| `MU_ID_AGGREGATETYPE_END` | 2357 | End |
| `MU_ID_AGGREGATETYPE_INTERPOLATIVE` | 2341 | Interpolative |
| `MU_ID_AGGREGATETYPE_MAXIMUM_2` | 11287 | Maximum2 |
| `MU_ID_AGGREGATETYPE_MINIMUM_2` | 11288 | Minimum2 |
| `MU_ID_AGGREGATETYPE_PERCENT_GOOD` | 2365 | PercentGood |
| `MU_ID_AGGREGATETYPE_PERCENT_BAD` | 2366 | PercentBad |
| `MU_ID_AGGREGATETYPE_RANGE` | 2350 | Range |
| `MU_ID_AGGREGATETYPE_START` | 2358 | Start |
| `MU_ID_AGGREGATETYPE_TIME_AVERAGE` | 2343 | TimeAverage |
| `MU_ID_AGGREGATETYPE_TIME_AVERAGE_2` | 11286 | TimeAverage2 |
| `MU_ID_AGGREGATETYPE_TOTAL` | 2344 | Total |
| `MU_ID_AGGREGATETYPE_TOTAL_2` | 11308 | Total2 |
| `MU_ID_AGGREGATETYPE_WORST_QUALITY` | 2367 | WorstQuality |
| `MU_ID_AGGREGATETYPE_WORST_QUALITY_2` | 11292 | WorstQuality2 |
| `MU_ID_AGGREGATETYPE_ANNOTATION_COUNT` | 2352 | AnnotationCount |
| `MU_ID_AGGREGATETYPE_DELTA_BOUNDS` | 11509 | DeltaBounds |

*(Plus bounding variants — deferred)*

---

## D4: Binary Size Measurement

### Entity: JSON Size Report

```json
{
  "toolchain": "arm-none-eabi-gcc 13.2.1",
  "target": "cortex-m0plus",
  "timestamp": "2026-07-06T12:00:00Z",
  "source": "measure_size.sh (linked-ELF)",
  "profiles": [
    {
      "profile": "nano",
      "archive": {"text": 18150, "data": 0, "bss": 0, "dec": 18150},
      "elf":     {"text": 16800, "data": 0, "bss": 0, "dec": 16800},
      "lto_elf": {"text": 15200, "data": 0, "bss": 0, "dec": 15200},
      "elf_path": "build/size-arm/nano/size_measure.elf"
    }
  ]
}
```

### Entity: Minimal Linker Description

Memory layout for size measurement (not for actual deployment):

- FLASH: origin 0x10000000, length 256K
- RAM: origin 0x20000000, length 256K
- Standard `.text`, `.rodata`, `.data`, `.bss` sections
- Stack: 4K at end of RAM
