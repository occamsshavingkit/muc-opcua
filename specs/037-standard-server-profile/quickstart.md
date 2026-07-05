# Quickstart: Building and Testing the Standard Profile

**Feature**: 037-standard-server-profile | **Date**: 2026-07-05

## Prerequisites

- CMake >= 3.16
- GCC or Clang (host build)
- Optional: Pico SDK for RP2040 cross-compile
- Optional: Arduino CLI for Arduino target

## Building the Standard Profile

```bash
# Host build (default: full profile, all features)
make full
ctest --test-dir build/test --output-on-failure

# Standard profile build (new target)
cmake -B build/standard -DMUC_OPCUA_PROFILE=standard
cmake --build build/standard

# Or with individual facet control:
cmake -B build/custom \
  -DMUC_OPCUA_DATA_ACCESS=ON \
  -DMUC_OPCUA_METHOD_SERVER=ON \
  -DMUC_OPCUA_EVENT_FILTER_WHERE=ON \
  -DMUC_OPCUA_AUDITING=ON \
  -DMUC_OPCUA_COMPLEX_TYPES=ON \
  -DMUC_OPCUA_SERVER_DIAGNOSTICS=ON \
  -DMUC_OPCUA_REDUNDANCY=ON \
  -DMUC_OPCUA_AGGREGATE_FULL=ON \
  -DMUC_OPCUA_REVERSE_CONNECT=ON \
  -DMUC_OPCUA_TIME_SYNC=ON
cmake --build build/custom
```

## Running Tests

```bash
# Unit tests for all new facets
ctest --test-dir build/test --output-on-failure -R "test_percent_deadband"
ctest --test-dir build/test --output-on-failure -R "test_method_call_arbitrary"
ctest --test-dir build/test --output-on-failure -R "test_event_filter_where"
ctest --test-dir build/test --output-on-failure -R "test_audit_events"
ctest --test-dir build/test --output-on-failure -R "test_complex_types"
ctest --test-dir build/test --output-on-failure -R "test_diagnostics"
ctest --test-dir build/test --output-on-failure -R "test_aggregate_full"
ctest --test-dir build/test --output-on-failure -R "test_transfer_subscriptions"
ctest --test-dir build/test --output-on-failure -R "test_reverse_connect"

# Full suite
ctest --test-dir build/test --output-on-failure

# With sanitizers
cmake -B build/asan -DMUC_OPCUA_SANITIZERS=address,undefined
cmake --build build/asan
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan/test --output-on-failure
```

## Registering a Custom Method

```c
#include <muc_opcua/services/method.h>

static mu_status_t my_echo_method(mu_method_call_t *call) {
    // Echo input argument to output
    call->output_arguments[0] = call->input_arguments[0];
    *call->callback_status = MU_STATUS_GOOD;
    return MU_STATUS_GOOD;
}

void setup_methods(mu_server_t *server) {
    mu_argument_t inputs[] = {
        {.name = "Input", .type = MU_TYPE_STRING}
    };
    mu_argument_t outputs[] = {
        {.name = "Output", .type = MU_TYPE_STRING}
    };

    mu_server_register_method(server,
        MU_NODE_ID(1, 1001),  // MyMethod NodeId
        my_echo_method,
        inputs, 1,
        outputs, 1
    );
}
```

## Registering a Custom Structure Type

```c
#include <muc_opcua/address_space/complex_types.h>

void setup_types(mu_address_space_t *addr) {
    mu_structure_field_t point3d_fields[] = {
        {.name = "X", .data_type = MU_NODE_ID_NUMERIC(0, 11), .value_rank = -1},
        {.name = "Y", .data_type = MU_NODE_ID_NUMERIC(0, 11), .value_rank = -1},
        {.name = "Z", .data_type = MU_NODE_ID_NUMERIC(0, 11), .value_rank = -1},
    };

    mu_structure_definition_t point3d_def = {
        .default_encoding_id = MU_NODE_ID(1, 2001),
        .base_data_type = MU_NODE_ID_NUMERIC(0, 22),  // Structure
        .structure_type = MU_STRUCTURE_TYPE_STRUCTURE,
        .fields = point3d_fields,
        .field_count = 3,
    };

    mu_register_structure_type(addr, MU_NODE_ID(1, 2000), &point3d_def);
}
```

## Profile Verification

To verify the Standard profile is correctly advertised:

```bash
# Build and run the minimal server with standard profile
cmake -B build/standard -DMUC_OPCUA_PROFILE=standard
cmake --build build/standard
./build/standard/examples/minimal_server &

# Use an OPC UA client to check ServerProfileArray
# Expected: "http://opcfoundation.org/UA-Profile/Server/StandardUA2017"
```

## Existing Profile Regression Check

```bash
# Verify nano/micro/embedded still build and pass
make nano && ctest --test-dir build/nano/test --output-on-failure
make micro && ctest --test-dir build/micro/test --output-on-failure
make embedded && ctest --test-dir build/embedded/test --output-on-failure

# Verify size budgets
arm-none-eabi-size build/nano/examples/minimal_server.elf
arm-none-eabi-size build/micro/examples/minimal_server.elf
arm-none-eabi-size build/embedded/examples/minimal_server.elf
arm-none-eabi-size build/standard/examples/minimal_server.elf
```
