# Implementation Plan: Optimize Hot Paths

**Branch**: `022-optimize-hot-paths` | **Date**: 2026-06-30 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/022-optimize-hot-paths/spec.md`

## Summary

This feature improves already-implemented OPC UA hot paths using controlled,
repeatable benchmark evidence while preserving wire-visible behavior, StatusCodes,
compatibility claims, zero-heap hot paths, and embedded resource headroom. Work is
limited to the existing OPC UA Binary, MessageChunk framing, Read, Write, and
response-framing paths; no new service surface or conformance claim is introduced.

## Technical Context

**Language/Version**: C11, C99-compatible style where practical
**Primary Dependencies**: None for runtime core; existing Unity tests, CMake, and
host benchmark tooling for verification
**Storage**: In-memory benchmark/result artifacts only; no runtime persistent
storage changes
**Testing**: Unity unit tests, CTest integration tests, existing fuzz harnesses,
interop smoke, controlled speed/resource benchmark matrix
**Target Platform**: Embedded microcontrollers through the portable library,
verified first on Linux host and current profile builds
**Project Type**: Embedded C OPC UA server library
**Performance Goals**: Improve at least one targeted hot-path workload by >=5%
normalized throughput; do not regress targeted hot-path workloads by >5% unless
explicitly accepted as a size-first optimization
**Constraints**: Preserve OPC UA wire-visible behavior; no new heap use; ROM growth
<=3%; static RAM growth <=5%; no undocumented stack growth; controlled benchmark
must run on isolated CPU 11 with realtime priority 80 and recorded host fingerprint
**Scale/Scope**: Existing `nano`, `micro`, `embedded`, and `full` profile benchmark
matrix; 42 current workload rows; batch limit remains 32 items
**OPC UA Normative References**: OPC-10000-6 section 5.2.1 Binary DataEncoding
general rules; OPC-10000-6 section 5.2.2.4 String; OPC-10000-6 section 5.2.2.7
ByteString; OPC-10000-6 section 5.2.2.9 NodeId; OPC-10000-6 section 5.2.5
Arrays; OPC-10000-6 section 6.7.2 MessageChunk structure; OPC-10000-6 section
6.7.2.2 Message Header; OPC-10000-6 section 6.7.2.4 Sequence Header;
OPC-10000-6 section 6.7.3 MessageChunks and error handling; OPC-10000-6 section
6.7.4 Establishing a SecureChannel; OPC-10000-6 section 7.1.2.2 OPC UA TCP Message Header;
OPC-10000-6 section 7.2 OPC UA TCP; OPC-10000-4 section 5.11.2 Read Service;
OPC-10000-4 section 5.11.4 Write Service; OPC-10000-4 section 6.5.8 Auditing for
Attribute Service Set; OPC-10000-4 section 7.38.2 Common StatusCodes;
OPC-10000-7 section 4.2 Conformance Units and Conformance Groups
**Target OPC UA Profile/Conformance Units**: No new profile or conformance unit;
preserve current documented server profile/conformance claims
**Conformance Status Target**: Preserve existing status; optimization evidence is
performance/resource evidence only, not CTT verification

## Embedded Size Budget

**Flash Impact**: Net target <=0 bytes; hard gate <=3% ROM growth per profile from
the controlled baseline (`nano` 34,286 B; `micro` 47,115 B; `embedded` 80,694 B;
`full` 96,398 B)
**RAM Impact**: Net target 0 bytes; hard gate <=5% static RAM growth per profile
from baseline (`nano` 264 B; `micro` 528 B; `embedded` 5,984 B; `full` 6,224 B);
no new mandatory buffers
**Heap Use**: None; optimized hot paths must remain heap-free
**Static Tables Added**: None planned; any proposed lookup table must prove net
ROM/RAM benefit or be rejected
**Transport Buffers**: Reuse existing caller/server-provided buffers and negotiated
limits; no buffer-size expansion. OPC UA basis: OPC-10000-6 sections 6.7.2,
7.1.2.2, and 7.2
**Crypto Dependency Impact**: None; no security policy or crypto backend changes

## Constitution Check

*GATE: Passed before Phase 0 research. Re-checked after Phase 1 design: Passed.*

- **Spec Fidelity**: Passed. Optimization scope is tied to OPC-10000-6 sections
  5.2.1, 5.2.2.4, 5.2.2.7, 5.2.2.9, 5.2.5, 6.7.2, 6.7.2.2, 6.7.2.4, 6.7.3,
  6.7.4, 7.1.2.2, 7.2; OPC-10000-4 sections 5.11.2, 5.11.4, 6.5.8, 7.38.2;
  and OPC-10000-7 section 4.2.
- **Embedded C Core**: Passed. Work remains in the freestanding portable C core and
  existing adapter boundary.
- **Memory Model**: Passed. No heap, no new mandatory transport buffers, no planned
  static tables.
- **Minimal OPC UA Surface**: Passed. No new services, transports, encodings,
  security policies, or conformance claims.
- **Profile Research**: Passed. This feature preserves current profile claims; it
  does not select or claim a new profile.
- **Correctness Gates**: Passed. Each behavior-sensitive optimization must have
  tests before implementation and run affected unit/integration/fuzz/interop
  checks plus the speed/resource compare.
- **Security Honesty**: Passed. No SecurityPolicy or crypto dependency changes.
- **Toolchain Discipline**: Passed. Uses existing CMake/CTest/Unity/fuzz/interop and
  benchmark tooling.
- **Size Discipline**: Passed. Baseline sizes and gates are documented; tasks must
  treat ROM/RAM/throughput-risk changes explicitly.

## Project Structure

### Documentation (this feature)

```text
specs/022-optimize-hot-paths/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── benchmark-report.md
└── tasks.md              # Created by /speckit-tasks
```

### Source Code (repository root)

```text
include/
└── micro_opcua/          # Public API; no planned public API expansion
src/
├── core/
│   ├── message_chunk.c   # MessageChunk/header parse and write path
│   ├── service_dispatch.c
│   ├── tcp_connection.c
│   └── uasc.c            # Response framing path
├── encoding/
│   ├── binary_reader.c
│   ├── binary_writer.c
│   └── binary_le.h       # Primitive OPC UA Binary hot paths
└── services/
    ├── read.c            # Read service behavior
    └── write.c           # Write service behavior
tests/
├── unit/                 # Binary, chunk, Read, Write, size tests
├── fuzz/                 # Decoder and MessageChunk fuzz harnesses
├── integration/          # Server framing, response, interop-adjacent tests
├── benchmark/            # Hot-path benchmark executable
└── tools/                # Benchmark runner tests
docs/
├── benchmarks/           # Controlled baseline and benchmark documentation
└── traceability/         # Optimization evidence and OPC UA section mappings
```

**Structure Decision**: Keep optimization changes in existing protocol modules and
test directories. Do not introduce new runtime subsystems, public APIs, persistent
state, or profile-specific source trees.

## Complexity Tracking

No constitution violations.
