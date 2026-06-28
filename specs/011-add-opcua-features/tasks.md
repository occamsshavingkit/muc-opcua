# Tasks: Implement Custom Methods, Aes256_Sha256_RsaPss, Server Diagnostics, and Dynamic Node Management

**Input**: Design documents from `/specs/011-add-opcua-features/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md

**Tests**: Tests are required for all newly introduced services, adapters, and callbacks.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)

## Phase 1: Setup & Foundational

- [ ] T001 Define custom method callback signature `mu_method_callback_t` in `include/micro_opcua/server.h` (OPC-10000-4 §5.12.2)
- [ ] T002 Extend `mu_node_t` structure in `src/address_space/address_space.h` to support method callbacks (OPC-10000-4 §5.12.2)
- [ ] T003 Add registration API `mu_server_add_method_node` to `include/micro_opcua/server.h` and implement skeleton in `src/core/server.c`
- [ ] T004 Define security policy constants for `Aes256_Sha256_RsaPss` in `src/security/security_policy.h` (OPC-10000-7 §6.2)
- [ ] T005 Add `Aes256_Sha256_RsaPss` to the parsed policy selection table in `src/security/security_policy.c` (OPC-10000-7 §6.2)
- [ ] T005b Configure new CMake option flags `MICRO_OPCUA_CUSTOM_METHODS`, `MICRO_OPCUA_SERVER_DIAGNOSTICS`, and `MICRO_OPCUA_DYNAMIC_NODES` in `cmake/MicroOpcUaOptions.cmake`

## Phase 2: User Story 1 - Custom Method Calls (P1)

- [ ] T006 Implement input argument validation helper in `src/core/service_dispatch.c` to parse input types against method signature (OPC-10000-4 §5.12.2)
- [ ] T007 Refactor `write_single_call_method_result` in `src/core/service_dispatch.c` to support user-registered callbacks (OPC-10000-4 §5.12.2)
- [ ] T008 Implement output argument encoding logic in `src/core/service_dispatch.c` for user method responses (OPC-10000-4 §5.12.2)
- [ ] T009 Create unit test suite `tests/unit/test_method_call.c` asserting successful method executions and argument mismatch checks
- [ ] T010 Add `test_method_call` target to `tests/unit/CMakeLists.txt`

## Phase 3: User Story 2 - Aes256_Sha256_RsaPss Security Policy (P2)

- [ ] T011 Extend `mu_crypto_adapter_t` struct to support RSA-PSS and SHA-256 OAEP parameters
- [ ] T012 Implement RSA-PSS signature verification in host OpenSSL adapter `src/platform/host_crypto_adapter.c` (OPC-10000-7 §6.2)
- [ ] T013 Implement RSA-OAEP SHA-256 decryption in host OpenSSL adapter `src/platform/host_crypto_adapter.c` (OPC-10000-7 §6.2)
- [ ] T014a Implement RSA-PSS signature verification in Mbed TLS adapter `src/platform/mbedtls_crypto_adapter.c` (OPC-10000-7 §6.2)
- [ ] T014b Implement RSA-OAEP SHA-256 decryption in Mbed TLS adapter `src/platform/mbedtls_crypto_adapter.c` (OPC-10000-7 §6.2)
- [ ] T015a Implement RSA-PSS signature verification in wolfSSL adapter `src/platform/wolfssl_crypto_adapter.c` (OPC-10000-7 §6.2)
- [ ] T015b Implement RSA-OAEP SHA-256 decryption in wolfSSL adapter `src/platform/wolfssl_crypto_adapter.c` (OPC-10000-7 §6.2)
- [ ] T016 Update asymmetric chunk handler `src/security/asym_chunk.c` to configure PSS padding flags when active policy is `Aes256_Sha256_RsaPss` (OPC-10000-6 §6.7.2)
- [ ] T017 Create unit tests in `tests/unit/test_security_policy.c` for Aes256_Sha256_RsaPss secure channel handshake
- [ ] T018 Add `test_security_policy` target to `tests/unit/CMakeLists.txt`

## Phase 4: User Story 3 - Server Diagnostics (P3)

- [ ] T019 Declare diagnostic summary variables in standard base node set in `src/address_space/base_nodes.c` (OPC-10000-5 §6.x)
- [ ] T020 Implement atomic counter update helpers in `src/core/server.c`
- [ ] T021 Hook channel opening and closing to update `CurrentSessionCount` and `AccumulatedSessionCount` (OPC-10000-5 §6.x)
- [ ] T022 Hook subscription lifetime events to update `CurrentSubscriptionCount` (OPC-10000-5 §6.x)
- [ ] T023 Implement integration test verifying diagnostics variable updates during standard client sessions

## Phase 5: User Story 4 - Dynamic Node Management (P4)

- [ ] T024 Initialize static dynamic node pool arrays in `src/address_space/address_space.c`
- [ ] T025 Implement safe `AddNodes` allocator extracting nodes from static pool under freestanding limits (OPC-10000-4 §5.5.2)
- [ ] T026 Implement safe `DeleteNodes` deallocator returning nodes to pool and cleansing references (OPC-10000-4 §5.5.3)
- [ ] T027 Add `AddNodes` and `DeleteNodes` request decoders and response encoders in `src/core/service_dispatch.c`
- [ ] T028 Create unit tests in `tests/unit/test_node_management.c` verifying dynamic node addition, browsing, and deletion

## Phase 6: Compliance & Polish

- [ ] T029 Run all host unit and integration tests to verify 100% correctness
- [ ] T030 Perform clang-format check and document memory footprint sizes in `plan.md`
