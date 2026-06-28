# Quick Start: Core Feature Expansion

**Feature**: Core Feature Expansion  
**Feature Branch**: `009-core-feature-expansion`

## Integration Guide

To enable and configure the new core capabilities:

### 1. Enable Features in CMake

Add these flags to your CMake configure step or `cmake/MicroOpcUaOptions.cmake`:

```cmake
option(MICRO_OPCUA_SERVICE_WRITE "Build the Write service" ON)
option(MICRO_OPCUA_MULTIPLE_CONNECTIONS "Support concurrent client TCP connections" ON)
option(MICRO_OPCUA_EVENTS "Build event and Alarm & Condition support" ON)
```

### 2. Configure Write Handler

Register a callback to handle incoming writes:

```c
#include "micro_opcua/micro_opcua.h"

static opcua_statuscode_t my_write_handler(void *handle,
                                           const mu_nodeid_t *node_id,
                                           opcua_uint32_t attribute_id,
                                           const mu_variant_t *value)
{
    if (node_id->identifier.numeric == 1001) {
        /* Apply write to device actuator */
        if (value->type == MU_TYPE_INT32) {
            set_actuator_speed(value->value.i32);
            return MU_STATUS_GOOD;
        }
        return MU_STATUS_BAD_TYPEMISMATCH;
    }
    return MU_STATUS_BAD_NOTWRITABLE;
}

int main(void)
{
    mu_server_config_t config;
    /* ... common configuration ... */
    
    config.write_handler = my_write_handler;
    config.write_handler_handle = NULL;
    
    /* ... init and poll loop ... */
}
```
