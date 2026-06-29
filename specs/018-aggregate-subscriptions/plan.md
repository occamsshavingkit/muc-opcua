# Implementation Plan: Aggregate Subscriptions

**Branch**: `018-aggregate-subscriptions` | **Date**: 2026-06-29 | **Spec**: [specs/018-aggregate-subscriptions/spec.md](spec.md)
**Input**: Feature specification from `specs/018-aggregate-subscriptions/spec.md`

## Summary

This feature adds support for `AggregateFilter` inside MonitoredItems in Subscriptions. Clients can subscribe to `Average`, `Minimum`, and `Maximum` computed values over a specified `processingInterval`. In accordance with our zero-heap architecture, this is implemented using a running calculation model stored in-place in each `mu_monitored_item_t` slot.

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (Freestanding C)  
**Storage**: In-memory subscription state  
**Testing**: Unity Unit Tests, CTest Integration tests  
**Target Platform**: Embedded Microcontrollers (Pico/Arduino) & Linux  
**Project Type**: Embedded Library  
**Performance Goals**: Publishing cycle latency < 1ms, zero overhead on non-aggregate items  
**Constraints**: strictly no heap allocations, static configuration limits  
**OPC UA Normative References**: OPC-10000-4 Section 7.16 (AggregateFilter), OPC-10000-13 (Aggregates)  
**Target OPC UA Profile/Conformance Units**: Monitored Item Aggregate Filter Facet  
**Conformance Status Target**: Profile-compliant  

## Embedded Size Budget

**Flash Impact**: ~1200 bytes  
**RAM Impact**: 24 bytes per monitored item slot  
**Heap Use**: None (statically allocated monitored item structures)  
**Static Tables Added**: None  
**Transport Buffers**: Reuses existing TCP publish buffers  
**Crypto Dependency Impact**: None  

## Constitution Check

*GATE: Passed*

- **Spec Fidelity**: Evaluates filter parameters according to OPC-10000-4. Returns `Bad_MonitoredItemFilterUnsupported` for unsupported types.
- **Embedded C Core**: Standard C11, no heap, platform-independent.
- **Memory Model**: No heap allocation on hot path. Bounded state arrays.
- **Minimal OPC UA Surface**: Restricts aggregates to Average, Min, and Max.
- **Profile Research**: Facet fits within small micro-embedded profiles.
- **Correctness Gates**: Unity unit tests verify decoding, state updates, aggregate computation, and output formatting.
- **Security Honesty**: No impact on Secure Channel structure.
- **Toolchain Discipline**: Standard CMake build and test framework.
- **Size Discipline**: Checked via size report target.

## Project Structure

### Documentation (this feature)

```text
specs/018-aggregate-subscriptions/
├── plan.md              # This file
├── research.md          # Research findings
├── data-model.md        # Data model changes
├── quickstart.md        # Quickstart guide
└── tasks.md             # Task list
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    └── services/
        └── history.h    # Shared types
src/
├── core/
│   └── subscription.c   # Subscription logic
└── services/
    └── history.c        # Extracted codecs if needed
```

**Structure Decision**: Code modifications will be focused on subscription handling, extending `mu_monitored_item_t` state.

## Complexity Tracking

No constitution check violations.
