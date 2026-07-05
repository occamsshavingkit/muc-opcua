# Research: Decompose Oversized Functions

**Feature**: 040-decompose-oversized
**Date**: 2026-07-05

## Decision 1: Decomposition Pattern

**Decision**: Each oversized function is refactored by extracting sub-concerns
into `static` helper functions in the same file. The original function becomes
a short orchestrator calling helpers in sequence. Helpers are named with a
consistent prefix based on the parent function name.

**Rationale**: This is the standard C refactoring pattern for long functions.
Static helpers have zero linkage overhead and the compiler inlines them
identically to the original monolithic code. No new header files, no API
surface changes.

**Alternatives considered**:
- Extract to separate `.c` files: Adds compilation units, complicates build,
  overkill for single-function helpers.
- Inline functions in header: Creates header dependency issues and compile-
  time visibility for test code. Unnecessary.

## Decision 2: Regression Testing Protocol

**Decision**: After each function is decomposed, rebuild and run the full test
suite. If any test fails, revert and debug before moving to the next function.
Target: one function decomposed per commit.

**Rationale**: Decomposing 16 functions sequentially means any failure can be
immediately attributed to the last change. Single-function commits keep the
git history clean.

**Alternatives considered**:
- Decompose all functions then test: Too risky — pinpointing which
  decomposition broke tests would be difficult.
- Add new tests per helper: Out of scope for this spec. Existing tests
  already cover all code paths through the parent functions.

## Decision 3: Helper Size Target

**Decision**: Each extracted helper targets ≤ 50 lines. If a logical sub-concern
exceeds 50 lines, it is split further. The parent function after decomposition
targets ≤ 100 lines.

**Rationale**: The audit found functions > 100 lines; 50-line helpers are small
enough to be clearly single-purpose. The parent function at ≤ 100 lines is the
minimum spec requirement.

## Decision 4: Order of Execution

**Decision**: Decompose functions in order: dispatch_session.c (US1) →
service_dispatch.c (US2) → server.c (US3) → asym_chunk.c → subscription_publish.c
→ read.c (US4-5). Within each file, decompose the longest function first.

**Rationale**: dispatch_session.c has the two largest functions in the codebase
(263 and 246 lines). Starting there gives the largest complexity reduction
earliest. Server.c functions are interconnected (process_message calls the
chunk handlers), so they must be decomposed together.
