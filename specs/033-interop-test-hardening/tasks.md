# Tasks: Interop Test Hardening

**Input**: Design documents from `specs/033-interop-test-hardening/`
**Tests**: This feature IS about tests. No implementation code changes.

---

## Phase 1: Audit Response Encoder Tests (US1)

- [x] T001 [US1] Audit `tests/unit/test_write_decoder.c` — verify `test_write_response_encode` reads diagnosticInfos to end. Already fixed in PR #241. Mark as verified.

- [x] T002 [US1] Audit `tests/unit/test_read_decoder.c` — check `test_read_response_encode` reads all fields (results[], diagnosticInfos[]) to end of buffer.

- [x] T003 [US1] Audit `tests/unit/test_browse_decoder.c` — check `test_browse_response_encode` reads all fields (results[], diagnosticInfos[]) to end of buffer.

- [x] T004 [US1] Audit any other response encoder tests in `tests/unit/` for the pattern: grep for `response_encode` in test files, verify each reads all mandatory fields through to end of buffer.

- [x] T005 [US1] For any test found deficient, add end-of-buffer validation: `TEST_ASSERT_EQUAL(w.position, r.position)` or read remaining fields to completion.

---

## Phase 2: Add Write Interop Test (US2)

- [x] T006 [US2] Read `tests/interop/interop_smoke.py` to understand the existing test structure and patterns.

- [x] T007 [US2] Add a write interop test to `tests/interop/interop_smoke.py` that: connects to the server, creates a session, writes a value to a variable, and verifies the WriteResponse is decoded without error.

- [x] T008 [US2] Verify the write interop test passes: start the muc-opcua server, run the interop test.

---

## Phase 3: Verify Interop Coverage (US3)

- [x] T009 [US3] Inventory all interop tests and map to service sets: Read, Write, Browse, Subscribe/Publish, CreateSession/ActivateSession. Identify gaps.

- [x] T010 [US3] Run all interop tests: `ctest -R interop` — verify all pass.

---

## Dependencies

- T001-T005 (audit): independent, can run in parallel
- T006 (read smoke): prerequisite for T007
- T007 (add write test): depends on T006
- T008 (verify write): depends on T007
- T009-T010 (coverage): independent after US2
