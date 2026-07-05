# Implementation Plan: Clear Remaining Backlog

**Branch**: `039-clear-remaining-backlog` | **Date**: 2026-07-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/039-clear-remaining-backlog/spec.md`

## Summary

Fix all 43 remaining TODO.md backlog items deferred from Spec 038: 3 verified
bugs, 13 hot-path performance issues, 17 medium-severity review findings, 5
interop hardening tasks, and 2 documentation tasks. No new features or
conformance units. All changes are behavior-preserving fixes, performance
improvements, and documentation additions to existing code.

## Technical Context

**Language/Version**: C11 (freestanding, C99-compatible subset)
**Primary Dependencies**: CMake >= 3.16, Unity test framework, CMocka where needed
**Storage**: Static-only; no heap allocation on protocol hot path; static tables
**Testing**: Unity (unit), CMocka (integration/session), ASAN/UBSan, fuzz targets (libFuzzer)
**Target Platform**: Host (GCC/Clang) for tests; ARM Cortex-M0+ (RP2040/Pico SDK) for embedded; Arduino (PlatformIO) secondary
**Project Type**: Portable C library (embedded OPC UA server) — backlog cleanup
**Performance Goals**: Hot-path fixes should improve measured throughput (HP2 cache fix ~20%), reduce poll cycle CPU (HP3), eliminate memory leaks (HP1)
**Constraints**: Zero regression on existing tests; Flash/RAM must not grow >1%
**Scale/Scope**: 43 fixes across ~20 source files and ~10 test files
**OPC UA Normative References**: OPC-10000-3 (Address Space), OPC-10000-4 (Services), OPC-10000-6 (Mappings), OPC-10000-7 (Profiles) — used primarily for spec grounding comments (US6)
**Target OPC UA Profile/Conformance Units**: Standard 2017 UA Server Profile (no change)
**Conformance Status Target**: profile-targeting (no change)

## Embedded Size Budget

**Flash Impact**: Net neutral or negative (bug fixes and code deduplication remove code; spec comments and ADRs add negligible bytes). Estimated -0.2 KB to +0.3 KB.
**RAM Impact**: Zero new static data. Hot-path fixes may reduce stack usage (HP7 removes zero-init). Read cache (HP2) already allocated — just logic fix. No new buffers.
**Heap Use**: None on protocol hot path. HP1 fix may add an explicit `free()` call path that already existed implicitly (variant array cleanup).
**Static Tables Added**: None. ADRs are documentation files outside binary.
**Transport Buffers**: Unchanged.
**Crypto Dependency Impact**: None. SE fixes are buffer-size and comparison-method changes; no new crypto algorithms.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. Bug fixes (US1) correct behavior to match OPC UA spec requirements. Spec grounding comments (US6) explicitly cite OPC-10000 sections. Security fixes (US4) align with spec-required cryptographic boundaries.
- **Embedded C Core**: PASS. All changes are to existing C11 code. Portable fallback for `__builtin_ctz` (CQ6) improves portability. UB fixes (UB3-UB7) improve compiler-agnostic correctness.
- **Memory Model**: PASS. HP1 fixes a memory leak, not adding new allocations. HP7 removes wasteful zero-initialization. Read cache (HP2) uses existing static storage. No new heap allocations added.
- **Minimal OPC UA Surface**: PASS. No new services, transports, or encodings. No XML/JSON/HTTPS.
- **Profile Research**: PASS. No profile changes. Standard 2017 UA Server Profile remains target.
- **Correctness Gates**: PASS. Test coverage improvements (US5) add behavioral assertions replacing placeholders. Interop tests expand to Subscribe/Publish and Call services. Bug fixes include unit tests.
- **Security Honesty**: PASS. SE1-SE4 fix security-sensitive code without weakening existing protections. No new security claims made.
- **Toolchain Discipline**: PASS. CMake, Unity/CMocka, ASAN/UBSan, clang-format unchanged. Portable CTZ fallback tested on existing CI matrix.
- **Size Discipline**: PASS. Flash/RAM impact negligible. Code deduplication (CQ7, CQ8) may reduce text size. ADRs are docs, not compiled.

## Project Structure

### Documentation (this feature)

```text
specs/039-clear-remaining-backlog/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── quickstart.md        # Phase 1 output
├── data-model.md        # Phase 1 output (N/A — no new data model)
├── contracts/           # Phase 1 output (N/A — no new contracts)
└── tasks.md             # Phase 2 output (/speckit-tasks)
```

### Source Code (repository root)

All changes are modifications to existing files. No new source files required
(ADRs in `docs/adr/` are the only new files).

```text
# Bug Fixes (US1)
src/core/tcp_connection.c          # FR-001: protocol version constant
src/core/service_dispatch.c        # FR-002, FR-003: ModifyMonitoredItems fixes

# Hot-Path Performance (US2)
src/encoding/binary_variant.c      # FR-004: variant array leak free()
src/services/read.c                # FR-005: cache enable + FR-010: zero-init removal
src/core/server.c                  # FR-006: tick_ms batching + FR-008: memmove + FR-011: session scan + FR-016: listen() defer
src/core/dispatch_session.c        # FR-007: auth token parse
src/encoding/binary_reader.c       # FR-008: ensure_bytes batching
src/encoding/binary_writer.c       # FR-009: ensure_space batching
src/core/dispatch_attribute.c      # FR-013: write type-check
src/address_space/address_space.c  # FR-014: duplicate check
src/core/message_chunk.c           # FR-015: MessageSize parse

# Code Quality & UB (US3)
src/core/ctz.h or similar           # FR-017: portable CTZ
src/encoding/ (multiple)            # FR-018: LE uint32 consolidation
src/core/server.c                   # FR-019: dispatch dedup (extends spec 038 CQ2 fix)
src/core/server_internal.h          # FR-020: conditional subscription include
src/services/history.c              # FR-021: error return
src/encoding/binary_string.c        # FR-022: strict aliasing
README.md or AGENTS.md              # FR-023: C11 requirement doc
src/core/safe_mem.c or similar      # FR-024: int32_t cast guard
src/encoding/binary_nodeid.c        # FR-025: expanded nodeid flags
src/ (multiple)                     # FR-026: calloc→static/stack

# Security (US4)
src/security/ (kdf, decrypt)        # FR-027, FR-028: nonce + RSA buffer
src/security/trust_list.c           # FR-029: constant-time memcmp
src/services/event_filter.c         # FR-030: select_count cap

# Tests (US5)
tests/unit/                         # FR-031-FR-033: placeholder replacements + new tests
tests/interop/interop_smoke.py      # FR-034-FR-038: interop hardening + new service coverage

# Docs (US6)
docs/adr/                           # FR-039: ADR files
src/encoding/binary_nodeid.c        # FR-040: spec comments
src/encoding/binary_datavalue.c
src/core/uasc.c
include/muc_opcua/encoding.h       # FR-041: spec comments
include/muc_opcua/server.h
```

**Structure Decision**: All fixes modify existing files under their current locations. ADR files are created in `docs/adr/` (new directory). No source module splits or reorganizations — scope is strictly fix-and-document.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitution violations. All changes are within established architecture and constraints.
