# Tasks: UserName/Password User Identity Token Authentication

**Input**: Design documents from `/specs/008-user-identity-auth/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, quickstart.md

**Tests**: Tests are mandatory for protocol parsing, decoding, validation, decryption, and error codes.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Build system configuration for user identity token support.

- [x] T001 Configure CMake option `MICRO_OPCUA_USER_AUTH` in `cmake/MicroOpcUaOptions.cmake`
- [x] T002 Configure compiler flags and source gating for user authentication in `CMakeLists.txt`
- [x] T003 Add build configuration check unit test in `tests/unit/test_build_config.c`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core interfaces and initial traceability configuration.

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [x] T004 Document target conformance unit and cited sections in `docs/traceability/008-user-identity-auth.md`
- [x] T005 Define `mu_user_auth_handler_t` and `user_auth_handler` / `user_auth_handler_handle` fields in `include/micro_opcua/server.h`
- [x] T006 Define token structures in `include/micro_opcua/types.h`

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - Anonymous User Authentication (Priority: P1)

**Goal**: Support session activation via AnonymousIdentityToken when enabled.

**Independent Test**: Connect using anonymous policy, verify session state becomes ACTIVATED.

### Tests for User Story 1

- [x] T007 [P] [US1] Add unit test for anonymous session activation in `tests/unit/test_session_auth.c`

### Implementation for User Story 1

- [x] T008 [US1] Implement anonymous policy check in `mu_session_activate` in `src/services/session.c`
- [x] T009 [US1] Measure and record flash/RAM size impact of User Story 1 in `specs/008-user-identity-auth/plan.md`

**Checkpoint**: User Story 1 works independently.

---

## Phase 4: User Story 2 - UserName/Password Authentication (Plaintext) (Priority: P1) [MVP]

**Goal**: Support plaintext Username/Password authentication over security policy None.

**Independent Test**: Connect client over None security policy with Username/Password, verify session activates successfully.

### Tests for User Story 2

- [x] T010 [P] [US2] Add binary fixture for plaintext `ActivateSessionRequest` (citing OPC-10000-4 §5.7.3 and OPC-10000-6 §7.38.3) in `tests/fixtures/activate_session_user_plaintext_req.bin`
- [x] T011 [P] [US2] Add unit test for `UserNameIdentityToken` decoder in `tests/unit/test_user_token_decoder.c`
- [x] T012 [US2] Add integration test for plaintext UserName validation using mock auth callback in `tests/integration/test_user_auth_plaintext.c`

### Implementation for User Story 2

- [x] T013 [P] [US2] Implement binary decoder `mu_username_identity_token_decode` (citing OPC-10000-6 §7.38.3) in `src/encoding/binary_reader.c` and `src/encoding/binary_reader.h`
- [x] T014 [US2] Implement user authentication callback invocation inside `ActivateSession` request processing in `src/core/service_dispatch.c`
- [x] T015 [US2] Advertise `UserName` token policy in `GetEndpoints` response (citing OPC-10000-4 §7.37) in `src/services/discovery.c`
- [x] T016 [US2] Measure and record flash/RAM size impact of User Story 2 in `specs/008-user-identity-auth/plan.md`

**Checkpoint**: User Stories 1 and 2 work independently.

---

## Phase 5: User Story 3 - Encrypted UserName/Password Authentication (Priority: P2)

**Goal**: Support decrypting and validating user credentials sent over standard secure channels.

**Independent Test**: Connect client over secure channel, authenticate with encrypted password, verify successful session activation.

### Tests for User Story 3

- [x] T017 [P] [US3] Add binary fixture for encrypted `ActivateSessionRequest` (citing OPC-10000-4 §5.7.3 and §7.36) in `tests/fixtures/activate_session_user_encrypted_req.bin`
- [x] T018 [P] [US3] Add unit test for encrypted password validation and decryption in `tests/unit/test_user_token_encrypted.c`

### Implementation for User Story 3

- [x] T019 [US3] Implement decryption using `crypto_adapter->rsa_oaep_decrypt` in `src/core/service_dispatch.c` when `encryptionAlgorithm` is set
- [x] T020 [US3] Measure and record size impact in `specs/008-user-identity-auth/plan.md`

**Checkpoint**: User Stories 1, 2, and 3 are all functional.

---

## Phase 6: User Story 4 - Rejection of Invalid Credentials (Priority: P1)

**Goal**: Enforce correct StatusCodes when authentication fails or token is malformed.

**Independent Test**: Verify invalid credentials return `Bad_IdentityTokenRejected` and malformed bodies return `Bad_DecodingError`.

### Tests for User Story 4

- [x] T021 [P] [US4] Add unit tests for credential error cases and unknown token type IDs (rejection of certificate/issued tokens) in `tests/unit/test_user_auth_errors.c`
- [x] T022 [US4] Add integration test validating error response structures (citing OPC-10000-4 §5.7.3.3) under `tests/integration/test_user_auth_validation.c`

### Implementation for User Story 4

- [x] T023 [US4] Implement status mapping logic and return correct status codes (`Bad_IdentityTokenRejected`, `Bad_IdentityTokenInvalid`) in `src/core/service_dispatch.c`
- [x] T024 [US4] Measure and record size impact in `specs/008-user-identity-auth/plan.md`

**Checkpoint**: Error handling and token verification conforms to the spec.

---

## Phase 7: Compliance, Size, and Polish

**Purpose**: Verification and polish before feature completion.

- [x] T025 Run all unit and integration tests in `Makefile`
- [x] T026 Run AddressSanitizer and UndefinedBehaviorSanitizer builds in `Makefile`
- [x] T027 Run formatting in `Makefile` and `clang-format`
- [x] T028 Run embedded size measurement in `scripts/measure_size.sh`
- [x] T029 Complete documentation of traceability mappings in `docs/traceability/008-user-identity-auth.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion; blocks all stories
- **User Stories (Phase 3+)**: Depend on Foundational phase completion
- **Compliance/Polish**: Depends on all user stories being complete
