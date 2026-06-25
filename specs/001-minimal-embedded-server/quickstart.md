# Quickstart: Minimal Embedded Server

This quickstart describes the intended developer workflow for the first complete implementation. Commands become executable as the `/speckit-tasks` implementation creates the referenced build targets, examples, and tests.

## Prerequisites

- CMake 3.20 or newer.
- C11 compiler for host builds: Clang or GCC.
- Unity test dependency, vendored or fetched reproducibly by CMake.
- Optional Clang with sanitizer and libFuzzer support.
- Optional AFL++ for alternate fuzzing.
- `clang-format`, `cppcheck`, and optionally `clang-tidy`.
- Pico SDK for RP2040 cross-compile.
- PlatformIO for Arduino adapter skeleton validation.

## Configure Host Build

```sh
cmake -S . -B build/host \
  -DMICRO_OPCUA_BUILD_TESTS=ON \
  -DMICRO_OPCUA_BUILD_EXAMPLES=ON \
  -DMICRO_OPCUA_PLATFORM=host
```

## Build Host Targets

```sh
cmake --build build/host
```

## Run Host Tests

```sh
ctest --test-dir build/host --output-on-failure
```

Expected coverage:

- Binary encoder/decoder round trips and boundaries.
- Byte fixtures for OPC UA TCP, UASC chunks, service messages, NodeIds, Variants, DataValues, and StatusCodes.
- Malformed input and invalid state sequence tests.
- Host integration flow for connect, discover, browse, and read.

## Run Formatting and Static Analysis

```sh
cmake --build build/host --target format-check
cmake --build build/host --target cppcheck
cmake --build build/host --target clang-tidy
```

`clang-tidy` may be disabled for generated files or unavailable toolchains, but the skip must be documented by CI output.

## Build Sanitizer Variant

```sh
cmake -S . -B build/asan \
  -DMICRO_OPCUA_BUILD_TESTS=ON \
  -DMICRO_OPCUA_SANITIZERS=address,undefined \
  -DMICRO_OPCUA_PLATFORM=host
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

## Build Fuzz Targets

```sh
cmake -S . -B build/fuzz \
  -DMICRO_OPCUA_BUILD_FUZZERS=ON \
  -DMICRO_OPCUA_PLATFORM=host
cmake --build build/fuzz
```

Example fuzz target invocation:

```sh
build/fuzz/mu_fuzz_binary_reader -max_total_time=60
build/fuzz/mu_fuzz_chunk_parser -max_total_time=60
```

If libFuzzer is unavailable, CI must compile deterministic malformed-input tests and document the skipped fuzz target build.

## Run Minimal Host Server

```sh
build/host/examples/minimal_server/mu_minimal_server \
  --endpoint opc.tcp://127.0.0.1:4840/micro-opcua
```

Expected behavior:

- Advertises a single OPC UA TCP endpoint.
- Uses caller-provided RX/TX buffers.
- Supports one active client/channel/session.
- Exposes Boolean, Int32, UInt32, Float, and bounded String read-only variables.
- Rejects unsupported services with cited OPC UA StatusCodes.

## Cross-Compile Pico SDK Example

```sh
cmake -S . -B build/pico \
  -DMICRO_OPCUA_PLATFORM=pico \
  -DMICRO_OPCUA_BUILD_EXAMPLES=ON \
  -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build build/pico
```

Expected outputs:

- `micro_opcua` static library for Pico.
- Minimal Pico server example or adapter compile target.
- Size report under `docs/size/pico-minimal-server.md` once measurement is implemented.

## Validate Arduino/PlatformIO Adapter Skeleton

```sh
platformio run -d platform/arduino
```

Expected behavior:

- Adapter skeleton compiles or reports clearly documented unsupported networking portions.
- Portable core headers do not include Arduino-specific headers.

## Generate Traceability and Conformance Docs

```sh
cmake --build build/host --target traceability
cmake --build build/host --target conformance-docs
cmake --build build/host --target size-report
```

Required outputs:

- `docs/traceability/sections-to-files.md`
- `docs/traceability/files-to-sections.md`
- `docs/traceability/statuscodes.md`
- `docs/traceability/fixtures.md`
- `docs/traceability/profile-target.md`
- `docs/traceability/opcua-mcp-queries.md`
- `docs/conformance/status.md`
- `docs/conformance/profile-nano.md`
- `docs/conformance/identity-policy.md`
- `docs/size/pico-minimal-server.md`

## Inventory async-opcua Compliance Assets

```sh
gh repo clone occamsshavingkit/async-opcua ../async-opcua
find ../async-opcua/.devcontainer ../async-opcua/codegen-tests \
  ../async-opcua/dotnet-tests ../async-opcua/fuzz \
  ../async-opcua/schemas ../async-opcua/tools -maxdepth 2 -type f
```

Record the inventory in `docs/conformance/async-opcua-inventory.md`. Do not require these tests in micro-opcua CI until the host minimal server completes connect, discover, browse, and read.

## Release Readiness Checklist

- Host tests pass.
- Pico cross-compile passes.
- Default example size is within the current flash/RAM budgets or the plan documents why it changed.
- Traceability docs map implemented behavior and tests to exact OPC UA sections.
- Conformance docs state `profile-targeting`, `profile-compliant`, or `CTT-verified` accurately.
- SecurityPolicy None is labeled profile-permitted or non-production.
- Unsupported services and malformed inputs have deterministic tests.
