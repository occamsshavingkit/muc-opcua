# Tasks: Decompose Oversized Functions

**Feature**: 040-decompose-oversized
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)
**Generated**: 2026-07-05

## Format: `- [ ] [ID] [P?] [Story?] Description with file path`

---

## Phase 1: Setup

- [ ] T001 Verify baseline: `cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build . && ctest --output-on-failure` — record that all pass

---

## Phase 2: US1 — Session Dispatch Decomposition (P1)

**Goal**: Decompose 3 functions in `dispatch_session.c`

- [ ] T002 [US1] Decompose `handle_create_session` (263 lines) in `src/core/dispatch_session.c` — extract helpers for: encode-routing-info, encode-server-endpoints, encode-server-signature, build-discovery-url. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T003 [US1] Decompose `handle_activate_session` (246 lines) in `src/core/dispatch_session.c` — extract helpers for: decode-client-software-certs, decode-user-identity-token, verify-and-activate. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T004 [US1] Decompose `read_create_session_request` (109 lines) in `src/core/dispatch_session.c` — extract helpers for: decode-session-name, decode-endpoint-urls. Parent ≤ 100 lines. Rebuild and test after.

---

## Phase 3: US2 — Service Dispatch Decomposition (P1)

**Goal**: Decompose 3 functions in `service_dispatch.c`

- [ ] T005 [US2] Decompose `handle_modify_monitored_items` (210 lines) in `src/core/service_dispatch.c` — extract helpers for: filter-resolution, node-validation, item-update, response-encode. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T006 [US2] Decompose `handle_open_secure_channel` (149 lines) in `src/core/service_dispatch.c` — extract helpers for: security-policy-select, nonce-validate, response-encode. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T007 [US2] Decompose `handle_create_monitored_items` (98 lines) in `src/core/service_dispatch.c` — extract helpers for: filter-resolution, item-allocation. Parent ≤ 100 lines. Rebuild and test after.

---

## Phase 4: US3 — Server Core Decomposition (P2)

**Goal**: Decompose 4 functions in `server.c`

- [ ] T008 [US3] Decompose `process_message` (183 lines) in `src/core/server.c` — extract helpers for: process-hello, process-opn-or-msg, process-multi-chunk-continuation, process-multi-chunk-final. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T009 [US3] Decompose `handle_data_chunk_secure` (134 lines) in `src/core/server.c` — extract helpers for: opn-path, msg-path. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T010 [US3] Decompose `handle_data_chunk_plaintext` (123 lines) in `src/core/server.c` — extract helpers for: opn-setup, sequence-check, dispatch. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T011 [US3] Decompose `mu_server_poll` (102 lines) in `src/core/server.c` — extract helpers for: connection-accept, read-process, timeout-maintenance. Parent ≤ 100 lines. Rebuild and test after.

---

## Phase 5: US4 — Crypto / Subscription Decomposition (P3)

**Goal**: Decompose 5 functions in `asym_chunk.c` + `subscription_publish.c`

- [ ] T012 [US4] Decompose `mu_asym_chunk_wrap` (166 lines) in `src/security/asym_chunk.c` — extract helpers for: policy-dispatch, cleartext-header, padding, signing, encrypt. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T013 [US4] Decompose `mu_asym_chunk_unwrap` (182 lines) in `src/security/asym_chunk.c` — extract helpers for: decrypt, signature-verify, payload-extract. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T014 [US4] Decompose `write_data_change_notification` (168 lines) in `src/services/subscription_publish.c` — extract helpers for: notification-header, payload-per-item, diagnostics. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T015 [US4] Decompose `publish_due` (139 lines) in `src/services/subscription_publish.c` — extract helpers for: subscription-select, notification-build, response-encode. Parent ≤ 100 lines. Rebuild and test after.
- [ ] T016 [US4] Decompose `build_publish_response` (117 lines) in `src/services/subscription_publish.c` — extract helpers for: header-write, results-write. Parent ≤ 100 lines. Rebuild and test after.

---

## Phase 6: US5 — Read Decomposition (P3)

**Goal**: Decompose 1 function in `read.c`

- [ ] T017 [US5] Decompose `read_attribute` (102 lines) in `src/services/read.c` — extract helpers for: attribute-type-dispatch. Parent ≤ 100 lines. Rebuild and test after.

---

## Phase 7: Polish

- [ ] T018 Run full test suite all profiles: default, micro, standard — verify all pass
- [ ] T019 Run clang-format check: `find src -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror`
- [ ] T020 Update TODO.md: mark CX1-CX16 as completed

---

## Dependencies

```
T001 (baseline)
 ├─ US1 (T002-T004) ── sequential within file
 ├─ US2 (T005-T007) ── sequential within file
 ├─ US3 (T008-T011) ── sequential within file (functions are interconnected)
 ├─ US4 (T012-T016) ── sequential within each file
 ├─ US5 (T017) ── independent
 └─ T018-T020 (polish) ── depends on all above
```

Each phase modifies a single file. Tasks within a phase are sequential (same file). Phases can run in parallel if dispatched to separate agents (different files).

## Task Summary

| Phase | Tasks | Story | Count |
|-------|-------|-------|-------|
| 1: Setup | T001 | — | 1 |
| 2: US1 dispatch_session.c | T002-T004 | US1 | 3 |
| 3: US2 service_dispatch.c | T005-T007 | US2 | 3 |
| 4: US3 server.c | T008-T011 | US3 | 4 |
| 5: US4 asym/sub | T012-T016 | US4 | 5 |
| 6: US5 read.c | T017 | US5 | 1 |
| 7: Polish | T018-T020 | — | 3 |
| **Total** | | | **20** |

---

## Implementation Strategy

**Per function**: extract helpers → rebuild → ctest → commit.
**Per AGENTS.md**: one task per subagent, sequential within same file.
