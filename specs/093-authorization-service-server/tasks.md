# Tasks: Authorization Service Server Facet

**Input**: Design documents from `specs/093-authorization-service-server/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Tests are mandatory for protocol parsing, JWT validation, signature verification, claim extraction, and ActivateSession error codes.

**Organization**: Tasks grouped by user story. Tests before implementation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- Include exact file paths
- Include OPC UA references for protocol tasks

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Kconfig symbols, CMake gating, build integration

- [ ] T001 Add `MUC_OPCUA_CU_USER_TOKEN_JWT` Kconfig symbol with depends on `MUC_OPCUA_CU_USER_AUTH`, default `y` for full profile, `n` otherwise in `Kconfig`
- [ ] T002 [P] Add `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` Kconfig symbol with depends on `MUC_OPCUA_CU_USER_TOKEN_JWT && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`, default `y` for full profile, `n` otherwise in `Kconfig`
- [ ] T003 [P] Add `mu_jwt_config_t` and `mu_jwt_issuer_t` structs to `include/muc_opcua/server.h` (gated on `MUC_OPCUA_CU_USER_TOKEN_JWT`) per data-model.md
- [ ] T004 [P] Add `mu_jwt_result_t` enum and `mu_jwt_alg_t` enum to new `include/muc_opcua/authorization/jwt.h`
- [ ] T005 Add CMake gating for `src/cu/core_2022_server/authorization/` source files in `src/CMakeLists.txt`

---

## Phase 2: Foundational — JWT Parser (Blocking Prerequisites)

**Purpose**: Core JWT parsing and validation that ALL user stories depend on. Must be complete before any user story work.

**CRITICAL**: No user story can begin until the parser, Base64url decoder, and signature verification wrappers are complete.

- [ ] T006 Implement Base64url decoder (no padding, URL-safe alphabet) in `src/cu/core_2022_server/authorization/base64url.c` and `base64url.h` per RFC 7515 §2, RFC 4648 §5. No heap — output buffer caller-provided.
- [ ] T007 Implement JWT parser: three-segment split, Base64url decode header + payload in `src/cu/core_2022_server/authorization/jwt.c` and `include/muc_opcua/authorization/jwt.h` per OPC-10000-7 CU 1697, RFC 7519 §4-5
- [ ] T008 [P] Implement claim scanner (minimal streaming JSON key-value extractor for `iss`, `sub`, `aud`, `exp`, `nbf`, `iat`) in `src/cu/core_2022_server/authorization/claim_scanner.c` and `claim_scanner.h`. Unknown keys skipped. Fixed-size output buffers. No heap.
- [ ] T009 Implement `mu_jwt_validate()` public API: header check (`alg` → algorithm dispatch), signature reconstruct + verify via platform crypto adapter, claim scanner → `mu_jwt_claims_t`, expiry/not-before/issuer/audience checks in `src/cu/core_2022_server/authorization/jwt.c` per RFC 7519 §4.1, RFC 8725 §3
- [ ] T010 [P] Add RSA signature verification wrapper calling platform crypto adapter (`mu_crypto_rsa_verify()`) in `src/cu/core_2022_server/authorization/crypto_jwt.c` and `crypto_jwt.h` per research.md R2
- [ ] T011 [P] Add ECDSA signature verification wrapper (`mu_crypto_ecdsa_verify()`) gated on `MUC_OPCUA_CU_SECURITY_ECC` in `src/cu/core_2022_server/authorization/crypto_jwt.c` per research.md R6

**Checkpoint**: JWT parser, scanner, and validator pass unit tests. No ActivateSession integration yet.

---

## Phase 3: User Story 1 — JWT Bearer Token Session Activation (Priority: P1) [MVP]

**Goal**: Client activates an OPC UA session with a valid RS256-signed JWT. Server validates signature, issuer, audience, expiry, and `sub` claim.

**Independent Test**: Configure server with a test RSA key pair, send ActivateSession with a valid signed JWT, verify Good status and user identity "operator1".

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [ ] T012 [P] [US1] Add JWT unit test: valid RS256 token → `MU_JWT_OK`, claims correctly extracted in `tests/unit/test_jwt.c` per spec.md SC-001, FR-003
- [ ] T013 [P] [US1] Add JWT unit test: expired token → `MU_JWT_ERR_EXPIRED` in `tests/unit/test_jwt.c` per spec.md SC-002, FR-003
- [ ] T014 [P] [US1] Add JWT unit test: wrong signing key → `MU_JWT_ERR_SIGNATURE` in `tests/unit/test_jwt.c` per spec.md SC-003, FR-003
- [ ] T015 [P] [US1] Add JWT unit test: wrong issuer → `MU_JWT_ERR_ISSUER`, wrong audience → `MU_JWT_ERR_AUDIENCE`, missing `sub` → `MU_JWT_ERR_NO_SUB` in `tests/unit/test_jwt.c` per FR-003, FR-007
- [ ] T016 [P] [US1] Add JWT unit test: malformed (not three segments) → `MU_JWT_ERR_MALFORMED`, bad Base64 → `MU_JWT_ERR_BASE64`, unsupported alg → `MU_JWT_ERR_UNSUPPORTED_ALG` in `tests/unit/test_jwt.c` per spec.md Edge Cases
- [ ] T017 [P] [US1] Add JWT unit test: `nbf` in future → rejected, no `exp` → rejected, no configured issuers → `MU_JWT_ERR_NO_CONFIGURED_ISSUERS` in `tests/unit/test_jwt.c` per spec.md Edge Cases, FR-003

### Implementation for User Story 1

- [ ] T018 [US1] Hook JWT validation into `handle_activate_session` in `src/core/service_dispatch/activate_session.c`: detect `tokenType` URI `urn:ietf:params:oauth:token-type:jwt`, extract raw JWT from `tokenData` ByteString, call `mu_jwt_validate()`, map result to `Bad_IdentityTokenInvalid` vs `Bad_IdentityTokenRejected` per OPC-10000-4 §5.7.3 Table 41, FR-002, FR-007, FR-008
- [ ] T019 [US1] Extract `sub` claim as session user identity in `src/core/service_dispatch/activate_session.c` per FR-006, OPC-10000-5 §6.4.7
- [ ] T020 [US1] Add integration test: ActivateSession with valid JWT → session created, user identity correct in `tests/integration/test_jwt_activate_session.c` per spec.md US1 Acceptance Scenario 1, SC-001

**Checkpoint**: JWT session activation works end-to-end. All P1 tests pass.

---

## Phase 4: User Story 2 — Multi-Issuer Configuration (Priority: P2)

**Goal**: Server supports multiple trusted OAuth2 issuers with independent keys, audiences, and clock skew. Issuer configuration is set at server init.

**Independent Test**: Configure two issuers with different keys, verify both accept valid JWTs and reject each other's audience.

### Tests for User Story 2

- [ ] T021 [P] [US2] Add unit test: two-issuer config, JWT from issuer A accepted, JWT from issuer B accepted, cross-issuer audience mismatch rejected in `tests/unit/test_jwt_multi_issuer.c` per spec.md US2 Acceptance Scenarios
- [ ] T022 [P] [US2] Add unit test: clock skew tolerance (token at boundary + skew accepted, beyond skew rejected) in `tests/unit/test_jwt_clock_skew.c` per FR-005

### Implementation for User Story 2

- [ ] T023 [P] [US2] Implement issuer table lookup in `mu_jwt_validate()`: iterate `issuer_count` issuers, match `iss` claim, dispatch to that issuer's key and audience in `src/cu/core_2022_server/authorization/jwt.c` per FR-005
- [ ] T024 [US2] Implement per-issuer clock skew tolerance in expiry/nbf checks in `src/cu/core_2022_server/authorization/jwt.c` per FR-005, Assumptions (last bullet)

**Checkpoint**: Multi-issuer support works. Both P2 tests pass. P1 tests still pass.

---

## Phase 5: User Story 3 — JWT Claims to User Identity (Priority: P3)

**Goal**: Extracted `sub` claim is correctly stored in session identity. Optional role claims are parsed.

**Independent Test**: JWT with `sub: "operator1"` → session reports "operator1".

### Tests for User Story 3

- [ ] T025 [P] [US3] Add unit test: `sub` claim handling — valid sub, empty sub (rejected), 128-byte sub, sub with special characters in `tests/unit/test_jwt_claims.c` per spec.md SC-005, FR-006

### Implementation for User Story 3

- [ ] T026 [US3] Wire `sub` claim → session user identity in `src/core/service_dispatch/activate_session.c` per FR-006, OPC-10000-4 §5.7.3
- [ ] T027 [US3] Update session diagnostics: include user identity from JWT claims in `src/cu/core_2022_server/diagnostics/diagnostics.c` per OPC-10000-5 §6.3.5

**Checkpoint**: Identity mapping works. P3 tests pass. P1/P2 tests still pass.

---

## Phase 6: Address Space (CU 1629)

**Purpose**: AuthorizationServiceConfigurationType InstanceDeclarations for CU 1629 compliance. Gated on `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER`.

- [ ] T028 Add `AuthorizationServiceConfigurationType` type-system InstanceDeclarations in `src/address_space/base_nodes.c` (gated on `MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`) per OPC-10000-12 §9.7.4, data-model.md
- [ ] T029 [P] Add conformance doc for CU 1629 in `docs/conformance/authorization-service.md` per OPC-10000-7 §6.6
- [ ] T030 [P] Add conformance doc for CU 1697 in `docs/conformance/jwt-user-token.md` per OPC-10000-7 §6.6
- [ ] T031 Update `docs/conformance/opc-profile-roadmap.md` to claim CU 1629 and CU 1697 for full profile

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Size measurement, profile builds, traceability, final validation

- [ ] T032 Run all profile builds (nano, micro, embedded, standard, full) and verify compile-out of JWT code when symbols undefined per spec.md SC-006
- [ ] T033 Measure `.text` growth in standard profile and verify ≤5 KB per plan.md size budget, spec.md SC-007
- [ ] T034 Run full test suite (`ctest --test-dir build/full --output-on-failure`) and verify zero regressions per spec.md SC-008
- [ ] T035 Run clang-format, cppcheck, and clang-tidy; fix any violations
- [ ] T036 Update `README.md` size table if ≥100 B delta
- [ ] T037 Update `docs/traceability/files-to-sections.md` with new source and test files

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — starts immediately
- **Foundational (Phase 2)**: Depends on Setup (Kconfig symbols needed for `#if` gates). All user stories blocked until complete.
- **US1 (Phase 3)**: Depends on Foundational. This is the MVP.
- **US2 (Phase 4)**: Depends on US1 (issuer table extends single-issuer path)
- **US3 (Phase 5)**: Depends on US1 (identity mapping extends `sub` extraction)
- **Address Space (Phase 6)**: Depends on Setup (CU gates). Independent of US1-US3.
- **Polish (Phase 7)**: Depends on all preceding phases

### Parallel Opportunities

- T001-T005 (Setup): all parallel
- T006-T011 (Foundational): T006 before T007, T008 parallel with T006-T007, T009 after T007-T008, T010-T011 parallel
- T012-T017 (US1 tests): all parallel
- T021-T022 (US2 tests): parallel
- T028-T031 (Address Space): T029-T031 parallel, T028 independent
- Phase 6 (Address Space) can run in parallel with Phases 3-5

### Suggested MVP Scope

Phase 1 + 2 + 3 = working JWT session activation with single issuer. This is independently shippable and testable.
