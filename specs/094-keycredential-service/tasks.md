# Tasks: KeyCredential Service Server Facet

**Input**: Design documents from `specs/094-keycredential-service/`

## Format: `[ID] [P?] [Story] Description`

---

## Phase 1: Setup

- [ ] T001 Add `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE` Kconfig symbol (depends on `MUC_OPCUA_CU_METHOD_SERVER`, default `y` for full) in `Kconfig`
- [ ] T002 [P] Create `include/muc_opcua/services/key_credential.h` with `mu_key_credential_adapter_t` struct and method NodeId constants (17516, 17519, 17521, 17522) per OPC-10000-12 §8.5-8.6
- [ ] T003 [P] Add `const mu_key_credential_adapter_t *key_credential_adapter` to `mu_server_config_t` in `include/muc_opcua/server.h` (gated on `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE`)
- [ ] T004 Add CMake gating for `src/cu/core_2022_server/key_credential/` in `src/CMakeLists.txt`

---

## Phase 2: Method Handlers

- [ ] T005 [P] [US1] Implement `mu_key_credential_get_encrypting_key()` handler: call adapter or fall back to server cert public key, encode Call response per OPC-10000-4 §5.11 in `src/cu/core_2022_server/key_credential/key_credential.c`
- [ ] T006 [P] [US2] Implement `mu_key_credential_create_credential()` handler: extract ResourceUri + credential ByteString + keyId from Call args, dispatch to adapter in `src/cu/core_2022_server/key_credential/key_credential.c`
- [ ] T007 [P] [US3] Implement `mu_key_credential_update_credential()` and `mu_key_credential_delete_credential()` handlers in `src/cu/core_2022_server/key_credential/key_credential.c`
- [ ] T008 [US1] Register all 4 methods via `mu_server_register_method_callback()` in server init when adapter is configured. Missing adapter → methods return Bad_NotSupported.

---

## Phase 3: Address Space

- [ ] T009 Add KeyCredentialConfigurationType(17533) InstanceDeclarations (ResourceUri=17512, ProfileUri=17513, EndpointUrls=17514, ServiceStatus=17515, GetEncryptingKey Method=17516) in `src/address_space/base_nodes.c` gated on `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`
- [ ] T010 [P] Add KeyCredentialConfigurationFolderType(17496) InstanceDeclarations (ServiceName_Placeholder=17511, CreateCredential=17522, UpdateCredential=17519, DeleteCredential=17521) in `src/address_space/base_nodes.c`

---

## Phase 4: Tests

- [ ] T011 [P] [US1] Unit test: GetEncryptingKey with adapter → Good + valid key; without adapter → Bad_NotSupported in `tests/unit/test_key_credential.c`
- [ ] T012 [P] [US2] Unit test: CreateCredential with adapter → Good; full store → Bad_ResourceUnavailable in `tests/unit/test_key_credential.c`
- [ ] T013 [P] [US3] Unit test: UpdateCredential + DeleteCredential round-trip in `tests/unit/test_key_credential.c`

---

## Phase 5: Polish

- [ ] T014 [P] Run `clang-format` on all new files
- [ ] T015 Run full profile build + test suite, verify zero regressions
- [ ] T016 [P] Add conformance doc `docs/conformance/key-credential-service.md` per OPC-10000-12 §8.5-8.6
- [ ] T017 Update `docs/conformance/status.md` embedded table with KeyCredential row
- [ ] T018 Verify compile-out on nano/micro profiles (zero `.text` growth)

---

## Dependencies

- Setup (T001-T004) blocks all
- Handlers (T005-T008) depend on Setup
- Address space (T009-T010) depends on Setup, independent of Handlers
- Tests (T011-T013) depend on Handlers
- Polish depends on all

### MVP Scope

T001-T005 + T008 + T011 = working GetEncryptingKey with adapter.
