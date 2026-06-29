# Implementation Plan: OPC UA Conformance and Size Repairs

**Branch**: `019-fix-conformance-size` | **Date**: 2026-06-29 | **Spec**: [specs/019-fix-conformance-size/spec.md](spec.md)
**Input**: Feature specification from `specs/019-fix-conformance-size/spec.md`

## Summary

Repair wire-visible OPC UA fidelity issues found in the repository audit before adding more feature surface: correct StatusCode and NodeId constants, validate scoped AggregateFilter support with standard identifiers, implement GetEndpoints transport-profile filtering, harden the stack-budget gate, and document measured size/performance effects. The technical approach is test-first for protocol behavior, with official OPC Foundation CSV values and OPC UA reference sections cited in tests, docs, and tasks.

## Technical Context

**Language/Version**: C11 with freestanding-compatible core style  
**Primary Dependencies**: Existing CMake build, Unity tests, OPC UA reference documentation, official OPC Foundation UA NodeSet CSV tables  
**Storage**: Static server, subscription, connection, endpoint, and test fixture storage; no heap introduced  
**Testing**: Unity unit tests, CTest integration tests, `scripts/measure_size.sh all`, stack-usage gate with `MICRO_OPCUA_STACK_USAGE=ON`  
**Target Platform**: Embedded microcontrollers represented by ARM Cortex-M0+ archive sizing, plus Linux host tests  
**Project Type**: Embedded C OPC UA server library  
**Performance Goals**: No protocol hot-path heap; no nano/embedded flash regression above 2 percent; remove at least one avoidable soft-float path; stack-budget gate passes for emitted frames within 10 KiB threshold  
**Constraints**: Preserve OPC UA StatusCode correctness, no silent conformance downgrades, no dynamic allocation, public API constants remain source-compatible names with corrected values  
**Scale/Scope**: Existing nano, micro, embedded, and full profiles; audited baseline archive text sizes are nano 16,653 bytes, micro 23,962 bytes, embedded 47,126 bytes, full 47,400 bytes  
**OPC UA Normative References**: OPC-10000-4 §5.5.4.2 GetEndpoints parameters; OPC-10000-4 §5.13.2.4 and §5.13.3.4 monitored-item StatusCodes; OPC-10000-4 §7.22.1 MonitoringFilter overview; OPC-10000-4 §7.22.4 AggregateFilter; OPC-10000-4 §7.38.2 Common StatusCodes; OPC-10000-6 §5.2.2.9 NodeId; OPC-10000-6 §5.2.2.15 ExtensionObject; OPC-10000-13 §4.2.2.4 Average Aggregate Object; OPC-10000-13 §4.2.2.9 Minimum Aggregate Object; OPC-10000-13 §4.2.2.10 Maximum Aggregate Object; OPC-10000-13 §5.4.2.3 aggregate data types; OPC-10000-13 §5.4.3.5 Average; OPC-10000-13 §5.4.3.10 Minimum; OPC-10000-13 §5.4.3.11 Maximum; OPC-10000-7 §4.3 Profiles  
**Target OPC UA Profile/Conformance Units**: Profile-targeting embedded server over OPC UA TCP Binary; scoped aggregate support references the OPC-10000-13 aggregate objects for Average, Minimum, and Maximum but does not claim full aggregate conformance or CTT verification  
**Conformance Status Target**: Profile-targeting

## Embedded Size Budget

**Flash Impact**: Target net change within +2 percent for nano and embedded profiles; expected small increase for additional tests/docs only, with implementation aiming to offset protocol fixes by removing avoidable soft-float conversion in session timeout handling  
**RAM Impact**: Static RAM impact measured in default profiles; audited subscription/connection hotspots require documented controls, reductions, or standards-constrained non-change rationale
**Heap Use**: None  
**Static Tables Added**: No runtime static tables; tests may contain expected constant tables compiled only into test binaries  
**Transport Buffers**: Existing request/response buffers only; GetEndpoints filtering must not add endpoint buffers beyond the existing bounded `MU_DISCOVERY_MAX_ENDPOINTS` array  
**Crypto Dependency Impact**: None

## Constitution Check

*GATE: Passed*

- **Spec Fidelity**: Every changed wire-visible StatusCode, NodeId, monitored-item filter behavior, ExtensionObject encoding rule, and GetEndpoints filter cites exact OPC UA sections plus official CSV rows for numeric values.
- **Embedded C Core**: The implementation remains C11 and platform-independent, with no OS dependency added to protocol core files.
- **Memory Model**: No heap allocation is introduced; all new tests and filters use existing bounded arrays or caller-provided buffers.
- **Minimal OPC UA Surface**: Scope stays within existing OPC UA TCP Binary server behavior; the feature repairs selected services instead of adding new services.
- **Profile Research**: Claims remain profile-targeting per OPC-10000-7 §4.3; no profile-compliant or CTT-verified claim is made.
- **Correctness Gates**: StatusCodes, NodeIds, aggregate filter decode/validation, GetEndpoints filtering, stack budget, and size deltas have test or measurement tasks before implementation completion.
- **Security Honesty**: No SecurityPolicy behavior is weakened; docs must qualify conformance and security claims where evidence is absent.
- **Toolchain Discipline**: CMake, Unity, CTest, stack-usage, and ARM size measurement commands are part of the task plan.
- **Size Discipline**: Baseline flash, server RAM, monitored-item RAM, connection RX buffers, and stack-frame findings are captured in research and must be remeasured after implementation.

## Project Structure

### Documentation (this feature)

```text
specs/019-fix-conformance-size/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── protocol-repair-contract.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    ├── opcua_ids.h        # Standard NodeId constants
    └── status.h           # Standard StatusCode constants
src/
├── core/
│   ├── service_dispatch.c # AggregateFilter and GetEndpoints service behavior
│   └── status.c           # Optional status-name diagnostics
├── services/
│   ├── discovery.c        # Endpoint descriptions and transport-profile URI values
│   ├── session.c          # Integer-only timeout conversion hotspot
│   └── subscription.*     # Aggregate state and publish hot path
scripts/
└── check_stack_usage.sh   # Stack-budget gate
tests/
├── unit/
│   ├── test_status.c
│   ├── test_aggregate.c
│   ├── test_discovery_endpoint.c
│   └── test_size_report.c
└── integration/
    └── test_server_handshake*.c
docs/
├── conformance/
└── traceability/
```

**Structure Decision**: Keep repairs in existing headers, service dispatch, service helpers, tests, scripts, and docs. Avoid broad service-dispatch splitting in this feature; record it as follow-up unless a small measured change is needed to meet the budget.

## Complexity Tracking

No constitution check violations.
