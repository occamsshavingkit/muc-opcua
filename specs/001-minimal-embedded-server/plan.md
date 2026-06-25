# Implementation Plan: Minimal Embedded Server

**Branch**: `001-minimal-embedded-server` | **Date**: 2026-06-25 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-minimal-embedded-server/spec.md`

## Summary

Build the first complete `micro-opcua` version as a freestanding C11 portable library with a host test harness, a Pico SDK adapter/example, an Arduino/PlatformIO adapter skeleton, and a minimal embedded OPC UA server example. The implementation targets OPC UA TCP over `opc.tcp` with OPC UA Binary encoding and remains `profile-targeting` until the Nano Embedded Device Server Profile requirements and conformance units are fully traced and tested.

The first server surface is intentionally narrow: endpoint discovery, OPC UA TCP connection negotiation, SecureChannel/session establishment required for browse/read, a tiny static address space, read-only scalar variables, and deterministic rejection of unsupported or malformed behavior. All wire-visible behavior must be tied to exact OPC UA sections and all memory used by the protocol hot path must be static or caller-provided.

## Technical Context

**Language/Version**: Freestanding C11, using a C99-compatible subset where practical; no C++ and no required hosted OS APIs in the core.

**Primary Dependencies**: CMake; Unity for small C unit tests; optional libFuzzer or AFL++ for host fuzzing; Pico SDK for first embedded integration; Arduino/PlatformIO adapter skeleton. No mandatory runtime dependency beyond a narrow platform adapter supplied by the application or embedded port.

**Storage**: No database or filesystem requirement. Server identity, address-space definitions, values, buffers, and session/channel storage are application-owned or statically allocated. Persistent storage is represented only by an adapter contract for future certificates or device identity.

**Testing**: Unity host tests, byte-fixture tests, integration tests through a host transport adapter, sanitizer builds where available, parser fuzz targets where available, `clang-format`, compiler warnings as errors, `cppcheck`, and `clang-tidy` where useful. Embedded validation includes at least one Pico SDK cross-compile.

**Target Platform**: Linux/POSIX host for development and CI; RP2040/Raspberry Pi Pico-class targets via Pico SDK first; Arduino-class boards via PlatformIO/Arduino adapter skeleton second.

**Project Type**: Portable embedded C library with examples, adapters, generated traceability docs, and test fixtures.

**Performance Goals**: Complete the minimal connect/discover/browse/read flow for one active client, one SecureChannel, and one Session without protocol hot-path heap allocation. Process malformed decoder input deterministically without unbounded recursion, unbounded loops, or buffer growth.

**Constraints**: Caller-provided buffers; default single client/channel/session; no subscriptions, writes, methods, history, alarms/events, PubSub, XML, JSON, HTTPS, WebSockets, dynamic address-space mutation, or companion-spec modeling in v1 unless Nano profile research proves a requirement. First-version scalar variable values are Boolean, Int32, UInt32, Float, and bounded String with a maximum encoded payload of 64 UTF-8 bytes. That 64-byte limit applies to bounded String Variable values; service/request string fields may use separate documented per-field limits. Bounded String Variable values exceeding their limit are strictly rejected rather than truncated, returning Bad_EncodingLimitsExceeded (malformed wire framing, such as truncated or negative lengths, remains Bad_DecodingError). For Browse requests requiring a continuation point, the server returns Bad_NoContinuationPoints. Only Anonymous user identity tokens are supported, and any other token type is rejected with Bad_IdentityTokenInvalid.

**Scale/Scope**: Tiny static address space suitable for an embedded device example: server root/object nodes plus a small application folder and a handful of read-only variable nodes. Static model size, reference count, and operation limits must be documented in generated traceability and size reports.

**OPC UA Normative References**:

- OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9 for AddressSpace NodeClass and Attribute requirements.
- OPC-10000-4 3.1.3, 5.5.1, 5.5.2, 5.5.4, 7.14 for DiscoveryEndpoint, Discovery, FindServers, GetEndpoints, and EndpointDescription.
- OPC-10000-4 5.6.2 and 5.6.3 for OpenSecureChannel and CloseSecureChannel.
- OPC-10000-4 5.7.2, 5.7.3, 5.7.4 for CreateSession, ActivateSession, and CloseSession.
- OPC-10000-4 5.9.2 for Browse, 5.9.2.4 for Browse operation-level StatusCodes, and 7.9 for ContinuationPoint parameters.
- OPC-10000-4 5.11.2 for Read and 5.11.2.3 for Read StatusCodes.
- OPC-10000-4 7.29, 7.32, 7.33, 7.38.2, 7.40.1, 7.40.3, and 7.41 for ReferenceDescription, request/response headers, common StatusCodes, identity tokens, and user token policy.
- OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.5, and 5.2.9 for OPC UA Binary encoding, String, NodeId, ExtensionObject, Variant, DataValue, array, and message body encoding.
- OPC-10000-6 6.7.1, 6.7.2, 6.7.3, 6.7.4, 6.7.7, 7.1.2.3, 7.1.2.4, 7.1.5, and 7.2 for UASC chunks, SecureChannel framing, Hello/Acknowledge, OPC UA TCP errors, and OPC UA TCP transport.
- OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, and 4.8 for facets, conformance units, profile categories, profile definitions, versioning, and applications.
- OPC-10000-14 5.1 for the PubSub overview, cited only to document PubSub as out-of-scope and to map PubSub requests to `Bad_ServiceUnsupported`.

**Target OPC UA Profile/Conformance Units**: Target the Nano Embedded Device Server Profile family, profile URIs `http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice` and `http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017`, and the transport profile URI `http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary`. The Micro Embedded Device profile is not the initial target because OPC Foundation profile metadata states that Micro builds on Nano and adds subscriptions. Exact mandatory facets and conformance units must be extracted from OPC-10000-7 and OPC Foundation profile metadata before any `profile-compliant` claim.

**Conformance Status Target**: `profile-targeting` for v1. The project must not claim `profile-compliant` or `CTT-verified` until all selected profile requirements are implemented, traced, tested, and independently verified.

## Embedded Size Budget

**Flash Impact**: Initial budget target is <= 128 KiB flash for the `micro-opcua` core plus the minimal example server on RP2040 release builds, excluding the board TCP/IP stack and unrelated application code. CI must produce a size report and fail or flag regressions once measurement infrastructure exists.

**RAM Impact**: Initial budget target is <= 32 KiB static plus peak stack for `micro-opcua`, including one connection/channel/session context and default RX/TX transport buffers, excluding platform TCP/IP buffers. The default example must document remaining RP2040 RAM headroom from the 264 KiB device RAM budget.

**Heap Use**: No mandatory heap allocation in the protocol hot path. The default example must run with zero `malloc` after initialization; optional cold-path allocation may be added only with a caller-provided allocator and a Complexity Tracking entry.

**Static Tables Added**: Generated or hand-maintained protocol/type tables are budgeted at <= 16 KiB flash in v1. Tables must be reproducible, auditable, size-reviewed, and traceable to OPC UA NodeIds/type definitions.

**Transport Buffers**: Caller-owned RX and TX buffers. Default example buffers are 8192 bytes RX and 8192 bytes TX to align with OPC UA TCP Hello/Acknowledge buffer negotiation requirements in OPC-10000-6 7.1.2.3 and 7.1.2.4. Smaller application-provided buffers are accepted only when validation confirms they remain legal for the advertised endpoint/security behavior.

**Crypto Dependency Impact**: No crypto backend linked by default for the v1 non-production SecurityPolicy None interoperability phase unless profile research requires otherwise. The public security adapter must make future mbedTLS, PSA Crypto, wolfSSL, or platform-provider integration possible without changing protocol state machines.

## Constitution Check

*GATE: Passed before Phase 0 research and re-checked after Phase 1 design using the final plan, research, data model, contracts, and generated tasks.*

| Principle | Status | Evidence |
|-----------|--------|----------|
| Spec Fidelity | Pass | Plan cites exact OPC-10000-3/4/6/7 sections for address-space, service, encoding, transport, StatusCode, and profile decisions. |
| Embedded C Core | Pass | C11 freestanding core with platform services behind adapters. |
| Memory Model | Pass | Protocol hot path uses caller-provided buffers and fixed contexts; no mandatory heap. |
| Minimal OPC UA Surface | Pass | Scope is OPC UA TCP, UASC, OPC UA Binary, Discovery, SecureChannel, Session, Browse, and Read only. |
| Profile Research | Pass with guard | Nano Embedded Device Server Profile is selected as the profile family to verify; status remains `profile-targeting` until conformance units are traced. |
| Correctness Gates | Pass | Plan requires test-first byte fixtures, boundary tests, malformed-input tests, state-machine tests, StatusCode tests, fuzz/property tests where available, and CI gates. |
| Security Honesty | Pass | SecurityPolicy None is documented as non-production or profile-permitted only; crypto remains pluggable. |
| Toolchain Discipline | Pass | CMake, Unity, fuzzing, static analysis, formatting, warnings-as-errors, and Pico SDK cross-compile are included. |
| Size Discipline | Pass | Initial flash/RAM/table/buffer/crypto budgets are stated and must be measured. |

## Project Structure

### Documentation (this feature)

```text
specs/001-minimal-embedded-server/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   |-- platform-adapter.md
|   |-- public-api.md
|   `-- testing-compliance.md
`-- tasks.md
```

### Source Code (repository root)

```text
CMakeLists.txt
cmake/
|-- MicroOpcUaOptions.cmake
|-- MicroOpcUaStaticAnalysis.cmake
|-- MicroOpcUaTraceability.cmake
`-- MicroOpcUaSizeReport.cmake
scripts/
|-- size-report.sh
`-- generate-traceability.sh
.github/
`-- workflows/
    `-- ci.yml
include/
`-- micro_opcua/
    |-- micro_opcua.h          # Umbrella public API
    |-- config.h               # Static limits and caller-owned memory contracts
    |-- status.h               # OPC UA StatusCode names/numeric mappings
    |-- types.h                # Public scalar, NodeId, Variant, and value types
    |-- server.h               # Server lifecycle/configuration API
    |-- address_space.h        # Static address-space API
    `-- platform.h             # Adapter interface declarations
src/
|-- core/                      # Server lifecycle, state machines, dispatch
|-- encoding/                  # OPC UA Binary reader/writer and fixtures
|-- services/                  # Discovery, SecureChannel, Session, Browse, Read
|-- address_space/             # Static model lookup and reference traversal
|-- security/                  # SecurityPolicy None and crypto adapter boundary
|-- platform/                  # Host-side adapter shims only
`-- generated/                 # Compact spec/type/status tables if used
platform/
|-- pico/                      # Pico SDK adapter and build integration
`-- arduino/                   # Arduino/PlatformIO adapter skeleton
examples/
`-- minimal_server/            # Host and Pico-oriented minimal server example
tests/
|-- unit/                      # Unity unit tests
|-- fixtures/                  # Wire bytes tied to OPC UA type/message sections
|-- fuzz/                      # libFuzzer/AFL++ decoder targets
`-- integration/               # Host transport/client flow tests
docs/
|-- adr/                       # Toolchain, profile, memory, security decisions
|-- conformance/               # Current conformance status and profile evidence
|-- size/                      # Flash/RAM reports
|-- validation/                # Recorded validation command outputs and results
`-- traceability/              # Generated implementation/test/spec mappings
```

**Structure Decision**: Use a single C library layout with public headers under `include/micro_opcua`, protocol implementation under `src`, board-specific integration under `platform`, and validation artifacts under `tests` and `docs`. The split keeps the portable core independent from Pico/Arduino I/O and makes traceability tables map implementation files directly to OPC UA sections.

## Complexity Tracking

No constitution violations are known at this planning stage. SecurityPolicy None remains limited to a documented non-production interoperability phase unless Nano profile evidence explicitly permits it.
