# Quickstart: mDNS Discovery

## Prerequisites

- Linux host with a working POSIX socket stack
- `avahi-browse` or `dns-sd` tool for verification

## Build

```bash
cmake -S . -B build -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build
```

## Verify No-Op (no regressions)

```bash
ctest --test-dir build --output-on-failure
# All existing tests must pass — zero regressions from adapter addition
```

## Verify Host mDNS Adapter

1. Build and run a server that configures the host mDNS adapter.
2. In another terminal: `avahi-browse -r _opcua-tcp._tcp`
3. Confirm the server appears with:
   - Port matching `endpoint_url`
   - TXT record containing `path=/discovery` and `applicationUri`
4. Stop the server — confirm the record disappears.

## Verify Disabled (NULL adapter)

Server init with `mdns_adapter = NULL` must operate normally with no mDNS
activity and all CTest tests passing.
