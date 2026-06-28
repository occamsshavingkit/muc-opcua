# Tasks: Implement Mbed TLS and wolfSSL platform adapters

**Input**: Design documents from `/specs/010-add-crypto-adapters/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md

**Tests**: Tests are required for all platform adapter cryptographic wrapper functions.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Build system preparation and skeleton files.

- [x] T001 Configure CMake option `MICRO_OPCUA_HAVE_MBEDTLS` and `MICRO_OPCUA_HAVE_WOLFSSL` in `cmake/MicroOpcUaOptions.cmake`
- [x] T002 Create blank placeholder source files for `src/platform/mbedtls_crypto_adapter.c` and `src/platform/wolfssl_crypto_adapter.c`
- [x] T003 Create blank placeholder test files for `tests/unit/test_mbedtls_adapter.c` and `tests/unit/test_wolfssl_adapter.c`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: API declarations and traceability.

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [x] T004 Document target profile/conformance-unit details in `docs/traceability/010-add-crypto-adapters.md`
- [x] T005 [P] Declare public initialization functions for the new adapters in `include/micro_opcua/platform.h`

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - Mbed TLS Platform Adapter (Priority: P1) [MVP]

**Goal**: Support cryptographic operations using Mbed TLS.

**Independent Test**: Build and run all tests linking with Mbed TLS adapter.

### Tests for User Story 1

- [x] T006 [P] [US1] Implement unit test skeleton in `tests/unit/test_mbedtls_adapter.c`
- [x] T007 [P] [US1] Add test cases for asymmetric RSA sign/verify in `tests/unit/test_mbedtls_adapter.c`
- [x] T008 [P] [US1] Add test cases for symmetric AES encryption/decryption in `tests/unit/test_mbedtls_adapter.c`
- [x] T009 [P] [US1] Add test cases for key derivation (P_SHA256) in `tests/unit/test_mbedtls_adapter.c`

### Implementation for User Story 1

- [x] T010 [P] [US1] Implement RSA decrypt and signature verification functions in `src/platform/mbedtls_crypto_adapter.c`
- [x] T011 [P] [US1] Implement AES symmetric encryption/decryption and key derivation in `src/platform/mbedtls_crypto_adapter.c`
- [x] T012 [US1] Measure and record binary footprint impact of the Mbed TLS adapter in `specs/010-add-crypto-adapters/plan.md`

**Checkpoint**: User Story 1 is independently testable and size impact is known.

---

## Phase 4: User Story 2 - wolfSSL Platform Adapter (Priority: P2)

**Goal**: Support cryptographic operations using wolfSSL.

**Independent Test**: Build and run all tests linking with wolfSSL adapter.

### Tests for User Story 2

- [x] T013 [P] [US2] Implement unit test skeleton in `tests/unit/test_wolfssl_adapter.c`
- [x] T014 [P] [US2] Add test cases for asymmetric RSA sign/verify in `tests/unit/test_wolfssl_adapter.c`
- [x] T015 [P] [US2] Add test cases for symmetric AES encryption/decryption and key derivation in `tests/unit/test_wolfssl_adapter.c`

### Implementation for User Story 2

- [x] T016 [P] [US2] Implement RSA decrypt and signature verification functions in `src/platform/wolfssl_crypto_adapter.c`
- [x] T017 [P] [US2] Implement AES symmetric encryption/decryption and key derivation in `src/platform/wolfssl_crypto_adapter.c`
- [x] T018 [US2] Measure and record binary footprint impact of the wolfSSL adapter in `specs/010-add-crypto-adapters/plan.md`

**Checkpoint**: User Stories 1 and 2 both work independently.

---

## Phase 5: User Story 3 - Configurable Build and Selection (Priority: P2)

**Goal**: Select adapter via CMake flags.

**Independent Test**: Verify compiler doesn't compile unselected files.

### Tests for User Story 3

- [x] T019 [P] [US3] Add unit test checking CMake options compile correctly with only Mbed TLS enabled in `tests/unit/test_build_config.c`
- [x] T020 [P] [US3] Add unit test checking CMake options compile correctly with only wolfSSL enabled in `tests/unit/test_build_config.c`

### Implementation for User Story 3

- [x] T021 [US3] Update `src/CMakeLists.txt` to include platform source files selectively depending on CMake flags

**Checkpoint**: Configurable build flags are fully functional.

---

## Phase 6: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation before feature completion.

- [x] T022 Run host unit and integration tests with all adapters
- [x] T023 Run formatting with clang-format on all modified files
- [x] T024 Compare final binary footprints with the budgeted allocations in `plan.md`
- [x] T025 Complete final update of `docs/traceability/010-add-crypto-adapters.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion; blocks all stories
- **User Stories (Phase 3+)**: Depend on Foundational phase completion
- **Compliance/Polish**: Depends on all desired user stories being complete
