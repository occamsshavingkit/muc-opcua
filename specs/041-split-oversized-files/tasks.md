# Tasks: Split Oversized Source Files

**Feature**: 041-split-oversized-files

## Format: `- [ ] [ID] [P?] [Story?] Description with file path`

---

## Phase 1: Setup

- [ ] T001 Verify baseline tests pass on all profiles

---

## Phase 2: US1 — Core Dispatch Files (P1)

- [ ] T002 [US1] Split `src/core/service_dispatch.c` into `src/core/service_dispatch/`: session_handler.c, attribute_handler.c, view_handler.c, subscription_handler.c, transfer_handler.c, common.c. Update CMake. ctest after.
- [ ] T003 [US1] Split `src/core/dispatch_session.c` into `src/core/dispatch_session/`: create.c, activate.c, close.c, common.c. Update CMake. ctest after.
- [ ] T004 [US1] Split `src/core/dispatch_discovery.c` into `src/core/dispatch_discovery/`: get_endpoints.c, find_servers.c, register.c. Update CMake. ctest after.
- [ ] T005 [US1] Split `src/core/server.c` into `src/core/server/`: init.c, poll.c, chunk_io.c, dispatch.c. Update CMake. ctest after.

---

## Phase 3: US2 — Service Implementation Files (P2)

- [ ] T006 [US2] Split `src/services/subscription_publish.c` into `src/services/subscription_publish/`: publish.c, notification.c, deadband.c, retransmit.c. Update CMake. ctest after.
- [ ] T007 [US2] Split `src/services/read.c` into `src/services/read/`: read_attribute.c, read_process.c, cache.c. Update CMake. ctest after.
- [ ] T008 [US2] Split `src/services/browse.c` into `src/services/browse/`: browse.c, browse_next.c, translate_paths.c. Update CMake. ctest after.
- [ ] T009 [US2] Split `src/services/node_management.c` into `src/services/node_management/`: add_nodes.c, add_references.c, delete.c, common.c. Update CMake. ctest after.

---

## Phase 4: US3 — Security and Platform Adapters (P3)

- [ ] T010 [US3] Split `src/security/asym_chunk.c` into `src/security/asym_chunk/`: wrap.c, unwrap.c, common.c. Update CMake. ctest after.
- [ ] T011 [US3] Split `src/platform/host_crypto_adapter.c` into `src/platform/host_crypto/`: cipher.c, asymmetric.c, hash.c, sign.c. Update CMake. ctest after.
- [ ] T012 [US3] Split `src/platform/wolfssl_crypto_adapter.c` into `src/platform/wolfssl_crypto/`: cipher.c, asymmetric.c, hash.c, sign.c. Update CMake. ctest after.

---

## Phase 5: Polish

- [ ] T013 Run full test suite on default, micro, standard profiles
- [ ] T014 Run clang-format on all new files
- [ ] T015 Update TODO.md

---

## Dependencies

All 11 splits are independent (different source directories). Phases ordered
by priority but can run in parallel.

## Task Summary

| Phase | Tasks | Count |
|-------|-------|-------|
| Setup | T001 | 1 |
| US1: Core dispatch | T002-T005 | 4 |
| US2: Service impl | T006-T009 | 4 |
| US3: Adapters | T010-T012 | 3 |
| Polish | T013-T015 | 3 |
| **Total** | | **15** |
