# Tasks: Standard UA Client 2025 Profile

**Feature**: 049-standard-client-profile
**Input**: Design documents from `specs/049-standard-client-profiles/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/api.md, quickstart.md

**Tests**: Tests are mandatory for client service dispatch (subscription lifecycle,
write round-trip, method call round-trip, event notification parsing), wire-visible
behavior, and StatusCode propagation.

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
- Include exact file paths in descriptions
- Include OPC UA part/section references in protocol task descriptions

## Path Conventions

- **Public API**: `include/muc_opcua/`
- **Client implementation**: `src/client/`
- **Build system**: `src/CMakeLists.txt`
- **Tests**: `tests/unit/`

---

## Phase 1: Setup

- [x] T001 Add `MUC_OPCUA_CLIENT_PROFILE` CMake option with `standard` value and
  `MUC_OPCUA_CLIENT_STANDARD` gate in `CMakeLists.txt` (root + src), default `nano`
- [x] T002 Gate macro `MUC_OPCUA_CLIENT_STANDARD` auto-defined when
  `MUC_OPCUA_CLIENT_PROFILE=standard`

---

## Phase 2: Foundational (API Headers)

- [x] T003 Add subscription/monitored-item/publish API declarations to
  `include/muc_opcua/client.h` (FR-001 to FR-004, OPC-10000-4 §5.13–5.14)
- [x] T004 Add write/call API declarations to `include/muc_opcua/client.h`
  (FR-008, FR-009, OPC-10000-4 §5.11.4, §5.12.2)

---

## Phase 3: User Story 1 — Subscription and Publish

- [x] T005 [US1] Create `src/client/client_subscription.c` — subscription create/delete
  (FR-001, FR-002, §5.14.2, §5.14.8)
- [x] T006 [US1] Create `src/client/client_monitored_item.c` — monitored item
  create/delete with DataChangeFilter (FR-003, FR-004, FR-007, §5.13.2, §5.13.6)
- [x] T007 [US1] Create `src/client/client_publish.c` — Publish request parking,
  notification dispatch (FR-005, FR-006, §5.14.5)
- [x] T008 [US1] Wire new source files in `src/CMakeLists.txt` gated on
  `MUC_OPCUA_CLIENT_STANDARD`

---

## Phase 4: User Story 1 — Write, Method Call, and Events

- [x] T009 [P] [US1] Create `src/client/client_write.c` — Write service
  (FR-008, §5.11.4)
- [x] T010 [P] [US1] Create `src/client/client_call.c` — Call service
  (FR-009, §5.12.2)
- [x] T011 [P] [US1] Extend `src/client/client_publish.c` — EventNotificationList
  parsing (FR-010, §5.14.5.2, OPC-10000-5 §6.4.2)
- [x] T012 [US1] Wire new source files in `src/CMakeLists.txt`

---

## Phase 5: Tests

- [x] T013 [US1] Add subscription round-trip test in `tests/unit/test_nano_client.c`
  (OPC-10000-4 §5.13–5.14)
- [x] T014 [P] [US1] Add write round-trip test (OPC-10000-4 §5.11.4)
- [x] T015 [P] [US1] Add method call test (OPC-10000-4 §5.12.2)
- [x] T016 [P] [US1] Add event subscription test (OPC-10000-4 §5.14.5.2, OPC-10000-5 §6.4.2)
- [x] T017 Add error-path test (OPC-10000-4 §5.11.4, §5.12.2)

---

## Phase 6: Validation

- [x] T018 CTest on standard profile — 125/125 pass, zero regressions (SC-005)
- [x] T019 CTest on nano profile — 89/89 pass, unchanged (SC-006)
- [x] T020 Run `clang-format -i` on new files
- [x] T021 Update `docs/traceability/files-to-sections.md` with new client files

---

## Task Summary

| Phase | Story | Count | Description |
|-------|-------|-------|-------------|
| Setup | — | 2 | CMake profile gating |
| Foundational | US1 | 2 | API header declarations |
| Subscription/Publish | US1 | 4 | Subscriptions, monitored items, publish |
| Write/Call/Events | US1 | 4 | Write, call, event parsing |
| Tests | US1 | 5 | Subscription/write/call/event tests |
| Validation | — | 4 | ctest, clang-format, traceability |
| **Total** | | **21** | |
