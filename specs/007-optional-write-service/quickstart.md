# Quickstart: Integrating the Optional Write Service

This document guides you through configuring and using the Write service.

---

## 1. Registering the Write Callback

Define your write callback in the application layer:

```c
#include "micro_opcua/server.h"
#include <stdio.h>

static opcua_uint32_t g_current_setpoint = 100;

opcua_statuscode_t my_write_handler(void *handle,
                                    const opcua_nodeid_t *node_id,
                                    const opcua_variant_t *value)
{
    (void)handle;
    
    /* We only expect writes to the setpoint node (numeric NodeId 5001) */
    if (node_id->identifierType == OPCUA_NODEIDTYPE_NUMERIC && node_id->identifier.numeric == 5001) {
        if (value->type == OPCUA_BUILTINTYPE_INT32) {
            g_current_setpoint = (opcua_uint32_t)value->value.int32;
            printf("Setpoint updated to: %u\n", g_current_setpoint);
            return MU_STATUS_GOOD;
        }
        return MU_STATUS_BAD_TYPEMISMATCH;
    }
    
    return MU_STATUS_BAD_NOTWRITABLE;
}
```

Then register the callback when configuring the server:

```c
mu_server_config_t config = {
    .endpoint_url = "opc.tcp://localhost:4840",
    .write_handler = my_write_handler,
    .write_handler_handle = NULL,
    ...
};
```

---

## 2. Compile Flag Configuration

Ensure that the Write service is enabled during compile-time:

```sh
cmake -B build -DMICRO_OPCUA_SERVICE_WRITE=ON
cmake --build build
```

---

## 3. Testing with a Client

Using a client like `asyncua` (Python), you can write to the node:

```python
import asyncio
from asyncua import Client, ua

async def main():
    async with Client("opc.tcp://localhost:4840") as client:
        # Resolve target node
        var_node = client.get_node("ns=1;i=5001")
        
        # Write Int32 value
        new_val = ua.DataValue(ua.Variant(250, ua.VariantType.Int32))
        await var_node.write_value(new_val)
        print("Write succeeded!")

asyncio.run(main())
```
