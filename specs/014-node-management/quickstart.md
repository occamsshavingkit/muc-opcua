# Quickstart: NodeManagement

To enable NodeManagement services in your micro-opcua application:

1. **Enable the CMake feature**:
   ```bash
   cmake -B build -DMICRO_OPCUA_SERVICE_NODEMANAGEMENT=ON
   ```

2. **Configure capacities**:
   Optionally, override the default dynamic limits in your build:
   ```bash
   cmake -B build -DMICRO_OPCUA_SERVICE_NODEMANAGEMENT=ON \
     -DMU_MAX_DYNAMIC_NODES=64 \
     -DMU_MAX_DYNAMIC_REFERENCES=128
   ```

3. **Initialize the Server**:
   The memory for dynamic nodes is automatically drawn from the server's pre-allocated storage block during `mu_server_init`. Clients with an administrative access role can now use standard OPC UA clients (like UaExpert) to Add/Delete nodes dynamically.
