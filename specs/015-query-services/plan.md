# Implementation Plan: Query Services

**Branch**: `015-query-services` | **Date**: 2026-06-29 | **Spec**: `specs/015-query-services/spec.md`
**Input**: Feature specification from `specs/015-query-services/spec.md`

## Summary

Implement the Query Service Set (`QueryFirst` and `QueryNext`) to allow clients to query the address space for nodes matching a `NodeTypeDescription` and `ContentFilter`. The implementation will restrict `ContentFilter` operators to a basic subset to satisfy Embedded Profile bounds, supporting single-node querying without recursive browsing.

## Technical Context

**Language/Version**: C11
**Primary Dependencies**: None (freestanding)
**Storage**: Static memory only (No heap)
**Testing**: Unity for unit tests, CTest, Fuzz testing
**Target Platform**: ARM Cortex-M, POSIX, Windows
**Project Type**: C Library
**Performance Goals**: Minimal flash and RAM overhead; O(n) filter scan for dynamic/static address spaces.
**Constraints**: `MaxQueryResults` enforced, fixed maximum of continuation points (e.g., 2), no complex filter logic (like spatial).
**Scale/Scope**: Embedded servers with <1000 nodes, 1-2 concurrent query requests.
**OPC UA Normative References**: OPC 10000-4 v1.05 (Section 5.9 Query Service Set, 5.9.3 QueryFirst, 5.9.4 QueryNext). OPC 10000-4 v1.05 (7.7 ContentFilter).
**Target OPC UA Profile/Conformance Units**: Standard Server Profile (Query Services are generally part of standard/advanced profiles, but we provide a minimal subset). Core Server Facet.
**Conformance Status Target**: experimental/profile-targeting

## Embedded Size Budget

**Flash Impact**: ~1.5 - 2 KB additional thumb code for `QueryFirst`, `QueryNext`, and `ContentFilter` evaluation.
**RAM Impact**: Configurable via `MU_MAX_QUERY_CONTINUATION_POINTS` (default: 1 or 2). Each point requires tracking the current index in the static and dynamic address space (~8 bytes per point). Max query results buffer is caller-provided or chunked.
**Heap Use**: None. Continuation points will be stored in a static array in the server context.
**Static Tables Added**: N/A.
**Transport Buffers**: Reuses the standard message chunks (`MU_MIN_CHUNK_SIZE`).
**Crypto Dependency Impact**: N/A.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: Implementation will use OPC UA Binary encoding for `QueryFirstRequest`/`Response` and `QueryNextRequest`/`Response`.
- **Embedded C Core**: Uses freestanding C11. No OS dependencies.
- **Memory Model**: No heap allocation. `ContinuationPoint` logic uses a fixed-size ring buffer or static array allocated inside `mu_server_storage`.
- **Minimal OPC UA Surface**: Scoped strictly to OPC UA Binary over TCP.
- **Profile Research**: Query Services aren't strictly mandatory for the Micro Profile, but they provide value for NodeManagement and complex static spaces. We will implement basic filtering.
- **Correctness Gates**: Unit tests for request decoding, filter evaluation, and response encoding.
- **Security Honesty**: N/A.
- **Toolchain Discipline**: CMake, Clang-Format, CPPCheck integration.
- **Size Discipline**: Tracked via `measure_size.sh`.

## Project Structure

### Documentation (this feature)

```text
specs/015-query-services/
├── plan.md              # This file
├── spec.md              # Feature specification
└── tasks.md             # To be generated
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    └── services/
        └── query.h      # Query interface
src/
├── core/
│   └── service_dispatch.c # Dispatch integration
├── encoding/
│   └── binary_query.c   # Decoders/Encoders for Query services and ContentFilter
└── services/
    └── query.c          # QueryFirst and QueryNext business logic
tests/
└── unit/
    └── test_query.c     # Unit tests for query filter and continuation
```

**Structure Decision**: Added `query.h` and `query.c` following the existing `browse.c`/`node_management.c` pattern. `binary_query.c` to house the complex `ContentFilter` encoding/decoding.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Basic ContentFilter Support | Required for QueryFirst | Without at least some ContentFilter operators (e.g. Equals, Like), QueryFirst is useless. |
