# Implementation Plan: OPC UA PubSub (UADP/UDP)

**Branch**: `012-opcua-pubsub` | **Date**: 2026-06-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/012-opcua-pubsub/spec.md`

## Summary

Implement OPC UA PubSub (Part 14) UADP over UDP connectionless publishing to allow low-footprint sensors to broadcast data without TCP overhead.

## Technical Context

**Language/Version**: C11 (freestanding)
**Primary Dependencies**: None (internal encoding/UDP adapter)
**Storage**: N/A
**Testing**: Unity, Wireshark/UaExpert for network validation
**Target Platform**: Embedded microcontrollers (RP2040, Arduino) and Host
**Project Type**: C Library
**Performance Goals**: Sub-millisecond packet construction
**Constraints**: Zero-heap, minimal RAM/Flash impact
**Scale/Scope**: Single Publisher, UDP Unicast/Multicast
**OPC UA Normative References**: OPC-10000-14 section 7.2 (UADP), section 7.3 (UDP)
**Target OPC UA Profile/Conformance Units**: PubSub UADP UDP Publisher
**Conformance Status Target**: Profile-targeting

## Embedded Size Budget

**Flash Impact**: ~1.5 - 2.5 KB (UADP encoder)
**RAM Impact**: < 200 bytes (Writer Group and DataSetWriter structs)
**Heap Use**: None
**Static Tables Added**: N/A
**Transport Buffers**: Reuses the `send_buffer` from `mu_server_config_t` for packet construction.
**Crypto Dependency Impact**: None (PubSub security out of scope for MVP).

## Constitution Check

*GATE: Passed*

- **Spec Fidelity**: Strictly implementing UADP NetworkMessage framing per OPC 10000-14.
- **Embedded C Core**: Freestanding C11, no heap.
- **Memory Model**: Static `mu_pubsub_connection_t` array, caller-provided UDP transmission buffer.
- **Minimal OPC UA Surface**: Scoped entirely to UADP Publisher over UDP; Subscriber and MQTT excluded.
- **Profile Research**: Conforms to Publisher requirements in Part 14.
- **Correctness Gates**: Unity tests for UADP binary encoding will be written.
- **Toolchain Discipline**: Fits existing CMake structure.
- **Size Discipline**: Will be gated by `MICRO_OPCUA_PUBSUB` macro to ensure `nano` stays small when disabled.

## Project Structure

### Documentation (this feature)

```text
specs/012-opcua-pubsub/
├── plan.md
├── research.md 
├── data-model.md
├── quickstart.md
└── tasks.md 
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    └── pubsub.h             # Public PubSub API
src/
├── core/
│   └── pubsub.c             # PubSub timing and state machine
├── encoding/
│   └── uadp_encoder.c       # UADP binary encoder
platform/
└── host/
    └── udp_adapter.c        # Host UDP broadcast implementation
tests/
└── unit/
    └── test_pubsub.c        # Unity tests for UADP encoding
```

**Structure Decision**: Added `pubsub.h`/`pubsub.c` behind the `MICRO_OPCUA_PUBSUB` toggle to isolate Part 14 features from the core TCP server logic.

## Complexity Tracking

*(No violations to justify. The feature perfectly aligns with zero-heap and minimal constraints).*
