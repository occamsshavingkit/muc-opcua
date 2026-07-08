# Quickstart: Standard UA Client 2025 Profile

## Prerequisites

- Linux host with POSIX sockets
- A running OPC UA server (use the included server via `make standard` or similar)

## Build

```bash
# Standard client profile (includes all new features)
cmake -S . -B build_standard -DMUC_OPCUA_CLIENT_PROFILE=standard
cmake --build build_standard

# Nano client profile (unchanged baseline)
cmake -S . -B build_nano -DMUC_OPCUA_CLIENT_PROFILE=nano
cmake --build build_nano
```

## Verify Nano Backward Compatibility

```bash
ctest --test-dir build_nano --output-on-failure
# All nano client and server tests must pass
```

## Verify Standard Profile Build and Tests

```bash
ctest --test-dir build_standard --output-on-failure
# All existing server-side tests pass (zero regressions)
# New client subscription/write/call/event tests pass
```

## Manual Verification

1. Start a server that supports subscriptions.
2. Run a standard-profile test client that:
   - Creates a subscription
   - Adds a monitored item for a known counter variable
   - Verifies data change notifications arrive as the counter increments
   - Writes a new value to a writable variable
   - Calls a method on the server
   - Subscribes to events and verifies event notifications arrive

## Expected Outcomes

- Subscription notifications are received within one publishing interval
- Write returns `Good` and a subsequent Read confirms the new value
- Method call returns output arguments matching server state
- Event notifications contain the fields specified in the SelectClause
- Nano profile build and tests are unaffected
