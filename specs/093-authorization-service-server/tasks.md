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

- [x] T001 Add `MUC_OPCUA_CU_USER_TOKEN_JWT` Kconfig symbol with depends on `MUC_OPCUA_CU_USER_AUTH`, default `y` for full profile, `n` otherwise in `Kconfig`
- [x] T002 [P] Add `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` Kconfig symbol with depends on `MUC_OPCUA_CU_USER_TOKEN_JWT && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`, default `y` for full profile, `n` otherwise in `Kconfig`
- [x] T003 [P] Add `mu_jwt_config_t` and `mu_jwt_issuer_t` structs to `include/muc_opcua/server.h` (gated on `MUC_OPCUA_CU_USER_TOKEN_JWT`) per data-model.md
- [x] T004 [P] Add `mu_jwt_result_t` enum and `mu_jwt_alg_t` enum to new `include/muc_opcua/authorization/jwt.h`
- [x] T005 Add CMake gating for `src/cu/core_2022_server/authorization/` source files in `src/CMakeLists.txt`

---

## Phase 2: Foundational — JWT Parser (Blocking Prerequisites)

**Purpose**: Core JWT parsing and validation that ALL user stories depend on. Must be complete before any user story work.

**CRITICAL**: No user story can begin until the parser, Base64url decoder, and signature verification wrappers are complete.

- [x] T006 Implement Base64url decoder (no padding, URL-safe alphabet) in `src/cu/core_2022_server/authorization/base64url.c` and `base64url.h` per RFC 7515 §2, RFC 4648 §5. No heap — output buffer caller-provided.
- [x] T007 Implement JWT parser: three-segment split, Base64url decode header + payload in `src/cu/core_2022_server/authorization/jwt.c` and `include/muc_opcua/authorization/jwt.h` per OPC-10000-7 CU 1697, RFC 7519 §4-5
- [x] T008 [P] Implement claim scanner (minimal streaming JSON key-value extractor for `iss`, `sub`, `aud`, `exp`, `nbf`, `iat`) in `src/cu/core_2022_server/authorization/claim_scanner.c` and `claim_scanner.h`. Unknown keys skipped. Fixed-size output buffers. No heap.
- [x] T009 Implement `mu_jwt_validate()` public API: header check (`alg` → algorithm dispatch), signature reconstruct + verify via platform crypto adapter, claim scanner → `mu_jwt_claims_t`, expiry/not-before/issuer/audience checks in `src/cu/core_2022_server/authorization/jwt.c` per RFC 7519 §4.1, RFC 8725 §3
- [x] T010 [P] Add RSA signature verification wrapper calling platform crypto adapter (`mu_crypto_rsa_verify()`) in `src/cu/core_2022_server/authorization/crypto_jwt.c` and `crypto_jwt.h` per research.md R2
- [x] T011 [P] Add ECDSA signature verification wrapper (`mu_crypto_ecdsa_verify()`) gated on `MUC_OPCUA_CU_SECURITY_ECC` in `src/cu/core_2022_server/authorization/crypto_jwt.c` per research.md R6

**Checkpoint**: JWT parser, scanner, and validator pass unit tests. No ActivateSession integration yet.

---

## Phase 3: User Story 1 — JWT Bearer Token Session Activation (Priority: P1) [MVP]

**Goal**: Client activates an OPC UA session with a valid RS256-signed JWT. Server validates signature, issuer, audience, expiry, and `sub` claim.

**Independent Test**: Configure server with a test RSA key pair, send ActivateSession with a valid signed JWT, verify Good status and user identity "operator1".

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [x] T012 [P] [US1] Add JWT unit test: valid RS256 token → `MU_JWT_OK`, claims correctly extracted in `tests/unit/test_jwt.c` per spec.md SC-001, FR-003
- [x] T013 [P] [US1] Add JWT unit test: expired token → `MU_JWT_ERR_EXPIRED` in `tests/unit/test_jwt.c` per spec.md SC-002, FR-003
- [x] T014 [P] [US1] Add JWT unit test: wrong signing key → `MU_JWT_ERR_SIGNATURE` in `tests/unit/test_jwt.c` per spec.md SC-003, FR-003
- [x] T015 [P] [US1] Add JWT unit test: wrong issuer → `MU_JWT_ERR_ISSUER`, wrong audience → `MU_JWT_ERR_AUDIENCE`, missing `sub` → `MU_JWT_ERR_NO_SUB` in `tests/unit/test_jwt.c` per FR-003, FR-007
- [x] T016 [P] [US1] Add JWT unit test: malformed (not three segments) → `MU_JWT_ERR_MALFORMED`, bad Base64 → `MU_JWT_ERR_BASE64`, unsupported alg → `MU_JWT_ERR_UNSUPPORTED_ALG` in `tests/unit/test_jwt.c` per spec.md Edge Cases
- [x] T017 [P] [US1] Add JWT unit test: `nbf` in future → rejected, no `exp` → rejected, no configured issuers → `MU_JWT_ERR_NO_CONFIGURED_ISSUERS` in `tests/unit/test_jwt.c` per spec.md Edge Cases, FR-003

### Implementation for User Story 1

- [x] T018 [US1] Hook JWT validation into `handle_activate_session` in `src/core/service_dispatch/activate_session.c`: detect `tokenType` URI `urn:ietf:params:oauth:token-type:jwt`, extract raw JWT from `tokenData` ByteString, call `mu_jwt_validate()`, map result to `Bad_IdentityTokenInvalid` vs `Bad_IdentityTokenRejected` per OPC-10000-4 §5.7.3 Table 41, FR-002, FR-007, FR-008
- [x] T019 [US1] Extract `sub` claim as session user identity in `src/core/service_dispatch/activate_session.c` per FR-006, OPC-10000-5 §6.4.7
- [x] T020 [US1] Add integration test: ActivateSession with valid JWT → session created, user identity correct in `tests/integration/test_jwt_activate_session.c` per spec.md US1 Acceptance Scenario 1, SC-001

**Checkpoint**: JWT session activation works end-to-end. All P1 tests pass.

---

## Phase 4: User Story 2 — Multi-Issuer Configuration (Priority: P2)

**Goal**: Server supports multiple trusted OAuth2 issuers with independent keys, audiences, and clock skew. Issuer configuration is set at server init.

**Independent Test**: Configure two issuers with different keys, verify both accept valid JWTs and reject each other's audience.

### Tests for User Story 2

- [x] T021 [P] [US2] Add unit test: two-issuer config, JWT from issuer A accepted, JWT from issuer B accepted, cross-issuer audience mismatch rejected in `tests/unit/test_jwt_multi_issuer.c` per spec.md US2 Acceptance Scenarios
- [x] T022 [P] [US2] Add unit test: clock skew tolerance (token at boundary + skew accepted, beyond skew rejected) in `tests/unit/test_jwt_clock_skew.c` per FR-005

### Implementation for User Story 2

- [x] T023 [P] [US2] Implement issuer table lookup in `mu_jwt_validate()`: iterate `issuer_count` issuers, match `iss` claim, dispatch to that issuer's key and audience in `src/cu/core_2022_server/authorization/jwt.c` per FR-005
- [x] T024 [US2] Implement per-issuer clock skew tolerance in expiry/nbf checks in `src/cu/core_2022_server/authorization/jwt.c` per FR-005, Assumptions (last bullet)

**Checkpoint**: Multi-issuer support works. Both P2 tests pass. P1 tests still pass.

---

## Phase 5: User Story 3 — JWT Claims to User Identity (Priority: P3)

**Goal**: Extracted `sub` claim is correctly stored in session identity. Optional role claims are parsed.

**Independent Test**: JWT with `sub: "operator1"` → session reports "operator1".

### Tests for User Story 3

- [x] T025 [P] [US3] Add unit test: `sub` claim handling — valid sub, empty sub (rejected), 128-byte sub, sub with special characters in `tests/unit/test_jwt_claims.c` per spec.md SC-005, FR-006

### Implementation for User Story 3

- [x] T026 [US3] Wire `sub` claim → session user identity in `src/core/service_dispatch/activate_session.c` per FR-006, OPC-10000-4 §5.7.3
- [x] T027 [US3] Update session diagnostics: include user identity from JWT claims in `src/cu/core_2022_server/diagnostics/diagnostics.c` per OPC-10000-5 §6.3.5

**Checkpoint**: Identity mapping works. P3 tests pass. P1/P2 tests still pass.

---

## Phase 6: Address Space (CU 1629)

**Purpose**: AuthorizationServiceConfigurationType InstanceDeclarations for CU 1629 compliance. Gated on `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER`.

- [x] T028 Add `AuthorizationServiceConfigurationType` type-system InstanceDeclarations in `src/address_space/base_nodes.c` (gated on `MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`) per OPC-10000-12 §9.7.4, data-model.md
- [x] T029 [P] Add conformance doc for CU 1629 in `docs/conformance/authorization-service.md` per OPC-10000-7 §6.6
- [x] T030 [P] Add conformance doc for CU 1697 in `docs/conformance/jwt-user-token.md` per OPC-10000-7 §6.6
- [x] T031 Update `docs/conformance/opc-profile-roadmap.md` to claim CU 1629 and CU 1697 for full profile

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Size measurement, profile builds, traceability, final validation

- [x] T032 Run all profile builds (nano, micro, embedded, standard, full) and verify compile-out of JWT code when symbols undefined per spec.md SC-006
- [x] T033 Measure `.text` growth in standard profile and verify ≤5 KB per plan.md size budget, spec.md SC-007
- [x] T034 Run full test suite (`ctest --test-dir build/full --output-on-failure`) and verify zero regressions per spec.md SC-008
- [x] T035 Run clang-format, cppcheck, and clang-tidy; fix any violations
- [x] T036 Update `README.md` size table if ≥100 B delta
- [x] T037 Update `docs/traceability/files-to-sections.md` with new source and test files

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

---

## Phase 8: Convergence Remediation

**Purpose**: Close the implementation and verification gaps found by the post-implementation convergence assessment. Tests precede each behavior change.

### Authorization Service address-space gating

- [x] T038 [P] Add type-system tests in `tests/unit/test_type_system.c` proving NodeIds 17852-17855, their NodeClasses, DataTypes, PropertyType references, and Mandatory modelling rules are present only when `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` is enabled per OPC-10000-12 §9.7.4 Table 158 and spec.md FR-009/OPC-005
- [x] T039 Implement NodeIds 17853 `ServiceUri`, 17854 `ServiceCertificate`, and 17855 `IssuerEndpointUrl`, and gate NodeIds 17852-17855 on `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` in `src/address_space/base_nodes.c` per OPC-10000-12 §9.7.4 Table 158 and spec.md FR-009/OPC-005

### JWT endpoint advertisement

- [x] T040 [P] Add discovery tests in `tests/unit/test_discovery_services.c` proving JWT-enabled endpoints advertise an IssuedToken policy with token type URI `urn:ietf:params:oauth:token-type:jwt`, while JWT-disabled endpoints do not, per OPC-10000-4 §7.40.2.1, OPC-10000-6 §5.2.3, and spec.md FR-002/OPC-003
- [x] T041 Advertise the JWT IssuedToken policy without displacing existing Anonymous/UserName policies by updating policy storage and population in `src/services/discovery.h` and `src/services/discovery.c` per OPC-10000-4 §7.40.2.1, OPC-10000-6 §5.2.3, and spec.md FR-002/OPC-003

### JWT identity, roles, and key selection

- [x] T042 [P] Extend `tests/integration/test_jwt_activate_session.c` to assert that successful activation persists the JWT `sub` identity when redundancy is disabled and exposes that identity through session diagnostics per OPC-10000-5 §6.3.5/§6.4.7 and spec.md FR-006/SC-005
- [x] T043 Persist the validated JWT `sub` into session identity independently of `MUC_OPCUA_CU_REDUNDANCY` in `src/core/service_dispatch/activate_session.c` per OPC-10000-5 §6.4.7 and spec.md FR-006/SC-005
- [x] T044 [P] Add JWT claim-scanner tests for a bounded array of OPC UA role NodeIds and malformed/over-capacity role claims in `tests/unit/test_jwt_claims.c` per RFC 7519 §4 and spec.md US3 Acceptance Scenario 1
- [x] T045 [P] Extend `tests/integration/test_jwt_activate_session.c` to prove validated role NodeIds reach the active session and its diagnostics per OPC-10000-5 §6.3.5 and spec.md US1/US3 Acceptance Scenario 1
- [x] T046 Parse the optional OPC UA role-claim array into bounded `mu_jwt_claims_t` storage in `include/muc_opcua/authorization/jwt.h`, `src/cu/core_2022_server/authorization/claim_scanner.h`, and `src/cu/core_2022_server/authorization/claim_scanner.c` per RFC 7519 §4 and spec.md US3 Acceptance Scenario 1
- [x] T047 Map validated JWT role NodeIds into the session's RoleSet authorization state in `src/core/service_dispatch/activate_session.c` per spec.md US1 Acceptance Scenario 1 and US3 Acceptance Scenario 1
- [x] T048 Expose JWT user identity and mapped roles from active sessions in `src/cu/core_2022_server/diagnostics/diagnostics.c` per OPC-10000-5 §6.3.5 and spec.md SC-005/US3 Acceptance Scenario 1
- [x] T049 [P] Add JWT unit tests in `tests/unit/test_jwt.c` proving a known `kid` selects the matching trusted key and an unknown `kid` returns `MU_JWT_ERR_SIGNATURE` per RFC 7515 §4.1.4 and spec.md Edge Cases
- [x] T050 Add bounded trusted-key identifiers to `mu_jwt_issuer_t`, parse the protected-header `kid`, and reject unmatched key identifiers in `include/muc_opcua/server.h`, `include/muc_opcua/authorization/jwt.h`, and `src/cu/core_2022_server/authorization/jwt.c` per RFC 7515 §4.1.4 and spec.md Edge Cases

### Backend-neutral build gating and size proof

- [x] T051 [P] Add CMake/Kconfig build-matrix coverage proving JWT translation units compile for OpenSSL, mbedTLS, and wolfSSL backends and compile out when `MUC_OPCUA_CU_USER_TOKEN_JWT` is disabled per spec.md FR-008/FR-010/SC-006
- [x] T052 Gate JWT translation units on `MUC_OPCUA_CU_USER_TOKEN_JWT` rather than `MUC_OPCUA_HAVE_OPENSSL` alone in `src/CMakeLists.txt`, preserving the backend dispatch already implemented in `src/cu/core_2022_server/authorization/crypto_jwt.c`, per spec.md FR-008/FR-010
- [x] T053 Measure equivalent Standard-profile JWT-on and JWT-off Arm Cortex-M0+ builds with `scripts/measure_size.sh` or an equivalent reproducible target, record the `.text`/`.data` delta, and enforce zero growth when both symbols are off and ≤5 KB `.text` growth when JWT is on per spec.md SC-006/SC-007
- [x] T054 Rebuild the nano profile and run its complete CTest suite after T038-T053 per spec.md SC-008
- [x] T055 Rebuild the micro profile and run its complete CTest suite after T038-T053 per spec.md SC-008
- [x] T056 Rebuild the embedded profile and run its complete CTest suite after T038-T053 per spec.md SC-008
- [x] T057 Rebuild the standard profile with JWT and Authorization Service enabled and run its complete CTest suite after T038-T053 per spec.md SC-008
- [x] T058 Rebuild the full profile and run its complete CTest suite after T038-T053 per spec.md SC-008

**T053 footprint evidence (2026-07-25)**: Equivalent Cortex-M0+ Standard-profile
archive builds used `arm-none-eabi-gcc` with
`-mcpu=cortex-m0plus -mthumb -flto -ffat-lto-objects`,
`MUC_OPCUA_PLATFORM=arduino-skeleton`, and `MUC_OPCUA_OPTIMIZE_SIZE=ON`.
The default Standard baseline and the explicit
`MUC_OPCUA_CU_USER_TOKEN_JWT=OFF` /
`MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER=OFF` build both measured
103,101 B `.text`, 0 B `.data`, and 0 B `.bss` (zero disabled-feature
growth). Enabling both symbols measured 107,256 B `.text`, 0 B `.data`, and
0 B `.bss`: a 4,155 B `.text` increase, below the 5 KiB limit.
