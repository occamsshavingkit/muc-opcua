# Quickstart Guide: Custom Methods, Aes256_Sha256_RsaPss, Server Diagnostics, and Dynamic Node Management

## 1. Custom Method Call Example

```c
#include "micro_opcua/server.h"

// Define callback
static opcua_statuscode_t my_reboot_callback(
    mu_server_t *server,
    const mu_nodeid_t *object_id,
    const mu_nodeid_t *method_id,
    const mu_variant_t *input_args,
    size_t input_args_count,
    mu_variant_t *output_args,
    size_t *output_args_count)
{
    // Local device reboot logic...
    return MU_STATUS_GOOD;
}

// Register in address space setup
void setup_address_space(mu_server_t *server) {
    mu_nodeid_t method_id = mu_nodeid_numeric(1, 5001);
    mu_server_add_method_node(server, &method_id, "Reboot", my_reboot_callback);
}
```

## 2. Enabling Aes256_Sha256_RsaPss

Configure CMake with the required crypto adapter enabled (e.g. OpenSSL, Mbed TLS, or wolfSSL):

```bash
cmake -S . -B build -DMICRO_OPCUA_HAVE_MBEDTLS=ON -DMICRO_OPCUA_BUILD_TESTS=ON
```

During endpoint configuration, specify the security policy:

```c
mu_server_add_endpoint(server, "opc.tcp://0.0.0.0:4840", "http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss", MU_MESSAGESECURITYMODE_SIGNANDENCRYPT);
```

## 3. Dynamic Node Management Example

```c
mu_nodeid_t new_var_id = mu_nodeid_numeric(1, 6001);
mu_server_add_variable_node(server, &new_var_id, "DynamicTemperature", MU_DATATYPE_FLOAT);

// When sensor unplugged:
mu_server_delete_node(server, &new_var_id);
```
