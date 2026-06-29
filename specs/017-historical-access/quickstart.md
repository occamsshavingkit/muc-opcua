# Quickstart: Historical Access

This quickstart demonstrates how to set up an in-memory history adapter to test Historical Access in `micro-opcua`.

## 1. Configure the Build

Ensure that `MICRO_OPCUA_SERVICE_HISTORY` is defined in `include/micro_opcua/config.h`.

```c
#define MICRO_OPCUA_SERVICE_HISTORY
```

## 2. Create the Backend Implementation

Implement the callbacks for the adapter interface.

```c
#include "micro_opcua/micro_opcua.h"

// Example in-memory store
static mu_historical_data_point_t g_store[100];
static size_t g_store_count = 0;

static opcua_statuscode_t my_read_history(
    void *context,
    const mu_nodeid_t *node_id,
    opcua_boolean_t is_read_modified,
    opcua_datetime_t start_time,
    opcua_datetime_t end_time,
    opcua_uint32_t num_values_per_node,
    opcua_boolean_t return_bounds,
    const opcua_byte_t *cp_in,
    size_t cp_in_length,
    opcua_byte_t *cp_out,
    size_t *cp_out_length,
    mu_historical_data_point_t *data_points,
    size_t max_data_points,
    size_t *actual_data_points
) {
    // Simply copy all stored data (ignoring filters for this minimal example)
    size_t to_copy = g_store_count > max_data_points ? max_data_points : g_store_count;
    for (size_t i = 0; i < to_copy; i++) {
        data_points[i] = g_store[i];
    }
    *actual_data_points = to_copy;
    *cp_out_length = 0; // No continuation
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t my_update_history(
    void *context,
    const mu_nodeid_t *node_id,
    opcua_uint32_t perform_insert_replace,
    const mu_historical_data_point_t *data_points,
    size_t data_points_count,
    opcua_statuscode_t *results
) {
    for (size_t i = 0; i < data_points_count; i++) {
        if (g_store_count < 100) {
            g_store[g_store_count++] = data_points[i];
            results[i] = MU_STATUS_GOOD;
        } else {
            results[i] = MU_STATUS_BAD_OUT_OF_MEMORY;
        }
    }
    return MU_STATUS_GOOD;
}
```

## 3. Attach the Adapter

Attach your implementation to the server config before initialization:

```c
mu_server_config_t config;
memset(&config, 0, sizeof(config));

// ... set up other config

config.history_adapter.read_raw_modified = my_read_history;
config.history_adapter.update_data = my_update_history;
config.history_adapter.context = NULL;

mu_server_init(storage, sizeof(storage), &config, &server);
```

Clients can now read and write historical data for variables that have `AccessLevel` and `Historizing` flags correctly configured.
