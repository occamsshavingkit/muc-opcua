# Verification: OPC UA Conformance and Size Repairs

## Pre-Change Audit Baselines

| Profile | ARM archive text bytes |
|---------|------------------------|
| nano | 16,653 |
| micro | 23,962 |
| embedded | 47,126 |
| full | 47,400 |

| RAM Proxy | Host bytes |
|-----------|------------|
| `struct mu_server` nano | 944 |
| `struct mu_server` micro | 3,048 |
| `struct mu_server` embedded | 69,344 |
| `struct mu_server` full | 77,832 |
| `mu_monitored_item_t` embedded/full | 392 |
| `mu_connection_t` embedded/full | 3,376 |

## Planned Commands

| Command | Purpose | OPC UA reference |
|---------|---------|------------------|
| `cmake -S . -B build/019-tests -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON -DMICRO_OPCUA_STACK_USAGE=ON -DCMAKE_BUILD_TYPE=Debug` | Configure host tests and stack usage instrumentation | OPC-10000-7 §4.3 |
| `cmake --build build/019-tests -j` | Build protocol tests | OPC-10000-4 §7.38.2; OPC-10000-4 §7.22.4; OPC-10000-4 §5.5.4.2 |
| `ctest --test-dir build/019-tests --output-on-failure` | Run unit and integration tests | OPC-10000-4 §7.38.2; OPC-10000-4 §7.22.4; OPC-10000-4 §5.5.4.2 |
| `scripts/measure_size.sh all` | Measure profile archive text sizes | OPC-10000-7 §4.3 |
| `bash scripts/check_stack_usage.sh --build-dir tests/fixtures/stack_usage/missing-root --threshold 10240` | Verify missing optimized-away root frame handling | OPC-10000-7 §4.3 |

## Stack-Budget Method

Compiler `.su` output reports emitted stack frames, not an abstract source-level call graph. A missing internal static helper frame may mean the compiler inlined or optimized it away. The stack-budget gate remains claim-honest under OPC-10000-7 §4.3 by enforcing thresholds on emitted frames and by documenting when an internal frame is omitted rather than fabricated.

## Results

### Configure, Build, and Test

| Command | Result |
|---------|--------|
| `cmake -S . -B build/019-tests -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON -DMICRO_OPCUA_STACK_USAGE=ON -DCMAKE_BUILD_TYPE=Debug` | PASS |
| `cmake --build build/019-tests -j` | PASS |
| `ctest --test-dir build/019-tests --output-on-failure` | PASS: 96/96 tests |

The passing suite covers the repaired StatusCode constants, AggregateFilter
encoding and aggregate function NodeIds, GetEndpoints `profileUris[]` filtering,
integer-only session timeout revision, documentation guardrails, and the stack
budget check.

### Post-Change ROM Measurements

`scripts/measure_size.sh all`:

| Profile | Baseline ARM archive text bytes | Post-change bytes | Delta |
|---------|---------------------------------|-------------------|-------|
| nano | 16,653 | 16,919 | +266 |
| micro | 23,962 | 24,228 | +266 |
| embedded | 47,126 | 47,400 | +274 |
| full-featured | 47,400 | 47,678 | +278 |

The OPC UA conformance repairs add 266 bytes of ARM text in each profile. This
is recorded as a ROM cost for corrected protocol behavior, not a ROM reduction.
The project remains profile-targeting per OPC-10000-7 §4.3; no externally
certified profile claim is made.

### Post-Change RAM Proxies

Host struct-size probes using the profile macros from `CMakeLists.txt`:

| RAM Proxy | Baseline host bytes | Post-change host bytes | Delta |
|-----------|---------------------|------------------------|-------|
| `struct mu_server` nano | 944 | 944 | 0 |
| `struct mu_server` micro | 3,048 | 3,048 | 0 |
| `struct mu_server` embedded | 69,344 | 89,120 | +19,776 |
| `struct mu_server` full | 77,832 | 97,608 | +19,776 |
| `mu_monitored_item_t` embedded/full | 392 | 392 | 0 |
| `mu_connection_t` embedded/full | 3,376 | 8,560 | +5,184 |

The post-review RAM increase comes from enforcing `MU_CONNECTION_RX_BUFFER_SIZE`
to be at least `MU_MIN_CHUNK_SIZE`. OPC-10000-6 §7.1.2.3 and §7.1.2.4 require
an 8192-byte receive-buffer negotiation floor for the non-ECC transport profiles
used by this project; advertising 8192 while backing each connection with a
smaller buffer would allow a valid negotiated chunk to disconnect the client.
Reducing this hotspot requires a separate design such as sharing a partial
receive buffer across connection slots or lowering `MU_MAX_CONNECTIONS` as an
explicit profile capacity choice.

### Stack Budget

`bash scripts/check_stack_usage.sh --build-dir tests/fixtures/stack_usage/missing-root --threshold 10240`:

| Metric | Result |
|--------|--------|
| Secured OPN stack estimate | 896 bytes |
| Threshold | 10,240 bytes |
| Missing internal root frame | Reported as `0 (not emitted)` |
| Gate result | PASS |

### Stale Reference Scan

The broad stale-reference scan from T044 returned only intentional 019 feature
guardrail/task/contract text. The focused legacy cleanup scan over
`specs/018-aggregate-subscriptions` and `docs` returned no matches.
