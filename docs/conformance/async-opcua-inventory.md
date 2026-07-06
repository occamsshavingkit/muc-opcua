#Async OPC - UA Inventory

Overview of the project areas, toolchains, and infrastructure that support
development, testing, and conformance of the micro-opcua library.

## .devcontainer

Defined in `.devcontainer/devcontainer.json`, the development container
provides a reproducible build environment (Debian Bookworm with C++ tools).
It provisions system dependencies (cmake, clang, gcc-arm-none-eabi,
cppcheck, clang-tidy, clang-format), the .NET 8 SDK for interop testing,
and the Python asyncua client for smoke testing. A prebuild step
(`updateContentCommand`) configures and compiles the host target so
Codespaces start warm.

## codegen-tests

The code generation infrastructure in `cmake/MucOpcUaCodegen.cmake` applies
size-oriented compile options (`-ffunction-sections`, `-fdata-sections`,
`-Os`) and linker flags (`--gc-sections`) so unreferenced code is stripped
from firmware builds. The `muc_opcua_apply_codegen` function is called for
the main library target and for platform targets (e.g., Pico RP2040). The
test suite indirectly validates these options by testing across profile
builds (embedded, micro, nano, standard).

## dotnet-tests

Located in `tests/interop/dotnet/`, these tests verify interoperability
between the micro-opcua server and the OPC Foundation .NET reference client.
The `run_interop_dotnet.sh` script starts the minimal server on port 4840,
builds and runs the .NET client from `tests/interop/dotnet/interop.csproj`,
and checks that the client can discover endpoints, browse the address space,
and read variables from the server.

## fuzz

Fuzz testing harnesses in `tests/fuzz/` exercise parsers with
libFuzzer-generated inputs. Harnesses cover: binary reader
(`fuzz_binary_reader`), message chunk parsing (`fuzz_message_chunk`),
binary type decoding (`fuzz_binary_types`), ExpandedNodeId parsing
(`fuzz_expanded_nodeid`), ActivateSession request handling
(`fuzz_activate_session`), subscription filter parsing
(`fuzz_subscription_filters`), event filter WhereClause parsing
(`fuzz_event_filter_where`), and Call request handling (`fuzz_call`).
All harnesses link against the main library with `-fsanitize=fuzzer`.

## schemas

OPC UA schema support covers the binary type schema (OPC-10000-6) used
for encoding and decoding service messages. The library includes built-in
type definitions for the standard OPC UA data types and relies on
spec-derived type tables for binary serialization. Schema adherence is
tested indirectly through the fuzz harnesses (which exercise the binary
reader/decoder) and through the interop tests (which validate wire-level
compatibility with reference clients).

## tools

Development and CI support tools include build scripts in `scripts/`
(check_build_matrix.sh, check_stack_usage.sh, measure_size.sh,
size-report.sh), CMake toolchain files for cross-compilation, static
analysis tooling (cppcheck, clang-tidy, clang-format configured via
`.clang-format` and `.clang-tidy`), and CI workflows in
`.github/workflows/`. The `Makefile` at the project root provides
convenience targets for compiling, testing, and linting across profiles.
