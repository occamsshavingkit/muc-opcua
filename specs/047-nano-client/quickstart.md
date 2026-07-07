# Quickstart: Nano Client

## Prerequisites

- CMake 3.15+, C11 compiler, POSIX system
- Build the server: `cmake -S . -B build -DMUC_OPCUA_PROFILE=standard && cmake --build build`

## Build the Client

```bash
cmake -S . -B build_client \
  -DMUC_OPCUA_CLIENT_PROFILE=nano \
  -DMUC_OPCUA_PROFILE=nano \
  && cmake --build build_client
```

This builds the library with nano client + nano server (smallest combined size).

## Run Tests

```bash
# Start a server in the background for interop tests
./build/bin/muc-opcua-server --port 4840 &
SERVER_PID=$!
sleep 1

# Run client tests
ctest --test-dir build_client -R test_nano_client

# Cleanup
kill $SERVER_PID
```

## Expected Output

```
test_nano_client_connect:      PASS
test_nano_client_get_endpoints: PASS
test_nano_client_create_session: PASS
test_nano_client_activate_session: PASS
test_nano_client_read:         PASS
test_nano_client_browse:       PASS
test_nano_client_browse_next:  PASS
test_nano_client_translate_path: PASS
test_nano_client_close_session: PASS
test_nano_client_disconnect:   PASS
test_nano_client_errors:       PASS
```

## Interop Verification

```bash
# Against the .NET reference server
# (requires OPC UA .NET client stack installed)
ctest --test-dir build -R interop_nano_client
```

## Size Measurement

```bash
cmake -S . -B build_size \
  -DMUC_OPCUA_CLIENT_PROFILE=nano \
  -DMUC_OPCUA_PROFILE=nano \
  && cmake --build build_size
arm-none-eabi-size build_size/lib/libmuc-opcua.a
```
