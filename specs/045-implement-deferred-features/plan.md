# Implementation Plan: Implement Deferred Features (D1-D4)

**Branch**: `045-implement-deferred-features` | **Date**: 2026-07-06 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/045-implement-deferred-features/spec.md`

## Summary

Implement 4 deferred features from TODO.md: D1 (complex type binary encode/decode),
D2 (audit event callback dispatch), D3 (39 additional OPC-10000-13 aggregate
functions), D4 (linked-ELF binary size measurement with JSON reporting).

All features gate on existing feature flags (`MUC_OPCUA_COMPLEX_TYPES`,
`MUC_OPCUA_AUDITING`, `MUC_OPCUA_AGGREGATE_FULL`) with zero impact on existing
profiles when flags are OFF. The research phase uncovered that CMake compile
definitions for several feature flags are missing from `src/CMakeLists.txt`,
which must be fixed as a prerequisite.

## Technical Context

**Language/Version**: C11 (freestanding with C99-compatible subset)
**Primary Dependencies**: Unity test framework, project-internal API headers
**Storage**: N/A (in-process: server BSS, fixed-size arrays)
**Testing**: ctest via Unity, `cmake --build . && ctest`
**Target Platform**: Host (Linux) for tests; ARM Cortex-M0+ for size measurement (D4)
**Project Type**: C library
**Performance Goals**: All tests complete in <10 seconds total (expanded from ~5s due to aggregate test count)
**Constraints**: Tests gate on same `MUC_OPCUA_*` feature flags as implementation; zero production code size impact when flags OFF
**Scale/Scope**: 4 feature areas, ~50 test cases total, ~29 source files modified
**OPC UA Normative References**:
  - D1: OPC-10000-6 §5.2.2.12 (Structure), §5.2.2.9 (Enumeration), §5.4.1 (Structure encoding body)
  - D2: OPC-10000-5 §6.5.2 (EventType), §6.5.3 (AuditEventType)
  - D3: OPC-10000-13 §4.2.2 (AggregateFunctionType), §5.4.3 (AggregateFilter), §5.4.4 (AggregateConfiguration)
  - D4: N/A (development tooling)
**Target OPC UA Profile/Conformance Units**: AggregateServerBase conformance unit (OPC-10000-7) for D3; StructureServerBase for D1; AuditServerBase for D2
**Conformance Status Target**: experimental (tests added, no CTT verification)

## Embedded Size Budget

**Flash Impact**:
  - D1 (complex types): ~1-2 KB flash when `MUC_OPCUA_COMPLEX_TYPES=ON`
  - D2 (audit events): ~0.5 KB flash when `MUC_OPCUA_AUDITING=ON`
  - D3 (aggregate functions): ~2-4 KB flash when `MUC_OPCUA_AGGREGATE_FULL=ON`
  - D4 (measurement tooling): 0 (script only)
  - All features OFF: 0 impact on existing profiles
**RAM Impact**:
  - D1: No static RAM increase (encoders use caller-provided writer/reader buffers)
  - D2: 4 × (function pointer + void*) = ~32-64 bytes for audit callback array
  - D3: Expanded accumulator union ~32 additional bytes when `MUC_OPCUA_AGGREGATE_FULL=ON`
  - D4: 0
**Heap Use**: 0 (all features use static allocation or caller-provided buffers)
**Static Tables Added**:
  - D3: 39 new aggregate type ID constants in `opcua_ids.h` (156 bytes in ROM if referenced)
**Transport Buffers**: N/A (no transport layer changes)
**Crypto Dependency Impact**: N/A (no security policy changes)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. Every implemented behavior cites exact OPC UA sections
  (OPC-10000-6 §5.2.2.12, OPC-10000-5 §6.5.2-3, OPC-10000-13 §4.2.2).
  Unsupported aggregate types return `BAD_MONITOREDITEMFILTERUNSUPPORTED` per
  OPC-10000-4 §7.22. Bad decode returns `Bad_DecodingError`.
- **Embedded C Core**: PASS. All implementations use freestanding C11. Encoders
  operate on caller-provided buffers via `mu_binary_writer_t`/`mu_binary_reader_t`.
  No OS dependencies. Audit callback storage is inline in `struct mu_server`.
- **Memory Model**: PASS. Protocol hot path (encode/decode, aggregate accumulate/
  publish) uses `mu_binary_writer_t`/`mu_binary_reader_t` (caller-provided buffers)
  and inline BSS in `mu_server`/`mu_monitored_item_t`. No heap allocation.
- **Minimal OPC UA Surface**: PASS. All features gate on existing profile flags.
  No new transport, encoding, or service-set additions.
- **Profile Research**: PASS. AggregateServerBase, StructureServerBase, and
  AuditServerBase conformance units identified from OPC-10000-7.
- **Correctness Gates**: PASS. D1: round-trip encode→decode with byte-level
  verification. D2: callback dispatch with ordered invocation verification.
  D3: per-function known-input/expected-output vectors. D4: measurement script
  produces deterministic, verifiable output.
- **Security Honesty**: PASS. No security policy changes. Audit callback dispatch
  is additive (no removal of existing NULL-safety). Server fields are `const`.
- **Toolchain Discipline**: PASS. Tests use Unity + ctest. Build via CMake.
  Cross-compile for D4 uses `arm-none-eabi-gcc`.
- **Size Discipline**: PASS. All features have estimated flash/RAM impact documented
  above. Existing profiles unchanged when flags OFF. D4 produces linked-ELF
  measurements for accurate size tracking.

## Project Structure

### Documentation (this feature)

```text
specs/045-implement-deferred-features/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (public API contracts)
└── tasks.md             # Phase 2 output (NOT created by /speckit-plan)
```

### Source Code (modified files)

```text
# D1 — Complex Types (5 files)
include/muc_opcua/address_space/complex_types.h  # Add lookup function declaration
src/encoding/binary_complex.c                    # Replace stub with encoder/decoder
src/address_space/complex_types.c                # Add lookup-by-NodeId helper
src/CMakeLists.txt                               # Add MUC_OPCUA_COMPLEX_TYPES gate
tests/unit/test_complex_types.c                  # Add round-trip encode/decode tests

# D2 — Audit Events (5 files)
include/muc_opcua/services/audit.h              # Add mu_audit_callback_t typedef
include/muc_opcua/server.h                      # Add mu_server_set/add_audit_callback declarations
src/services/audit_events.c                     # Replace no-op with callback dispatch
src/core/server_internal.h                       # Add callback storage to mu_server
src/CMakeLists.txt                               # Add MUC_OPCUA_AUDITING gate
tests/unit/test_audit_events.c                   # Add callback dispatch tests

# D3 — Aggregate Functions (5 files)
include/muc_opcua/opcua_ids.h                   # Add 39 aggregate type ID constants
src/services/subscription.h                      # Expand accumulator union
src/services/subscription_aggregate.c            # Add accumulate/publish for new types
src/core/service_dispatch/filter_reader.c        # Expand aggregate type whitelist
tests/unit/test_aggregate_full.c                 # Add per-function edge-case tests
tests/unit/test_aggregate.c                      # Add integration tests for new types

# D4 — Binary Size Measurement (4 files)
scripts/measure_size.sh                          # Add ELF linking, JSON output, LTO comparison
scripts/size_measure.ld                          # NEW: minimal Cortex-M0+ linker script
scripts/size_measure_startup.c                   # NEW: minimal startup code
docs/size/                                      # Update size ledger with linked-ELF data

# CMake integration fix (1 file)
src/CMakeLists.txt                               # Fix missing target_compile_definitions
```

**Structure Decision**: All work is in-place modification of existing files (D1-D3) or
additions to existing scripts/tools (D4). Two new files for D4 linker infrastructure.
No new source directories.

## Complexity Tracking

No constitution violations. All features are:
- Gated on existing feature flags with zero impact when OFF
- Following established codebase patterns (encoder/decoder signatures, callback
  patterns, accumulate/publish patterns, script architecture)
- Stack/BSS allocated (no heap)
