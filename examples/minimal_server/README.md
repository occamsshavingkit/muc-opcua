# Minimal Server Example

This example demonstrates how to configure and run the muc-opcua minimal server. It focuses on the initialization of the server identity, network buffers, and a statically allocated address space.

## Memory Lifetimes

In muc-opcua, the server is designed to operate entirely without dynamic memory allocation (`malloc`/`free`). All required memory must be provided by the application during initialization.

### Server Storage
The core server state (`mu_server_t`) is completely opaque. It requires a contiguous block of statically allocated memory provided at initialization:
```c
#define STORAGE_BYTES 4096
static opcua_byte_t g_server_storage[STORAGE_BYTES];

mu_server_init(g_server_storage, sizeof(g_server_storage), &config, &server);
```

### Network Buffers
All transmit and receive operations use caller-owned buffers provided via the `mu_server_config_t`:
```c
static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];

config.receive_buffer = g_recv_buffer;
config.send_buffer = g_send_buffer;
```

### Static Address Space
The OPC UA address space (nodes, references, and static value sources) must reside in read-only static memory (`.rodata` or `const`). The server references these structures directly and never copies them or modifies them:
```c
config.address_space = &g_minimal_address_space;
```
If dynamic variables are required, they can be implemented using `MU_VALUESOURCE_CALLBACK`, where your application code supplies the current value dynamically via a read callback.

## OPC UA References
- OPC-10000-3 5.2.1 (Node definitions)
- OPC-10000-3 5.5.1 (Variable definitions)
- OPC-10000-3 5.6.2 (Variable Value definitions)
- OPC-10000-3 5.9 (Static Address Space Model)
