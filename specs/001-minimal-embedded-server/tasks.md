# Tasks: Minimal Embedded Server

**Input**: Design documents from `/specs/001-minimal-embedded-server/`
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: Tests are mandatory for protocol parsing, serialization, service dispatch, StatusCodes, SecureChannel/session state, address-space behavior, security behavior, and every wire-visible change. Protocol test tasks appear immediately before the implementation tasks they validate.

**Task Size Rule**: Each task is scoped so the resulting diff should be reviewable as a standalone PR. If a task expands beyond the named file path plus directly required CMake wiring, split it before implementation.

**OPC Reference Rule**: Each task line ends with `OPC refs:`. Protocol, service, encoding, StatusCode, transport, security, and conformance tasks list the exact OPC UA section(s) to use; pure build, tooling, size, or validation-runner tasks use an explicit `N/A` rationale.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the build, test, analysis, and repository scaffolding used by every story.

- [X] T001 Create root CMake project with `MICRO_OPCUA_BUILD_TESTS`, `MICRO_OPCUA_BUILD_EXAMPLES`, `MICRO_OPCUA_BUILD_FUZZERS`, `MICRO_OPCUA_SANITIZERS`, and `MICRO_OPCUA_PLATFORM` options in CMakeLists.txt (OPC refs: N/A - build infrastructure only)
- [X] T002 Create reusable CMake option helpers for host, pico, and arduino-skeleton builds in cmake/MicroOpcUaOptions.cmake (OPC refs: N/A - build infrastructure only)
- [X] T003 Create warning and C11 standard configuration with warnings-as-errors support in cmake/MicroOpcUaWarnings.cmake (OPC refs: N/A - C toolchain configuration only)
- [X] T004 [P] Create clang-format configuration for C headers, C sources, CMake, and Markdown in .clang-format (OPC refs: N/A - formatting infrastructure only)
- [X] T005 [P] Create static-analysis CMake targets for format-check (clang-format), cppcheck, and optional clang-tidy in cmake/MicroOpcUaStaticAnalysis.cmake (OPC refs: N/A - static-analysis infrastructure only)
- [X] T006 Create top-level test registration for Unity, integration tests, fixtures, and fuzz targets in tests/CMakeLists.txt (OPC refs: N/A - test infrastructure only)
- [X] T007 Create Unity unit-test target skeleton and test discovery rules in tests/unit/CMakeLists.txt (OPC refs: N/A - test infrastructure only)
- [X] T008 [P] Create shared Unity configuration for host tests in tests/support/unity_config.h (OPC refs: N/A - test infrastructure only)
- [X] T009 [P] Create CI workflow skeleton for host build, tests, static analysis, sanitizer, fuzz compile, and Pico cross-compile in .github/workflows/ci.yml (OPC refs: N/A - CI orchestration only)
- [X] T010 [P] Create developer build target summary matching quickstart.md in docs/adr/0001-build-and-test-workflow.md (OPC refs: N/A - developer workflow documentation only)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish traceability, conformance status, status-code records, and size tracking before user-story code starts.

**CRITICAL**: No user-story implementation begins until this phase is complete.

- [X] T011 Create OPC UA section traceability skeleton with Part 3/4/6/7 anchors from plan.md in docs/traceability/sections-to-files.md (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9; OPC-10000-4 5.5.1, 5.5.2, 5.5.4, 5.6.2, 5.6.3, 5.7.2, 5.7.3, 5.7.4, 5.9.2, 5.11.2, 7.29, 7.32, 7.33, 7.38.2, 7.40.1, 7.40.3, 7.41; OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.5, 5.2.9, 6.7.1, 6.7.2, 6.7.3, 6.7.4, 6.7.7, 7.1.2.3, 7.1.2.4, 7.1.5, 7.2; OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, 4.8)
- [X] T012 [P] Create reverse artifact-to-section traceability skeleton in docs/traceability/files-to-sections.md (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9; OPC-10000-4 5.5.1, 5.5.2, 5.5.4, 5.6.2, 5.6.3, 5.7.2, 5.7.3, 5.7.4, 5.9.2, 5.11.2, 7.29, 7.32, 7.33, 7.38.2, 7.40.1, 7.40.3, 7.41; OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.5, 5.2.9, 6.7.1, 6.7.2, 6.7.3, 6.7.4, 6.7.7, 7.1.2.3, 7.1.2.4, 7.1.5, 7.2; OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, 4.8)
- [X] T013 [P] Create StatusCode traceability skeleton for Bad_ServiceUnsupported and TCP errors in docs/traceability/statuscodes.md (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 7.1.5)
- [X] T014 [P] Create fixture traceability skeleton and sidecar rules in docs/traceability/fixtures.md (OPC refs: OPC-10000-6 5.2.9, 6.7.2; fixture-specific service sections supplied per fixture)
- [X] T015 Create Nano Embedded Device Server Profile target record and profile-targeting guard in docs/conformance/profile-nano.md (OPC refs: OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, 4.8)
- [X] T016 [P] Create OPC UA MCP query provenance ledger for profile, service, encoding, transport, StatusCode, and conformance decisions in docs/traceability/opcua-mcp-queries.md (OPC refs: OPC-10000-4 5.5.1, 5.5.2, 5.5.4, 5.6.2, 5.6.3, 5.7.2, 5.7.3, 5.7.4, 5.9.2, 5.11.2, 7.38.2; OPC-10000-6 5.2.1, 6.7.2, 7.1.2.3, 7.1.2.4, 7.2; OPC-10000-7 4.2)
- [X] T017 Create profile-derived identity-token policy evidence before endpoint/session implementation in docs/conformance/identity-policy.md (OPC refs: OPC-10000-4 5.7.3.2, 7.40.1, 7.40.3, 7.41; OPC-10000-7 4.2)
- [X] T018 [P] Create initial conformance status document labeled profile-targeting and not CTT-verified in docs/conformance/status.md (OPC refs: N/A - QA infrastructure only)
- [X] T019 [P] Create initial SecurityPolicy None non-production notice and crypto-pluggability note in docs/conformance/security.md (OPC refs: OPC-10000-7 4.2)
- [X] T020 [P] Create services support matrix for Discovery, SecureChannel, Session, Browse, Read, and unsupported services in docs/conformance/services.md (OPC refs: OPC-10000-4 5.5.1, 5.6.2, 5.7.2, 5.9.2, 5.11.2, 5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.9.2.4, 5.9.3.2, 5.9.3.4, 5.9.4.2, 5.9.4.4, 5.9.5.2, 5.9.5.3, 5.11.2.2, 5.11.2.3, 5.11.3.2, 5.11.3.4, 5.11.4.2, 5.11.4.4, 5.12.2.2, 5.12.2.4, 5.14.2.2, 5.14.5.2, 5.14.5.4, 7.38.2; OPC-10000-6 5.3, 5.4.1, 7.4, 7.5.1)
- [X] T021 Create size-budget ledger with 128 KiB flash and 32 KiB RAM budgets from plan.md in docs/size/feature-size-ledger.md (OPC refs: N/A - size-budget tracking only)
- [X] T022 Create reproducible size-report helper invoked by CMake targets in scripts/size-report.sh (OPC refs: N/A - size-report tooling only)
- [X] T023 [P] Create fixture directory README with byte-fixture, sidecar, and OPC UA citation rules in tests/fixtures/README.md (OPC refs: OPC-10000-6 5.2.9, 6.7.2; fixture-specific service sections supplied per fixture)

**Checkpoint**: Foundation ready - user-story tasks can start.

---

## Phase 3: User Story 1 - Build and Verify the Portable Core (Priority: P1) [MVP]

**Goal**: A maintainer can build and test the portable C core on host and cross-compile the core plus minimal example skeleton for RP2040/Pico without OS or heap dependency in the protocol hot path.

**Independent Test**: Configure `build/host`, run Unity/CTest smoke tests, configure the Pico SDK build, and prove public headers do not expose POSIX, Pico, Arduino, socket, or TLS types.

### Tests for User Story 1

- [X] T024 [P] [US1] Add public-header compilation smoke test including every public header in tests/unit/test_public_headers.c (OPC refs: N/A - public-header compile coverage only)
- [X] T025 [P] [US1] Add server configuration validation tests for null buffers, invalid endpoint scheme, and default one-client limits in tests/unit/test_server_config.c (OPC refs: OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [X] T026 [P] [US1] Add no-heap lifecycle test that wraps malloc/free and validates init/poll/close do not allocate after init in tests/unit/test_no_heap_lifecycle.c (OPC refs: N/A - embedded memory-discipline test only)
- [X] T027 [P] [US1] Add platform adapter contract tests for null optional persistence and crypto callbacks in tests/unit/test_platform_adapter_contract.c (OPC refs: N/A - platform adapter contract test only)
- [X] T028 [P] [US1] Add status-name unit tests for Good, Bad_ServiceUnsupported, Bad_DecodingError, and TCP busy mappings in tests/unit/test_status.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 7.1.5)
- [X] T029 [P] [US1] Add Pico public-header compile smoke target that includes only portable headers in platform/pico/test_public_headers.c (OPC refs: N/A - embedded public-header compile coverage only)

### Implementation for User Story 1

- [X] T030 [US1] Define public compile-time limits and `MU_SERVER_STORAGE_BYTES` in include/micro_opcua/config.h (OPC refs: OPC-10000-6 7.1.2.3, 7.1.2.4)
- [X] T031 [US1] Define bytes, string, NodeId, value, and size-report public types in include/micro_opcua/types.h (OPC refs: OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.16, 5.2.2.17)
- [X] T032 [US1] Define public OPC UA StatusCode constants and status helper prototypes in include/micro_opcua/status.h (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 7.1.5)
- [X] T033 [US1] Define TCP, time, entropy, persistence, and crypto adapter interfaces in include/micro_opcua/platform.h (OPC refs: OPC-10000-4 5.6.2.2; OPC-10000-6 7.2)
- [X] T034 [US1] Define server config, storage, lifecycle, and validation APIs in include/micro_opcua/server.h (OPC refs: OPC-10000-4 5.6.2.2, 5.7.2.2; OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [X] T035 [US1] Create umbrella public include that exposes config, types, status, platform, address-space, and server headers in include/micro_opcua/micro_opcua.h (OPC refs: N/A - header aggregation only)
- [X] T036 [US1] Implement StatusCode constants and `mu_status_name` in src/core/status.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 7.1.5)
- [X] T037 [US1] Implement server config validation for endpoint scheme, caller buffers, limits, and adapter pointers in src/core/server.c (OPC refs: OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [X] T038 [US1] Implement fixed-storage server init, bounded poll stub, and close without hot-path heap allocation in src/core/server.c (OPC refs: N/A - embedded lifecycle and memory discipline only)
- [X] T039 [US1] Add portable core library target and public include directories in CMakeLists.txt (OPC refs: N/A - build infrastructure only)
- [X] T040 [US1] Add host test support fake platform callbacks for TCP/time/entropy in tests/support/fake_platform.c (OPC refs: N/A - host test support only)
- [X] T041 [US1] Add minimal host example skeleton using caller-owned buffers and no network behavior yet in examples/minimal_server/main.c (OPC refs: N/A - example skeleton without protocol behavior)
- [X] T042 [US1] Add example build registration for the host minimal server in examples/CMakeLists.txt (OPC refs: N/A - build infrastructure only)
- [X] T043 [US1] Add Pico SDK library/example CMake skeleton that links the portable core in platform/pico/CMakeLists.txt (OPC refs: N/A - embedded build skeleton only)
- [X] T044 [US1] Add Pico adapter skeleton with no public SDK type leakage in platform/pico/mu_pico_adapter.c (OPC refs: N/A - platform adapter skeleton only)
- [X] T045 [US1] Add PlatformIO project skeleton for Arduino adapter builds in platform/arduino/platformio.ini (OPC refs: N/A - embedded build skeleton only)
- [X] T046 [US1] Add Arduino adapter skeleton preserving the C adapter contract in platform/arduino/src/mu_arduino_adapter.c (OPC refs: N/A - platform adapter skeleton only)
- [X] T047 [US1] Update traceability rows for US1 public headers, server lifecycle, platform adapters, and tests in docs/traceability/files-to-sections.md (OPC refs: OPC-10000-4 5.6.2.2, 5.7.2.2, 7.38.2; OPC-10000-6 7.1.2.3, 7.1.2.4, 7.1.5, 7.2)
- [X] T048 [US1] Record US1 size impact for public headers, storage, examples, and adapter skeletons in docs/size/feature-size-ledger.md (OPC refs: N/A - size-impact tracking only)

**Checkpoint**: US1 is complete when host tests pass, the minimal host example builds, and the Pico/Arduino skeleton compile targets configure.

---

## Phase 4: User Story 2 - Configure a Tiny Static Address Space (Priority: P2)

**Goal**: A firmware developer can configure server identity, endpoint URL, caller-owned buffers, static Object/Variable nodes, read-only fixed values, and read callbacks.

**Independent Test**: A host unit test initializes a server with an application-owned static address space and verifies Boolean, Int32, UInt32, Float, and bounded String values are available without ownership transfer or mutation.

### Tests for User Story 2

- [X] T049 [P] [US2] Add address-space validation tests for duplicate NodeIds and unresolved references in tests/unit/test_address_space_validation.c (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2; OPC-10000-6 5.2.2.9)
- [X] T050 [P] [US2] Add static value-source tests for Boolean, Int32, UInt32, Float, empty String, and 64-byte String in tests/unit/test_address_space_values.c (OPC refs: OPC-10000-3 5.9; OPC-10000-6 5.2.1, 5.2.2.4)
- [X] T051 [P] [US2] Add callback value-source lifetime and StatusCode propagation tests in tests/unit/test_address_space_callbacks.c (OPC refs: OPC-10000-4 5.11.2.3, 7.38.2)
- [X] T052 [P] [US2] Add over-limit bounded String validation test for 65-byte UTF-8 payloads expecting Bad_EncodingLimitsExceeded in tests/unit/test_address_space_string_limits.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 5.2.2.4)
- [X] T053 [P] [US2] Add server init test using application-owned address-space storage and caller-owned buffers in tests/unit/test_server_address_space_config.c (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2)

### Implementation for User Story 2

- [X] T054 [US2] Define public address-space node, reference, value-source, and callback types in include/micro_opcua/address_space.h (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9; OPC-10000-6 5.2.2.9)
- [X] T055 [US2] Add address-space declarations to the umbrella include in include/micro_opcua/micro_opcua.h (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9)
- [X] T056 [US2] Implement NodeId equality, namespace bounds, and lookup helpers in src/address_space/node_id.c (OPC refs: OPC-10000-6 5.2.2.9)
- [X] T057 [US2] Implement static address-space validation for duplicate nodes and invalid references in src/address_space/address_space.c (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2)
- [X] T058 [US2] Implement fixed value-source reads for Boolean, Int32, UInt32, Float, and bounded String in src/address_space/value_source.c (OPC refs: OPC-10000-3 5.9; OPC-10000-6 5.2.1, 5.2.2.4)
- [X] T059 [US2] Implement callback value-source reads with caller-owned lifetime rules in src/address_space/value_source.c (OPC refs: OPC-10000-4 5.11.2.3, 7.38.2)
- [X] T060 [US2] Implement 64-byte bounded String validation returning Bad_EncodingLimitsExceeded for over-limit values in src/address_space/value_source.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 5.2.2.4)
- [X] T061 [US2] Wire address-space sources into the core library target in CMakeLists.txt (OPC refs: N/A - build infrastructure only)
- [X] T062 [US2] Add reusable tiny static address-space definition for examples and integration tests in examples/minimal_server/static_address_space.c (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9)
- [X] T063 [US2] Update minimal server example to pass server identity, endpoint, buffers, and static address space in examples/minimal_server/main.c (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2; OPC-10000-6 7.2)
- [X] T064 [US2] Document address-space configuration and memory lifetimes in examples/minimal_server/README.md (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9)
- [X] T065 [US2] Update traceability rows for OPC-10000-3 5.2.1, 5.5.1, 5.6.2, and 5.9 address-space behavior in docs/traceability/sections-to-files.md (OPC refs: OPC-10000-3 5.2.1, 5.5.1, 5.6.2, 5.9)
- [X] T066 [US2] Record US2 static node, reference, value, and callback memory impact in docs/size/feature-size-ledger.md (OPC refs: N/A - size-impact tracking only)

**Checkpoint**: US2 is complete when a host test initializes the server with a tiny static address space and validates all five v1 scalar read-only value kinds.

---

## Phase 5: User Story 3 - Connect, Discover, Browse, and Read (Priority: P3)

**Goal**: An OPC UA client can connect over OPC UA TCP, complete the required SecureChannel/Session flow for the profile-targeting phase, discover the endpoint, browse the static address space, and read configured scalar values.

**Independent Test**: A host integration test drives Hello/Acknowledge, OpenSecureChannel, CreateSession, ActivateSession, GetEndpoints/FindServers, Browse, and Read against the minimal server with byte fixtures tied to OPC UA sections.

### Tests and Fixtures for User Story 3

- [X] T067 [P] [US3] Add Binary primitive round-trip tests for Boolean, Int32, UInt32, Float, UInt32 lengths, and StatusCode in tests/unit/test_binary_primitives.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 5.2.1)
- [X] T068 [P] [US3] Add Binary String tests for null, empty, 64-byte, and over-limit values (over-limit expecting Bad_EncodingLimitsExceeded) in tests/unit/test_binary_string.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 5.2.2.4)
- [X] T069 [P] [US3] Add Binary NodeId tests for numeric and string variants in tests/unit/test_binary_nodeid.c (OPC refs: OPC-10000-6 5.2.2.9)
- [X] T070 [P] [US3] Add ExtensionObject header tests in tests/unit/test_binary_extension_object.c (OPC refs: OPC-10000-6 5.2.2.15)
- [X] T071 [P] [US3] Add Variant and DataValue tests for Boolean, Int32, UInt32, Float, and String in tests/unit/test_binary_variant_datavalue.c (OPC refs: OPC-10000-6 5.2.2.16, 5.2.2.17)
- [X] T072 [P] [US3] Add Hello/Acknowledge byte fixtures and sidecars in tests/fixtures/opcua-tcp/hello_ack.md (OPC refs: OPC-10000-6 7.1.2.3, 7.1.2.4)
- [X] T073 [P] [US3] Add MessageChunk header and sequence header fixtures in tests/fixtures/uasc/message_chunk.md (OPC refs: OPC-10000-6 6.7.2, 6.7.2.4)
- [X] T074 [P] [US3] Add OpenSecureChannel SecurityPolicy None fixtures in tests/fixtures/services/open_secure_channel.md (OPC refs: OPC-10000-4 5.6.2.2; OPC-10000-6 6.7.4, 6.7.7)
- [X] T075 [P] [US3] Add FindServers and GetEndpoints fixtures in tests/fixtures/services/discovery.md (OPC refs: OPC-10000-4 5.5.2.2, 5.5.4.2)
- [X] T076 [P] [US3] Add DiscoveryEndpoint no-session unit tests for FindServers and GetEndpoints in tests/unit/test_discovery_endpoint.c (OPC refs: OPC-10000-4 3.1.3, 5.5.1, 5.5.2.2, 5.5.4.2)
- [X] T077 [P] [US3] Add host integration test proving FindServers and GetEndpoints work before CreateSession/ActivateSession in tests/integration/test_discovery_endpoint_no_session.c (OPC refs: OPC-10000-4 3.1.3, 5.5.1, 5.5.2.2, 5.5.4.2)
- [X] T078 [P] [US3] Add CreateSession, ActivateSession, and CloseSession fixtures in tests/fixtures/services/session.md (OPC refs: OPC-10000-4 5.7.2.2, 5.7.3.2, 5.7.4.2)
- [X] T079 [P] [US3] Add Browse fixture for the default static address space in tests/fixtures/services/browse.md (OPC refs: OPC-10000-4 5.9.2.2, 7.29)
- [X] T080 [P] [US3] Add Browse limits tests for requestedMaxReferencesPerNode, response-size bounds, and no continuation points in tests/unit/test_browse_limits.c (OPC refs: OPC-10000-4 5.9.2.2, 5.9.2.4, 7.9)
- [X] T081 [P] [US3] Add BrowseNext unsupported-service fixture expecting Bad_ServiceUnsupported in tests/fixtures/services/browse_next_unsupported.md (OPC refs: OPC-10000-4 5.9.3.2, 5.9.3.4, 7.38.2)
- [X] T082 [P] [US3] Add Read fixtures for Boolean, Int32, UInt32, Float, and bounded String in tests/fixtures/services/read.md (OPC refs: OPC-10000-4 5.11.2.2, 5.11.2.3)
- [X] T083 [P] [US3] Add TCP connection state tests for Hello/Acknowledge negotiation and 8192-byte default buffers in tests/unit/test_tcp_connection.c (OPC refs: OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [X] T084 [P] [US3] Add MessageChunk parser tests for message type, chunk type, size, SecureChannelId, and sequence header in tests/unit/test_message_chunk.c (OPC refs: OPC-10000-6 6.7.2, 6.7.2.4, 6.7.3)
- [X] T085 [P] [US3] Add service dispatch tests for known Discovery, SecureChannel, Session, Browse, and Read request IDs in tests/unit/test_service_dispatch.c (OPC refs: OPC-10000-4 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 5.2.9)
- [X] T086 [P] [US3] Add SecureChannel state tests for OpenSecureChannel and CloseSecureChannel with SecurityPolicy None in tests/unit/test_secure_channel.c (OPC refs: OPC-10000-4 5.6.2.2, 5.6.3.2; OPC-10000-6 6.7.4, 6.7.7)
- [X] T087 [P] [US3] Add Session state tests for CreateSession, ActivateSession identity policy from docs/conformance/identity-policy.md, and CloseSession in tests/unit/test_session.c (OPC refs: OPC-10000-4 5.7.2.2, 5.7.3.2, 5.7.4.2, 7.40.1, 7.40.3, 7.41)
- [X] T088 [P] [US3] Add Discovery service tests for FindServers and GetEndpoints response fields in tests/unit/test_discovery_services.c (OPC refs: OPC-10000-4 5.5.2.2, 5.5.4.2, 7.14)
- [X] T089 [P] [US3] Add Browse service tests for ReferenceDescription output from static references in tests/unit/test_browse_service.c (OPC refs: OPC-10000-4 5.9.2.2, 7.29)
- [X] T090 [P] [US3] Add Read service tests for DataValue output from all v1 scalar value kinds in tests/unit/test_read_service.c (OPC refs: OPC-10000-4 5.11.2.2, 5.11.2.3; OPC-10000-6 5.2.2.17)
- [X] T091 [P] [US3] Add host integration test for minimal connect/discover/browse/read happy path in tests/integration/test_minimal_server_flow.c (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.7.2.2, 5.7.3.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 6.7.2, 6.7.4, 7.1.2.3, 7.1.2.4, 7.2)

### Implementation for User Story 3

- [X] T092 [US3] Implement bounded Binary reader cursor with little-endian primitive reads in src/encoding/binary_reader.c (OPC refs: OPC-10000-6 5.2.1)
- [X] T093 [US3] Implement bounded Binary writer cursor with little-endian primitive writes in src/encoding/binary_writer.c (OPC refs: OPC-10000-6 5.2.1)
- [X] T094 [US3] Implement OPC UA Binary String encode/decode with null and length handling in src/encoding/binary_string.c (OPC refs: OPC-10000-6 5.2.2.4)
- [X] T095 [US3] Implement OPC UA Binary NodeId encode/decode for v1 numeric and string variants in src/encoding/binary_nodeid.c (OPC refs: OPC-10000-6 5.2.2.9)
- [X] T096 [US3] Implement ExtensionObject header decode and bounded skip behavior in src/encoding/binary_extension_object.c (OPC refs: OPC-10000-6 5.2.2.15)
- [X] T097 [US3] Implement Variant encode/decode for v1 scalar values in src/encoding/binary_variant.c (OPC refs: OPC-10000-6 5.2.2.16)
- [X] T098 [US3] Implement DataValue encode/decode for v1 Read responses in src/encoding/binary_datavalue.c (OPC refs: OPC-10000-4 5.11.2.3; OPC-10000-6 5.2.2.17)
- [X] T099 [US3] Define compact request/response encoding IDs and service IDs in src/generated/opcua_ids.c (OPC refs: OPC-10000-4 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 5.2.9)
- [X] T100 [US3] Implement OPC UA TCP Hello/Acknowledge negotiation and buffer limit checks in src/core/tcp_connection.c (OPC refs: OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [X] T101 [US3] Implement UASC MessageChunk header parser and writer in src/core/message_chunk.c (OPC refs: OPC-10000-6 6.7.2, 6.7.3)
- [X] T102 [US3] Implement UASC sequence header validation for the single active SecureChannel in src/core/sequence.c (OPC refs: OPC-10000-6 6.7.2.4, 6.7.7)
- [X] T103 [US3] Implement service message body prefix handling using Binary encoding NodeIds in src/core/service_message.c (OPC refs: OPC-10000-6 5.2.2.9, 5.2.9)
- [X] T104 [US3] Implement service dispatch table for supported services and request Session requirements in src/core/service_dispatch.c (OPC refs: OPC-10000-4 3.1.3, 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.1, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 5.2.9)
- [X] T105 [US3] Implement DiscoveryEndpoint dispatch for FindServers and GetEndpoints without requiring an activated Session in src/core/service_dispatch.c (OPC refs: OPC-10000-4 3.1.3, 5.5.1, 5.5.2.2, 5.5.4.2)
- [X] T106 [US3] Implement SecurityPolicy None OpenSecureChannel and CloseSecureChannel state handling in src/services/secure_channel.c (OPC refs: OPC-10000-4 5.6.2.2, 5.6.3.2; OPC-10000-6 6.7.4, 6.7.7)
- [X] T107 [US3] Implement CreateSession response with bounded server nonce/session token fields in src/services/session.c (OPC refs: OPC-10000-4 5.7.2.1, 5.7.2.2, 7.32, 7.33)
- [X] T108 [US3] Implement ActivateSession identity-token behavior from docs/conformance/identity-policy.md in src/services/session.c (OPC refs: OPC-10000-4 5.7.3.2, 7.40.1, 7.40.3, 7.41)
- [X] T109 [US3] Implement CloseSession teardown of the single Session slot in src/services/session.c (OPC refs: OPC-10000-4 5.7.4.2)
- [X] T110 [US3] Implement FindServers response using ServerConfig identity fields in src/services/discovery.c (OPC refs: OPC-10000-4 5.5.2.2)
- [X] T111 [US3] Implement GetEndpoints response with opc.tcp, UA-TCP UA-SC UA-Binary, SecurityPolicy None, and identity policies from docs/conformance/identity-policy.md in src/services/discovery.c (OPC refs: OPC-10000-4 5.5.4.2, 7.14, 7.40.1, 7.40.3, 7.41; OPC-10000-6 7.2)
- [X] T112 [US3] Implement Browse request decode and static reference traversal in src/services/browse.c (OPC refs: OPC-10000-4 5.9.2.2; OPC-10000-6 5.2.9)
- [X] T113 [US3] Implement Browse response encoding with ReferenceDescription fields in src/services/browse.c (OPC refs: OPC-10000-4 5.9.2.2, 7.29)
- [X] T114 [US3] Implement Browse requestedMaxReferencesPerNode, response-size bounds, and no-continuation behavior in src/services/browse.c (OPC refs: OPC-10000-4 5.9.2.2, 5.9.2.4, 7.9)
- [X] T115 [US3] Implement Read request decode for Value and metadata attributes needed by tested clients in src/services/read.c (OPC refs: OPC-10000-4 5.11.2.2, 5.11.2.3; OPC-10000-6 5.2.9)
- [X] T116 [US3] Implement Read response encoding with DataValue values from the static address space in src/services/read.c (OPC refs: OPC-10000-4 5.11.2.3; OPC-10000-6 5.2.2.17)
- [X] T117 [US3] Integrate TCP, chunk, dispatch, service handlers, and bounded server poll loop in src/core/server.c (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 6.7.2, 6.7.4, 7.1.2.3, 7.1.2.4, 7.2)
- [X] T118 [US3] Implement host socket transport adapter for integration tests without exposing POSIX types in src/platform/host_tcp_adapter.c (OPC refs: OPC-10000-6 7.2)
- [X] T119 [US3] Update minimal host server to listen on opc.tcp endpoint and drive `mu_server_poll` in examples/minimal_server/main.c (OPC refs: OPC-10000-6 7.2)
- [X] T120 [US3] Add CMake registration for encoding, core transport, service sources, and integration test target in CMakeLists.txt (OPC refs: N/A - build infrastructure only)
- [X] T121 [US3] Update traceability rows for supported Discovery, SecureChannel, Session, Browse, and Read services in docs/traceability/sections-to-files.md (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.9.2.4, 5.11.2.2, 5.11.2.3; OPC-10000-6 5.2.9, 6.7.2, 6.7.4, 7.1.2.3, 7.1.2.4, 7.2)
- [X] T122 [US3] Record US3 flash/RAM impact for encoders, chunking, service handlers, generated IDs, and host adapter in docs/size/feature-size-ledger.md (OPC refs: N/A - size-impact tracking only)

**Checkpoint**: US3 is complete when host integration completes connect, discovery, SecureChannel/session setup, Browse, and Read for all v1 scalar value kinds.

---

## Phase 6: User Story 4 - Reject Unsupported and Malformed Requests Correctly (Priority: P4)

**Goal**: Unsupported services, unsupported features, malformed Binary payloads, invalid chunks, invalid lengths, invalid NodeIds, invalid ExtensionObjects, invalid arrays, invalid strings, invalid state sequences, and second-client attempts fail deterministically with cited OPC UA behavior.

**Independent Test**: Run malformed-input, unsupported-service, and invalid-state tests and verify each result maps to a documented StatusCode or OPC UA TCP error behavior.

### Tests and Fixtures for User Story 4

- [X] T123 [P] [US4] Add unsupported Write, CreateSubscription, Publish, Call, HistoryRead, BrowseNext, TranslateBrowsePathsToNodeIds, RegisterNodes, XML, JSON, HTTPS, and WebSockets matrix fixture expecting Bad_ServiceUnsupported or cited transport rejection in tests/fixtures/services/unsupported_services.md (OPC refs: OPC-10000-4 5.9.3.2, 5.9.3.4, 5.9.4.2, 5.9.4.4, 5.9.5.2, 5.9.5.3, 5.11.3.2, 5.11.3.4, 5.11.4.2, 5.11.4.4, 5.12.2.2, 5.12.2.4, 5.14.2.2, 5.14.5.2, 5.14.5.4, 7.38.2; OPC-10000-6 5.3, 5.4.1, 7.4, 7.5.1)
- [X] T124 [P] [US4] Add malformed MessageChunk size tests for too-small, too-large, and inconsistent declared length in tests/unit/test_message_chunk_errors.c (OPC refs: OPC-10000-6 6.7.2, 6.7.3)
- [X] T125 [P] [US4] Add invalid message type and invalid chunk type tests in tests/unit/test_message_chunk_errors.c (OPC refs: OPC-10000-6 6.7.3)
- [X] T126 [P] [US4] Add truncated String, negative length, excessive length, and embedded overrun tests asserting Bad_DecodingError for malformed framing and Bad_EncodingLimitsExceeded for over-limit lengths in tests/unit/test_binary_string_errors.c (OPC refs: OPC-10000-4 7.38.2; OPC-10000-6 5.2.2.4)
- [X] T127 [P] [US4] Add invalid array length and truncated array tests in tests/unit/test_binary_array_errors.c (OPC refs: OPC-10000-6 5.2.5)
- [X] T128 [P] [US4] Add invalid NodeId encoding mask tests in tests/unit/test_binary_nodeid_errors.c (OPC refs: OPC-10000-6 5.2.2.9)
- [X] T129 [P] [US4] Add invalid ExtensionObject encoding and body length tests in tests/unit/test_binary_extension_object_errors.c (OPC refs: OPC-10000-6 5.2.2.15)
- [X] T130 [P] [US4] Add Browse-before-ActivateSession and Read-before-ActivateSession tests in tests/unit/test_service_state_errors.c (OPC refs: OPC-10000-4 5.7.2.1, 5.9.2, 5.11.2, 7.38.2)
- [X] T131 [P] [US4] Add Session-before-SecureChannel and service-before-Hello invalid sequence tests in tests/unit/test_service_state_errors.c (OPC refs: OPC-10000-4 5.6.2.2, 5.7.2.1, 5.7.2.2; OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [X] T132 [P] [US4] Add unsupported SecurityPolicy and identity-token rejection tests derived from docs/conformance/identity-policy.md in tests/unit/test_security_identity_errors.c (OPC refs: OPC-10000-4 5.6.2.2, 5.7.3.2, 7.40.1, 7.40.3, 7.41; OPC-10000-6 6.7.4)
- [X] T133 [P] [US4] Add second-client integration test expecting OPC UA TCP busy/error behavior in tests/integration/test_single_client_limit.c (OPC refs: OPC-10000-6 7.1.5)
- [X] T134 [P] [US4] Add fuzz target for Binary reader entry point in tests/fuzz/fuzz_binary_reader.c (OPC refs: OPC-10000-6 5.2.1)
- [X] T135 [P] [US4] Add fuzz target for MessageChunk parser in tests/fuzz/fuzz_message_chunk.c (OPC refs: OPC-10000-6 6.7.2, 6.7.3)
- [X] T136 [P] [US4] Add fuzz target for NodeId, String, array, ExtensionObject, Variant, and DataValue decode surfaces in tests/fuzz/fuzz_binary_types.c (OPC refs: OPC-10000-6 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.5)

### Implementation for User Story 4

- [X] T137 [US4] Implement unsupported-service dispatch to Bad_ServiceUnsupported in src/core/service_dispatch.c (OPC refs: OPC-10000-4 7.38.2)
- [X] T138 [US4] Implement MessageChunk size, type, and chunk validation failures in src/core/message_chunk.c (OPC refs: OPC-10000-6 6.7.2, 6.7.3)
- [X] T139 [US4] Implement Binary reader bounds errors for truncated primitive, String, and array reads in src/encoding/binary_reader.c (OPC refs: OPC-10000-6 5.2.1, 5.2.2.4, 5.2.5)
- [X] T140 [US4] Implement NodeId invalid encoding mask rejection in src/encoding/binary_nodeid.c (OPC refs: OPC-10000-6 5.2.2.9)
- [X] T141 [US4] Implement ExtensionObject invalid encoding and bounded body-length rejection in src/encoding/binary_extension_object.c (OPC refs: OPC-10000-6 5.2.2.15)
- [X] T142 [US4] Implement invalid service-state StatusCode mapping for pre-Session Browse/Read and pre-SecureChannel Session services in src/core/service_dispatch.c (OPC refs: OPC-10000-4 5.6.2.2, 5.7.2.1, 5.9.2.2, 5.11.2.2, 7.38.2)
- [X] T143 [US4] Implement unsupported SecurityPolicy rejection in OpenSecureChannel handling in src/services/secure_channel.c (OPC refs: OPC-10000-4 5.6.2.2, 7.38.2; OPC-10000-6 6.7.4)
- [X] T144 [US4] Implement unsupported identity-token rejection in ActivateSession handling in src/services/session.c (OPC refs: OPC-10000-4 5.7.3.2, 7.38.2, 7.40.1, 7.40.3, 7.41)
- [X] T145 [US4] Implement single-client TCP busy/error behavior for second active connection in src/core/tcp_connection.c (OPC refs: OPC-10000-6 7.1.5)
- [X] T146 [US4] Implement the write/subscription/method/history/PubSub/XML/JSON/HTTPS/WebSocket out-of-scope status-mapping table (the data table consumed by the Bad_ServiceUnsupported dispatch implemented in T137) in src/core/status.c (OPC refs: OPC-10000-4 5.11.3.2, 5.11.3.4, 5.11.4.2, 5.11.4.4, 5.12.2.2, 5.12.2.4, 5.14.2.2, 5.14.5.2, 5.14.5.4, 7.38.2; OPC-10000-6 5.3, 5.4.1, 7.4, 7.5.1; OPC-10000-14 5.1)
- [ ] T147 [US4] Register fuzz target builds with graceful unavailable-tool documentation in tests/fuzz/CMakeLists.txt (OPC refs: N/A - fuzz build infrastructure only)
- [X] T148 [US4] Update StatusCode traceability rows for all US4 error conditions, including identity-token rejection codes, in docs/traceability/statuscodes.md (OPC refs: OPC-10000-4 5.7.2.1, 5.7.3.2, 5.9.3, 5.9.4, 5.9.5, 5.11.3, 5.11.4, 5.12.2, 5.14.2, 5.14.5, 7.38.2, 7.40.1, 7.40.3, 7.41; OPC-10000-6 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.5, 6.7.3, 7.1.5)
- [X] T149 [US4] Update traceability rows for malformed fixtures, fuzz targets, and state-machine error tests in docs/traceability/fixtures.md (OPC refs: OPC-10000-4 5.6.2.2, 5.7.2.1, 5.9.2.2, 5.11.2.2, 7.38.2; OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.5, 6.7.2, 6.7.3, 7.1.5)
- [X] T150 [US4] Record US4 parser checks, fuzz targets, and error-table size impact in docs/size/feature-size-ledger.md (OPC refs: N/A - size-impact tracking only)

**Checkpoint**: US4 is complete when malformed-input, unsupported-service, fuzz-build, and invalid-state tests all return cited deterministic behavior.

---

## Phase 7: User Story 5 - Audit Conformance, Traceability, and Size (Priority: P5)

**Goal**: A maintainer can inspect conformance status, traceability, async-opcua inventory, supported service claims, SecurityPolicy behavior, validation evidence, and embedded size impact.

**Independent Test**: Generated documentation maps every implemented wire-visible behavior and test to exact OPC UA sections, reports profile-targeting status honestly, inventories async-opcua assets, and records Pico/host size output against the budget.

### Tests for User Story 5

- [X] T151 [P] [US5] Add traceability completeness and OPC UA MCP provenance coverage test that fails if protocol source or test files lack OPC UA section rows or if plan-cited protocol/conformance decisions lack query-ledger entries in tests/unit/test_traceability_docs.c (OPC refs: N/A - traceability enforcement tooling; validates cited section rows and MCP query provenance)
- [X] T152 [P] [US5] Add conformance-claim lint test forbidding profile-compliant or CTT-verified claims without evidence in tests/unit/test_conformance_docs.c (OPC refs: OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, 4.8)
- [X] T153 [P] [US5] Add size-report parser test for flash, RAM, stack, heap, buffer, and static-table fields in tests/unit/test_size_report.c (OPC refs: N/A - size-report validation only)
- [ ] T154 [P] [US5] Add async-opcua inventory validation test for required repository areas in tests/unit/test_async_opcua_inventory.c (OPC refs: N/A - external compliance-suite inventory only)

### Implementation for User Story 5

- [ ] T155 [US5] Implement traceability generation helper for sections-to-files and files-to-sections tables in scripts/generate-traceability.sh (OPC refs: N/A - traceability generation tooling; consumes cited section rows)
- [ ] T156 [US5] Implement CMake traceability and conformance-docs targets that invoke scripts/generate-traceability.sh and assemble the docs/conformance outputs in cmake/MicroOpcUaTraceability.cmake (OPC refs: N/A - build orchestration for traceability tooling only)
- [ ] T157 [US5] Complete Nano profile-targeting checklist and conformance-unit evidence placeholders in docs/conformance/profile-nano.md (OPC refs: OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, 4.8)
- [ ] T158 [US5] Complete supported/unsupported service compatibility matrix with exact services, transport, encoding, and SecurityPolicy behavior in docs/conformance/services.md (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.6.3.2, 5.7.2.2, 5.7.3.2, 5.7.4.2, 5.9.2.2, 5.9.2.4, 5.9.3.2, 5.9.3.4, 5.9.4.2, 5.9.4.4, 5.9.5.2, 5.9.5.3, 5.11.2.2, 5.11.2.3, 5.11.3.2, 5.11.3.4, 5.11.4.2, 5.11.4.4, 5.12.2.2, 5.12.2.4, 5.14.2.2, 5.14.5.2, 5.14.5.4, 7.14, 7.38.2; OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.9, 5.3, 5.4.1, 6.7.4, 7.2, 7.4, 7.5.1)
- [ ] T159 [US5] Complete SecurityPolicy None and user token policy documentation with profile-targeting caveats in docs/conformance/security.md (OPC refs: OPC-10000-4 5.6.2.2, 5.7.3.2, 7.40.1, 7.40.3, 7.41; OPC-10000-6 6.7.4; OPC-10000-7 4.2)
- [ ] T160 [US5] Add interoperability tool and client evidence template for host tests in docs/conformance/interop.md (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.7.2.2, 5.7.3.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 7.1.2.3, 7.1.2.4, 7.2)
- [ ] T161 [US5] Inventory occamsshavingkit/async-opcua .devcontainer, codegen-tests, dotnet-tests, fuzz, schemas, tools, and workflows in docs/conformance/async-opcua-inventory.md (OPC refs: N/A - external compliance-suite inventory only)
- [ ] T162 [US5] Implement host size-report generation and output template in docs/size/host-minimal-server.md (OPC refs: N/A - size-report documentation only)
- [ ] T163 [US5] Implement Pico size-report generation and output template in docs/size/pico-minimal-server.md (OPC refs: N/A - size-report documentation only)
- [ ] T164 [US5] Add CMake size-report target that writes docs/size/host-minimal-server.md and docs/size/pico-minimal-server.md in cmake/MicroOpcUaSizeReport.cmake (OPC refs: N/A - size-report build tooling only)
- [ ] T165 [US5] Update CI workflow to run traceability, conformance-doc, and size-report checks, including <= 128 KiB flash, <= 32 KiB RAM, and <= 16 KiB static-table threshold comparison that fails or flags regressions, in .github/workflows/ci.yml (OPC refs: N/A - CI orchestration only)
- [ ] T166 [US5] Update quickstart validation commands to match implemented CMake targets in specs/001-minimal-embedded-server/quickstart.md (OPC refs: N/A - quickstart command documentation only)
- [ ] T167 [US5] Record US5 documentation, scripts, and CI size impact in docs/size/feature-size-ledger.md (OPC refs: N/A - size-impact tracking only)

**Checkpoint**: US5 is complete when conformance status, traceability, async-opcua inventory, and size reports are current and validated by tests/CI.

---

## Phase 8: Polish and Cross-Cutting Validation

**Purpose**: Final verification, cleanup, and release-readiness evidence after the selected stories are complete.

- [ ] T168 Run host configure/build/unit/integration test flow and record command output summary plus total elapsed wall-clock time against the SC-001 under-15-minutes target in docs/validation/host-tests.md (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.7.2.2, 5.7.3.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 5.2.1, 6.7.2, 7.1.2.3, 7.1.2.4, 7.2)
- [ ] T169 Run sanitizer configure/build/test flow or document unavailable tooling in docs/validation/sanitizers.md (OPC refs: N/A - sanitizer validation runner only)
- [ ] T170 Run fuzz target compile and one bounded smoke execution per fuzz target or document unavailable tooling in docs/validation/fuzz.md (OPC refs: OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.5, 6.7.2, 6.7.3)
- [ ] T171 Run clang-format, cppcheck, and clang-tidy checks and record results in docs/validation/static-analysis.md (OPC refs: N/A - static-analysis validation runner only)
- [ ] T172 Run Pico SDK cross-compile and record artifact/size output in docs/validation/pico-cross-compile.md (OPC refs: N/A - embedded build validation runner only)
- [ ] T173 Run Arduino/PlatformIO adapter skeleton validation and record result in docs/validation/arduino-platformio.md (OPC refs: N/A - embedded adapter validation runner only)
- [ ] T174 Verify every task-created protocol fixture has a sidecar row in docs/traceability/fixtures.md (OPC refs: OPC-10000-6 5.2.9, 6.7.2; fixture-specific service sections supplied per fixture)
- [ ] T175 Verify every unsupported service and malformed-input test maps to docs/traceability/statuscodes.md (OPC refs: OPC-10000-4 5.9.3.2, 5.9.3.4, 5.9.4.2, 5.9.4.4, 5.9.5.2, 5.9.5.3, 5.11.3.2, 5.11.3.4, 5.11.4.2, 5.11.4.4, 5.12.2.2, 5.12.2.4, 5.14.2.2, 5.14.5.2, 5.14.5.4, 7.38.2; OPC-10000-6 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.5, 6.7.3, 7.1.5)
- [ ] T176 Finalize README project status, build commands, and conformance caveats in README.md (OPC refs: OPC-10000-4 5.5.1, 5.5.2.2, 5.5.4.2, 5.6.2.2, 5.7.2.2, 5.7.3.2, 5.9.2.2, 5.11.2.2; OPC-10000-6 5.2.1, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.2.16, 5.2.2.17, 5.2.9, 6.7.2, 7.2; OPC-10000-7 3.1.5, 4.2, 4.4, 4.6, 4.7, 4.8)

---

## Dependencies and Execution Order

### Phase Dependencies

- **Phase 1 Setup**: No dependencies.
- **Phase 2 Foundational**: Depends on Phase 1 and blocks all user stories.
- **Phase 3 US1**: Depends on Phase 2; suggested MVP.
- **Phase 4 US2**: Depends on US1 public API and server config.
- **Phase 5 US3**: Depends on US1 lifecycle and US2 address-space configuration.
- **Phase 6 US4**: Depends on US3 parser, chunk, dispatch, channel, session, Browse, and Read surfaces.
- **Phase 7 US5**: Can start after Phase 2 for documentation skeletons, but final completion depends on US1-US4 evidence.
- **Phase 8 Polish**: Depends on all selected story phases.

### Story Completion Order

1. US1: Buildable portable core and embedded skeletons.
2. US2: Static address-space configuration and scalar value access.
3. US3: Interoperable connect/discover/browse/read path.
4. US4: Deterministic unsupported and malformed behavior.
5. US5: Conformance, traceability, size, async-opcua inventory, and validation evidence.

### Within Each User Story

- Fixture/test tasks come before the implementation they validate.
- Public headers come before source files that implement them.
- Binary encoders/decoders come before service dispatch and service handlers.
- StatusCode mapping comes before integration tests rely on it.
- Traceability and size records close each story checkpoint.

## Parallel Execution Examples

### Setup

- T004, T005, T008, T009, and T010 can run in parallel after T001 is planned because they touch independent files.

### US1

- T024, T025, T026, T027, T028, and T029 can be reviewed in parallel as independent tests.
- T030, T031, T032, T033, and T034 can proceed in parallel once the API shapes are agreed.

### US2

- T049, T050, T051, T052, and T053 can be written in parallel.
- T056 and T057 should precede T058-T060; T062 and T064 can proceed in parallel after T054.

### US3

- T067-T082 fixture, encoder, discovery, and Browse tests can be written in parallel by file.
- T092-T098 encoder implementations can proceed in parallel after their paired tests are reviewed.
- Discovery, Session, Browse, and Read implementation tasks T106-T116 can proceed in parallel after dispatch and encoding are available.

### US4

- T123-T136 malformed, unsupported, and fuzz tests can be written in parallel by file.
- T137-T146 can proceed in parallel where they touch separate parser/service files, after status mapping expectations are agreed.

### US5

- T151-T154 documentation validation tests can be written in parallel.
- T157-T163 documentation and report templates can be updated in parallel after traceability skeletons exist.

## Implementation Strategy

### MVP First

Complete Phases 1-3 to deliver the MVP: a host-buildable, host-testable, embedded-cross-compilable portable C core with public APIs, no hot-path heap allocation, and adapter skeletons.

### Incremental Delivery

1. Add US2 to make the server useful to firmware through a static address space and caller-owned values.
2. Add US3 to expose the first interoperable OPC UA path over OPC UA TCP Binary.
3. Add US4 to harden every unsupported, malformed, and invalid-state path.
4. Add US5 to make conformance status, traceability, size, and compliance-suite planning auditable.

### Review Discipline

Each task should be reviewable as a standalone PR. If implementation discovers that a task needs broad cross-file changes, split the work and update this file before coding.
