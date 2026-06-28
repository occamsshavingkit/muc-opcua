# Tasks: Memory and Performance Optimizations

**Input**: Design documents from `/specs/006-implement-memory-and/`
**Prerequisites**: plan.md (required), spec.md (required for user stories)

**Tests**: Tests are mandatory for all protocol parsing, serialization, and structure modifications.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)

---

## Phase 1: Setup (Shared Infrastructure)

- [X] T001 Configure C11 host build and verify compiler warnings are treated as errors in CMakeLists.txt
- [X] T002 Establish size budget measurement command in scripts/measure_size.sh

---

## Phase 2: Foundational (Blocking Prerequisites)

- [X] T003 Verify existing session and handshake unit tests compile and run under tests/unit/
- [X] T004 Verify static analysis tools compile cleanly on the target branch

---

## Phase 3: User Story 1 - Session Timeout 4-byte Integer (Priority: P1)

**Story Goal**: Reduce internal session storage by converting raw double bits to 4-byte millisecond representation.

**Independent Test**: Session initialization unit tests must verify revised timeouts remain correct on the wire while the structure size is reduced.

### Tests for User Story 1

- [X] T005 [P] [US1] Add unit test checking session timeout clamping bounds and wire encoding/decoding in tests/unit/test_session.c

### Implementation for User Story 1

- [X] T006 [US1] Change revised_session_timeout_bits from 8-byte double to 4-byte ms integer in src/services/session.h
- [X] T007 [US1] Update CreateSession request handling to convert double bits to ms, clamp inside integer limits, and reconstruct double bits in response in src/services/session.c

---

## Phase 4: User Story 2 - Asymmetric Unwrap Stack Reduction (Priority: P2)

**Story Goal**: Eliminate 6 KB stack allocations in asymmetric unwrapping by passing scratch references.

**Independent Test**: OpenSecureChannel integration tests must run successfully with reduced stack allocation.

### Tests for User Story 2

- [X] T008 [P] [US2] Add unit test checking asymmetric decryption on corrupted/malformed chunks in tests/unit/test_asym_chunk.c

### Implementation for User Story 2

- [X] T009 [US2] Update mu_asym_chunk_unwrap to accept scratch buffer parameters and remove internal static stack arrays in src/security/asym_chunk.c and src/security/asym_chunk.h
- [X] T010 [US2] Pass server->secure_scratch as the scratch buffer to mu_asym_chunk_unwrap in src/core/server.c

---

## Phase 5: User Story 3 - Default Cipher Context Size (Priority: P3)

**Story Goal**: Reduce the default size of MU_CIPHER_CTX_SIZE from 512 to 32 bytes for pointer-based adaptors.

**Independent Test**: Verify host build successfully compiles and has smaller BSS size.

### Tests for User Story 3

- [X] T011 [P] [US3] Add compile-time check for MU_CIPHER_CTX_SIZE bounds in tests/unit/test_sym_chunk.c

### Implementation for User Story 3

- [X] T012 [US3] Modify default macro definition of MU_CIPHER_CTX_SIZE to be 32 bytes when using pointer-based adaptors in include/micro_opcua/config.h

---

## Phase 6: Compliance, Size, and Polish

- [X] T013 Run host unit and integration tests via make test
- [X] T014 Measure and record final Flash and RAM sizes in plan.md
