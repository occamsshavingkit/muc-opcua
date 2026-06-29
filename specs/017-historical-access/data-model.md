# Data Model: Historical Access

## Structs

```c
typedef struct {
    opcua_datetime_t source_timestamp;
    opcua_datetime_t server_timestamp;
    opcua_statuscode_t status;
    mu_variant_t value;
} mu_historical_data_point_t;

typedef struct {
    opcua_statuscode_t (*read_raw_modified)(
        void *context,
        const mu_nodeid_t *node_id,
        opcua_boolean_t is_read_modified,
        opcua_datetime_t start_time,
        opcua_datetime_t end_time,
        opcua_uint32_t num_values_per_node,
        opcua_boolean_t return_bounds,
        const opcua_byte_t *continuation_point_in,
        size_t cp_in_length,
        opcua_byte_t *continuation_point_out,
        size_t *cp_out_length,
        mu_historical_data_point_t *data_points,
        size_t max_data_points,
        size_t *actual_data_points
    );

    opcua_statuscode_t (*update_data)(
        void *context,
        const mu_nodeid_t *node_id,
        opcua_uint32_t perform_insert_replace,
        const mu_historical_data_point_t *data_points,
        size_t data_points_count,
        opcua_statuscode_t *results
    );
    
    void *context;
} mu_history_adapter_t;
```

## Internal Additions

- Extend `mu_server_config_t` with `mu_history_adapter_t history_adapter`.
- Extend `mu_server_t` with `mu_history_adapter_t history_adapter`.
- Calculate max continuation point sizes and store them inside static context if needed, though they can be handled opaque by the adapter.
