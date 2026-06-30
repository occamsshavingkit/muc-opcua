# Implementation Plan: OPC UA Fidelity Docs and Next Work

**Branch**: `021-opcua-fidelity-docs` | **Date**: 2026-06-30 | **Spec**: [specs/021-opcua-fidelity-docs/spec.md](spec.md)
**Input**: Feature specification from `specs/021-opcua-fidelity-docs/spec.md`

## Summary

This feature starts the next OPC UA fidelity pass in five independently testable slices: FreeRTOS + lwIP getting-started documentation, conformance citation cleanup, scoped aggregate-filter follow-up, Subscription/MonitoredItem negative-path hardening, and profile-claim cleanup. The first implementation slice is documentation-only and closes GitHub issue #222 by adding concrete FreeRTOS + lwIP guidance to `docs/getting-started.md`; later protocol tasks are test-first and section-cited.

## Technical Context

**Language/Version**: C11 for protocol code; Markdown for this first documentation slice
**Primary Dependencies**: Existing CMake build, Unity tests, CTest, documentation/conformance tests, GitHub issue #222, OPC UA reference documentation
**Storage**: No new runtime storage for documentation/citation slice; future protocol slices use existing static server/subscription storage
**Testing**: Documentation/conformance tests for docs; Unity/CTest RED tests before aggregate or subscription behavior changes
**Target Platform**: Linux host validation plus embedded targets represented by FreeRTOS + lwIP guidance, Pico SDK, Arduino/PlatformIO, and ARM size builds
**Project Type**: Embedded C OPC UA server library
**Performance Goals**: Documentation/citation slice has zero runtime cost; future protocol hardening must keep malformed-input paths bounded and avoid throughput regressions in valid Publish paths. The async-opcua localhost perf pass from `/home/quackdcs/scratch/opcua-localhost-bench/perf-20260630-b92d983f-4e74d40-async-*` is comparative evidence only: prioritize local measurement around OPC-10000-6 chunk encode/decode, Read/Write request decode, send-buffer writes, and request fanout/audit analogs; do not prioritize `trace_locks` unless fresh local evidence makes it material.
**Constraints**: No new heap use in protocol hot path; no new profile-compliant or CTT-verified claim without external evidence; every behavior-changing task cites the exact OPC UA section; resource-risk tasks are marked before implementation
**Scale/Scope**: Existing nano, micro, embedded, and full profiles; FreeRTOS + lwIP guidance targets out-of-tree external platform integration, not a new in-tree firmware example
**OPC UA Normative References**: OPC-10000-4 §5.8 NodeManagement Service Set; OPC-10000-4 §5.12.2 Method Service Set Call; OPC-10000-4 §5.13 MonitoredItem Service Set; OPC-10000-4 §5.14 Subscription Service Set; OPC-10000-4 §7.22.4 AggregateFilter; OPC-10000-4 §7.38.2 StatusCodes; OPC-10000-6 §5.2 Binary encoding; OPC-10000-6 §6.7.2 MessageChunk structure; OPC-10000-6 §7.1.2.2 Message Header; OPC-10000-6 §7.1.2.3 Hello Message; OPC-10000-6 §7.1.2.4 Acknowledge Message; OPC-10000-6 §7.2 OPC UA TCP; OPC-10000-7 §4.2 ConformanceUnits and §4.3 Profiles; OPC-10000-13 §4.2.2.4 / §5.4.3.5 Average; OPC-10000-13 §4.2.2.9 / §5.4.3.10 Minimum; OPC-10000-13 §4.2.2.10 / §5.4.3.11 Maximum
**Target OPC UA Profile/Conformance Units**: Existing profile-targeting embedded server subset; selected aggregate and subscription conformance-unit evidence only
**Conformance Status Target**: Profile-targeting only; not profile-compliant or CTT-verified without external OPC Foundation CTT evidence

## Embedded Size Budget

**Flash Impact**: Documentation/citation slice: 0 bytes runtime. Future aggregate/subscription code slices target less than +2 KiB ARM text each unless justified by a cited protocol requirement.
**RAM Impact**: Documentation/citation slice: 0 bytes runtime. Future protocol slices must not add default mutable storage unless behind an owning feature macro and must measure caller-storage deltas.
**Heap Use**: None. FreeRTOS/lwIP docs must state that platform heaps are outside the core library's no-heap claim.
**Static Tables Added**: Documentation/citation slice: none. Future tests may add test-only tables; runtime tables require size review.
**Transport Buffers**: FreeRTOS + lwIP docs keep the existing caller-owned `MU_MIN_CHUNK_SIZE` receive/send buffer guidance tied to OPC-10000-6 Hello/Acknowledge and OPC UA TCP.
**Crypto Dependency Impact**: Documentation/citation slice: none. Guidance may mention optional crypto adapters but does not add backend dependencies.

## Constitution Check

*GATE: Passed*

- **Spec Fidelity**: Plan cites exact OPC UA sections for documentation, Method Call, NodeManagement, AggregateFilter, Subscription/MonitoredItem services, transport, StatusCodes, and profile claims.
- **Embedded C Core**: Documentation keeps FreeRTOS/lwIP outside the core and uses adapter boundaries; future code tasks remain in the existing C core.
- **Memory Model**: Documentation reinforces caller-owned storage and platform-owned FreeRTOS/lwIP heaps; no protocol heap is added.
- **Minimal OPC UA Surface**: Scope does not add transports, encodings, full aggregate coverage, or TransferSubscriptions support.
- **Profile Research**: Profile status remains profile-targeting under OPC-10000-7 §4.2/§4.3 until external CTT evidence exists.
- **Correctness Gates**: Future aggregate/subscription behavior tasks require RED tests before production code; documentation-only tasks use doc/conformance checks.
- **Security Honesty**: SecurityPolicy None remains scoped to current profiles and non-production/plaintext identity warnings; no security claim is broadened.
- **Toolchain Discipline**: Uses existing CMake, CTest, documentation tests, size scripts, and embedded cross-build commands.
- **Size Discipline**: Documentation slice is runtime-size neutral; future resource-risk tasks must measure flash/RAM/throughput impact and preserve the static-memory model even when comparative perf data points at allocation-heavy request fanout in another stack.

## Project Structure

### Documentation (this feature)

```text
specs/021-opcua-fidelity-docs/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── conformance-citations.md
│   └── freertos-lwip-getting-started.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
docs/
├── getting-started.md          # FreeRTOS + lwIP first-slice documentation
├── integration-guide.md        # Existing deeper adapter guidance, linked from getting started
└── conformance/
    ├── services.md             # Citation cleanup and service matrix
    ├── profile-embedded.md     # Profile-targeting claim cleanup as needed
    ├── profile-micro.md
    └── profile-nano.md
tests/
└── unit/
    └── test_conformance_docs.c # Existing documentation/conformance guardrails
src/
└── services/
    └── subscription.c          # Future aggregate/subscription behavior slices
cmake/
└── MicroOpcUaOptions.cmake     # External platform selection
CMakeLists.txt                  # Public MICRO_OPCUA_PLATFORM cache string
```

**Structure Decision**: Keep the first implemented slice in existing documentation files. Do not create a new platform page until the getting-started path proves too large; keep deeper adapter detail in `docs/integration-guide.md`. Future protocol hardening stays in existing subscription tests and service implementation files with section-cited tasks.

## Complexity Tracking

No constitution check violations.
