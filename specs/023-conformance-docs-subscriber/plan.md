# Implementation Plan: Conformance Docs and PubSub Subscriber

**Branch**: `023-conformance-docs-subscriber` | **Date**: 2026-06-30 | **Spec**: [specs/023-conformance-docs-subscriber/spec.md](spec.md)
**Input**: Feature specification from `specs/023-conformance-docs-subscriber/spec.md`

## Summary

This feature closes the next conformance/evidence and usability gaps before adding more standard surface: update user and implementor documentation, strengthen in-repo stale-claim/stale-number checks, add a caller-storage decoder for the scoped UADP/UDP subscriber subset that matches the existing publisher, and add selected subscription/DataChange/status negative-path evidence. The technical approach is test-first for all protocol-visible behavior, with exact OPC UA citations and resource-risk markers on code-affecting tasks.

## Technical Context

**Language/Version**: C11 with C99-compatible style where practical
**Primary Dependencies**: None in the core library; existing Unity/CTest and shell/Python validation helpers for tests
**Storage**: Caller-provided server storage and caller-provided PubSub decode output arrays; no heap and no mandatory new server state
**Testing**: Unity unit tests, CTest, existing documentation/conformance tests, targeted size-report and traceability checks
**Target Platform**: Freestanding embedded C library, host tests, RP2040/Pico-style Arm Cortex-M0+ size evidence
**Project Type**: Embedded library
**Performance Goals**: Documentation/check work adds zero runtime cost; UADP subscriber decode is linear in input bytes and field count, avoids heap, and targets less than 2 KiB added Arm Cortex-M0+ `.text`
**Constraints**: No mandatory heap allocation, no hidden dynamic PubSub metadata, no new blocking receive loop, no broad PubSub configuration model, no CTT/profile-compliance claim
**Scale/Scope**: Existing docs, conformance checks, one scoped UADP NetworkMessage decoder, and targeted subscription/DataChange/status negative paths
**OPC UA Normative References**: OPC-10000-7 §4.2 and §4.3; OPC-10000-14 §5.4.2.1, §5.4.2.2, §5.4.6.2.2, §7.2.4.1, §7.2.4.2, §7.2.4.4.2, §7.2.4.5.2, §7.2.4.5.3, §7.2.4.5.4, §7.2.4.5.5, §7.3.2.1; OPC-10000-6 §5.2.2.16; OPC-10000-4 §5.8, §5.13, §5.14, §7.17.2, §7.22.1, §7.22.2, §7.22.4, §7.38.2, Appendix B §B.2.3/§B.2.4; OPC-10000-13 §4.2.2.4/§5.4.3.5, §4.2.2.9/§5.4.3.10, §4.2.2.10/§5.4.3.11
**Target OPC UA Profile/Conformance Units**: Existing profile-targeting embedded server subset; scoped experimental PubSub UADP/UDP subset; no new profile-compliance claim
**Conformance Status Target**: Profile-targeting for server documentation; experimental/scoped for PubSub subscriber

## Embedded Size Budget

**Flash Impact**: Documentation/check work: 0 runtime bytes. UADP subscriber decoder target: <2 KiB added Arm Cortex-M0+ `.text`; any excess must be recorded with rationale.
**RAM Impact**: No mandatory static RAM; PubSub decode uses caller-provided output slots and local stack only. Target added peak stack <256 B.
**Heap Use**: None.
**Static Tables Added**: None planned. Any added table must be bounded, cited, and measured.
**Transport Buffers**: Reuses caller UDP payload buffers; no receive buffer is added by this feature. OPC-10000-14 §7.3.2.1 UDP transport is treated as payload input to the decoder, not a new transport loop.
**Crypto Dependency Impact**: None; PubSub security is out of scope.

## Constitution Check

*GATE: Passed before Phase 0 research; passed again after Phase 1 design.*

- **Spec Fidelity**: PubSub, Variant, StatusCode, Subscription/MonitoredItem, Query, NodeManagement, aggregate, and profile-claim work cites exact OPC UA sections in the spec, plan, contracts, and tasks.
- **Embedded C Core**: Plan stays within freestanding C11, caller-owned storage, and existing platform adapters.
- **Memory Model**: UADP subscriber decode is caller-storage only; no heap and no mandatory server-state growth.
- **Minimal OPC UA Surface**: Scope is the publisher-compatible UADP/UDP decoding subset, documentation/evidence checks, and selected negative paths. Broad PubSub configuration, security, discovery metadata, JSON/broker mappings, and profile compliance are out of scope.
- **Profile Research**: OPC-10000-7 §4.2/§4.3 are used only for conformance-claim discipline; no PubSub profile is claimed.
- **Correctness Gates**: UADP decode success, malformed input, unsupported layout, output-capacity, and subscription/status negative paths are test-first tasks.
- **Security Honesty**: No new security policy or PubSub security claim is added; CTT and profile-compliance language remains withheld without external evidence.
- **Toolchain Discipline**: Existing CMake, Unity/CTest, doc/conformance tests, traceability checks, and size measurements remain the gates.
- **Size Discipline**: All code-affecting tasks are resource-risk marked in tasks.md, and size impact must be recorded before completion.

## Project Structure

### Documentation (this feature)

```text
specs/023-conformance-docs-subscriber/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── documentation-evidence.md
│   └── pubsub-subscriber.md
└── tasks.md
```

### Source Code (repository root)

```text
include/micro_opcua/
└── pubsub.h                    # Public scoped subscriber decode types/API
src/
├── encoding/
│   └── uadp_encoder.c          # Existing UADP publisher codec plus scoped decoder
└── core/
    └── pubsub.c                # Existing publisher poll path; no receive loop planned
tests/
├── unit/
│   ├── test_uadp_encoding.c    # UADP decode success and negative tests
│   ├── test_conformance_docs.c # Stale-claim/stale-number doc checks
│   └── test_traceability_docs.c
└── fixtures/
    └── uadp_network_message.bin
docs/
├── conformance/
├── integration-guide.md
├── api-reference.md
├── size/
└── traceability/
```

**Structure Decision**: Keep the scoped UADP decoder adjacent to the existing encoder so the publisher and subscriber share constants and binary Variant helpers. Do not add a PubSub receive loop, dynamic metadata model, or transport adapter receive API in this feature.

## Complexity Tracking

No constitution check violations.
