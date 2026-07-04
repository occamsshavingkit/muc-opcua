# Tasks: Audit Follow-Up

**Input**: Design documents from `/specs/030-audit-followup/`
**Prerequisites**: plan.md, spec.md, research.md, contracts/fix-contracts.md, quickstart.md

**Tests**: Protocol/security changes are test-first per Constitution IV.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files)
- **[Story]**: US1 (nonce), US2 (zeroize), US3 (channel), US4 (cert)

---

## Phase 1: Setup

- [ ] T001 [P] Capture pre-change size baseline: `scripts/measure_size.sh all` → quickstart.md
- [ ] T002 [P] Capture pre-change test baseline: `ctest --test-dir build` → quickstart.md

---

## Phase 3: User Story 1 — Nonce fail-closed (Priority: P1)

**Goal**: ActivateSession rejects on entropy failure, consistent with CreateSession/OPN.

### Implementation for User Story 1

- [ ] T003 [US1] In `src/core/service_dispatch.c`, change `fill_server_nonce()` return type from `void` to `opcua_statuscode_t`; return `mu_session_generate_server_nonce` result directly (no fallback to zeros). OPC-10000-4 §5.7.3.

- [ ] T004 [US1] In `src/core/service_dispatch.c` `handle_activate_session`, move nonce generation before `write_response_prefix()` and check return value; reject with returned status on failure. OPC-10000-4 §7.38.2.

- [ ] T005 [US1] Fix `tests/unit/test_dispatch_services.c` `fake_entropy` callback — the signature `(void *c, opcua_byte_t *buf, size_t len)` already matches the adapter. Verify test continues to pass.

**Checkpoint**: Nonce fail-closed; test_dispatch_services green.

---

## Phase 4: User Story 2 — Password buffer zeroize (Priority: P1)

**Goal**: decrypt_buf zeroized at all exit points within MUC_OPCUA_SECURITY block.

### Implementation for User Story 2

- [ ] T006 [US2] In `src/core/service_dispatch.c`, move `decrypt_buf[256]` declaration and all `mu_secure_zero()` calls under `#ifdef MUC_OPCUA_SECURITY` guard. OPC-10000-4 §5.7.3.

- [ ] T007 [US2] Ensure zeroization occurs AFTER `user_auth_handler` call (not before — password must be valid when passed to handler). Zeroize at all 5 `goto activate_done` sites within SECURITY block and at natural exit. OPC-10000-4 §5.7.3.

- [ ] T008 [US2] Verify ARM nano/micro profiles compile clean (`scripts/measure_size.sh all`).

**Checkpoint**: Zeroization on all exit paths; nano/micro build green.

---

## Phase 5: User Story 3 — Channel entropy (Priority: P2)

**Goal**: Channel ID unique per open, not hardcoded to 1.

### Implementation for User Story 3

- [ ] T009 [US3] In `src/services/secure_channel.c` `mu_secure_channel_open`, use static incrementing counter for channel_id instead of hardcoded `1`. OPC-10000-6 §6.7.4.

- [ ] T010a [P] [US3] Update `tests/integration/test_view_services.c` to read channel_id from OPN response header instead of assuming value 1.

- [ ] T010b [P] [US3] Update `tests/integration/test_subscriptions.c` to read channel_id from OPN response header instead of assuming value 1.

- [ ] T010c [P] [US3] Update `tests/integration/test_user_auth_plaintext.c` to read channel_id from OPN response header instead of assuming value 1.

**Checkpoint**: Unique channel IDs; all integration tests green.

---

## Phase 6: User Story 4 — Self-cert validation (Priority: P2)

**Goal**: get_own_certificate failure propagated at server init.

### Implementation for User Story 4

- [ ] T011 [US4] In `src/core/server.c` `mu_server_config_validate`, propagate `get_own_certificate` failure when `config->crypto_adapter != NULL` and function pointer is set. OPC-10000-4 §5.5.

- [ ] T012 [US4] Verify test_view_services and test_server_config pass (no crypto adapter = check skipped).

**Checkpoint**: Self-cert validation fail-closed; no regressions.

---

## Phase 7: Polish

- [ ] T013 Run gate sweep: `ctest` green, `scripts/measure_size.sh all` ≤ +3% text, ≤ +5% data+bss.
- [ ] T014 [P] Update `docs/review/five-lens-audit-2026-07-04.md` with status of T1, T7, T17, T44.
- [ ] T015 [P] Update `quickstart.md` with final numbers.

---

## Dependencies & Execution Order

- **Setup** → US1/US2/US3/US4 in parallel (different files)
- **US1**: T003→T004→T005 (service_dispatch.c sequential)
- **US2**: T006→T007→T008 (service_dispatch.c sequential; depends on US1 for same-file coordination)
- **US3**: T009→T010a/T010b/T010c (test updates are [P] after code change)
- **US4**: T011→T012 (independent of all others)
- **Polish**: After all stories

## MVP Scope

US1 + US2 (T003-T008) — the two P1 security fixes. US3 + US4 can follow.

## Implementation Strategy

1. US1 first (nonce fail-closed, 3 tasks in service_dispatch.c)
2. US2 next (zeroize, 3 tasks in same file — avoid merge conflicts)
3. US3 + US4 in parallel (different files, fully independent)
4. Polish
