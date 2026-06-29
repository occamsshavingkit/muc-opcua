# Data Model Changes: Aggregate Subscriptions

## Entities

### `mu_aggregate_state_t` (New Struct)

Represents the active aggregation state for a MonitoredItem.

```c
typedef struct {
    opcua_uint32_t aggregate_type;      /* MU_ID_AGGREGATETYPE_AVERAGE, MINIMUM, MAXIMUM */
    opcua_double_t processing_interval;  /* in milliseconds */
    opcua_datetime_t last_calculation;   /* timestamp of last aggregate calculation */
    opcua_uint32_t sample_count;        /* number of samples in the current interval */
    union {
        struct {
            opcua_double_t sum;
        } avg;
        struct {
            mu_variant_t min_val;
        } min;
        struct {
            mu_variant_t max_val;
        } max;
    } accumulator;
} mu_aggregate_state_t;
```

### `mu_monitored_item_t` (Updated Struct)

We extend the existing `mu_monitored_item_t` structure with optional aggregate support.

```c
typedef struct {
    /* ... existing fields ... */
    opcua_boolean_t has_aggregate;
    mu_aggregate_state_t aggregate_state;
} mu_monitored_item_t;
```

## Validation Rules

1. **Numeric Source Only**: Aggregate filters are only allowed on numeric variables. If the variable datatype is not a numeric type (Int32, Double, etc.), creation returns `Bad_FilterNotAllowed`.
2. **Unsupported Aggregates**: If the client requests an aggregate type other than `Average` (`2342`, OPC-10000-13 §4.2.2.4), `Minimum` (`2346`, OPC-10000-13 §4.2.2.9), or `Maximum` (`2347`, OPC-10000-13 §4.2.2.10), return `Bad_MonitoredItemFilterUnsupported`.
3. **Invalid Interval**: `processingInterval` must be > 0. Otherwise, return `Bad_FilterNotAllowed`.
