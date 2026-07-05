# Implementation Plan: Standard 2017 UA Server Profile

**Branch**: `037-standard-server-profile` | **Date**: 2026-07-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/037-standard-server-profile/spec.md`

## Summary

Implement the Standard 2017 UA Server Profile (OPC-10000-7 §6.6, profile ID 1663,
URI `http://opcfoundation.org/UA-Profile/Server/StandardUA2017`) by adding 11
facets currently missing or incomplete in the Embedded 2017 baseline: Data
Access Server Facet, Method Server Facet, Standard Event Subscription with
Address Space Notifier, Auditing, ComplexType Server Facet, Server Diagnostics,
Base Server Behavior, Durable Subscriptions, full aggregate functions, Reverse
Connect, and Security Time Synchronization. Each facet is gated behind its own
CMake flag and composable into a new `standard` profile target. The existing
nano/micro/embedded profiles are unchanged. Historical Access Server Facet
(OPC-10000-7 §6.6.49) is already fully implemented (src/services/history.c)
and requires no new work — it is included in the standard profile via the
existing `MUC_OPCUA_SERVICE_HISTORY` flag.

## Technical Context

**Language/Version**: C11 (freestanding, C99-compatible subset)
**Primary Dependencies**: CMake >= 3.16, Unity test framework, CMocka where needed
**Storage**: Static-only; no heap allocation on protocol hot path; static tables
**Testing**: Unity (unit), CMocka (integration/session), ASAN/UBSan, fuzz targets (libFuzzer)
**Target Platform**: Host (GCC/Clang) for tests; ARM Cortex-M0+ (RP2040/Pico SDK) for embedded; Arduino (PlatformIO) secondary
**Project Type**: Portable C library (embedded OPC UA server)
**Performance Goals**: Read/Browse/Write latency < 1ms at MCU speeds; Publish keep-alive within 2x publishing interval
**Constraints**: Zero `.data`/`.bss`/heap in steady state across all profiles; static tables only; no OS dependency
**Scale/Scope**: Standard profile capacity minima: >= 1000 MonitoredItems, >= 50 subscriptions, >= 50 sessions, >= 10 Publish requests per session
**OPC UA Normative References**: OPC-10000-2, 3, 4, 5, 6, 7, 11, 13 (see spec.md OPC-001 through OPC-016)
**Target OPC UA Profile/Conformance Units**: Standard 2017 UA Server Profile + Data Access Server Facet + Method Server Facet + Standard Event Subscription Server Facet + Address Space Event Notifier Server Facet + ComplexType Server Facet + Auditing Server Facet + Base Server Behavior Facet + Client Redundancy Facet + Aggregate Server Facet + Historical Access Server Facet + Reverse Connect Facet
**Conformance Status Target**: profile-targeting (CTT-verified requires external CTT run)

## Embedded Size Budget

**Flash Impact**: Estimated +15-20 KB `.text` on ARM Cortex-M0+ above embedded profile baseline (current full: 61,281 B). Standard target: < 80 KB. **Note: This is a pre-implementation estimate based on feature complexity analysis. Actual measurement in Phase 10 task T095 will validate.**
**RAM Impact**: Static data zero per constitution. Stack depth increases for event filter evaluation (~256 B), aggregate state (~128 B), and audit event building (~256 B). New static tables: diagnostics counters (~64 B), type definitions (~512 B), event notifier masks (~32 B per object node). Total static RAM: zero (all allocated at init from caller-provided storage).
**Heap Use**: None on protocol hot path. Caller-provided buffers for audit event payloads and complex type encoding.
**Static Tables Added**: AnalogItemType properties table (~200 B), diagnostics counter table (~64 B), aggregate function jump table (~256 B), DataType registry (~512 B), Method dispatch table extension (~128 B).
**Transport Buffers**: Unchanged. Reverse Connect uses same buffer pool as inbound TCP.
**Crypto Dependency Impact**: No new crypto. Auditing uses existing event delivery pipeline. Security Time Synchronization uses existing time adapter.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. All 26 FRs cite exact OPC-10000 sections. 16 OPC normative citations in spec. Unsupported aggregates return Bad_MonitoredItemFilterUnsupported; unsupported methods return Bad_MethodInvalid.
- **Embedded C Core**: PASS. C11 freestanding. All new code behind CMake gates; platform adapters for time, entropy, crypto unchanged. No OS dependency.
- **Memory Model**: PASS. Protocol hot path avoids heap. Event filter evaluation uses stack-local state. Audit event buffer caller-provided. Aggregate state static.
- **Minimal OPC UA Surface**: PASS. UA TCP + UA Binary only. No XML, JSON, HTTPS, WebSockets. Companion specs out of scope.
- **Profile Research**: PASS. Standard 2017 UA Server Profile (profile ID 1663) researched from OPC-10000-7 and companion spec requirements (OPC-30060, OPC-30000). Facet composition confirmed from OPC Foundation .NET reference stack.
- **Correctness Gates**: PASS. Test-first for event filter where-clause, percent deadband, audit event types, aggregate functions, TransferSubscriptions. Fuzz targets planned for event filter evaluation and structure encoding.
- **Security Honesty**: PASS. Auditing is new security-positive behavior. SecurityPolicy None still permitted for interop testing. All new features behind CMake gates; embedded profile maintains current security surface.
- **Toolchain Discipline**: PASS. CMake, Unity/CMocka, ASAN/UBSan, clang-format, cppcheck. Pico SDK cross-compile verified.
- **Size Discipline**: PASS. Flash/RAM estimated (see Size Budget). Static tables sized. Application headroom documented. Existing profiles must not regress >2%.

## Project Structure

### Documentation (this feature)

```text
specs/037-standard-server-profile/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── method-call.md
│   ├── event-filter.md
│   ├── auditing.md
│   └── complex-types.md
└── tasks.md             # Phase 2 output (/speckit-tasks)
```

### Source Code (repository root)

```text
include/muc_opcua/
├── services/
│   ├── method.h                   # NEW: Method callback registration API
│   ├── event_filter.h             # NEW: EventFilter where-clause evaluation API
│   ├── audit.h                    # NEW: Audit event type definitions
│   ├── diagnostics.h              # NEW: Server diagnostics counter API
│   ├── aggregation.h              # MODIFIED: Extended aggregate function set
│   └── history.h                  # EXISTING: extended for at-time reads
├── address_space/
│   ├── base_nodes.h               # MODIFIED: AnalogItemType, diagnostics nodes
│   └── complex_types.h            # NEW: StructureDefinition, EnumDefinition
├── types.h                        # MODIFIED: complex type enums, event field types
└── config.h                       # MODIFIED: standard profile capacity minima

src/
├── core/
│   ├── dispatch_method.c          # MODIFIED: arbitrary method dispatch
│   ├── dispatch_audit.c           # NEW: audit event generation
│   ├── dispatch_transfer.c        # NEW: TransferSubscriptions handler
│   └── service_dispatch.c         # MODIFIED: new dispatch table entries
├── services/
│   ├── subscription_aggregate.c   # MODIFIED: full 42 aggregate functions
│   ├── subscription_publish.c     # MODIFIED: event filter where-clause eval
│   ├── event_filter.c             # NEW: EventFilter evaluation engine
│   ├── audit_events.c             # NEW: audit event construction
│   ├── diagnostics.c              # NEW: diagnostics counter management
│   ├── reverse_connect.c          # NEW: server-initiated TCP connections
│   └── time_sync.c                # NEW: security time synchronization
├── address_space/
│   ├── base_nodes.c               # MODIFIED: diagnostics, redundancy, namespaces nodes
│   ├── analog_item.c              # NEW: AnalogItemType property definitions
│   └── complex_types.c            # NEW: Structure/Enum type registry
├── encoding/
│   └── binary_complex.c           # NEW: Structure/Enum binary encode/decode
└── platform/
    └── host_reverse_connect.c     # NEW: host reverse connect adapter (example)

tests/
├── unit/
│   ├── test_percent_deadband.c    # NEW
│   ├── test_method_call_arbitrary.c # NEW
│   ├── test_event_filter_where.c  # NEW
│   ├── test_audit_events.c        # NEW
│   ├── test_complex_types.c       # NEW
│   ├── test_diagnostics.c         # NEW
│   ├── test_aggregate_full.c      # NEW
│   ├── test_transfer_subscriptions.c # NEW
│   └── test_reverse_connect.c     # NEW
├── integration/
│   └── test_standard_profile.c    # NEW: end-to-end standard profile test
└── fuzz/
    ├── fuzz_event_filter_where.c  # NEW
    └── fuzz_complex_encode.c      # NEW

cmake/
├── MucOpcUaOptions.cmake          # MODIFIED: new feature flags
└── profiles/
    └── standard.cmake             # NEW: standard profile preset

CMakeLists.txt                      # MODIFIED: standard profile target
```

**Structure Decision**: New source files follow existing conventions (flat under `src/core/`, `src/services/`, `src/address_space/`, `src/encoding/`). Each facet gets one or more dedicated implementation files plus matching test files. CMake flags follow `MUC_OPCUA_<FACET>` naming. Profile preset in `cmake/profiles/standard.cmake`.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Event filter where-clause evaluation adds bounded stack recursion for nested ContentFilter elements | OPC-10000-4 §7.22.3 requires ContentFilter with ElementOperand and nested operator elements | Flat filter evaluation without nesting would fail to support standard AND/OR filter combinations used by real clients |
| StructureDefinition encoding table adds ~512 B static data | OPC-10000-3 §5.6.4 requires StructureField array in DataTypeDefinition attribute; must be stored for Read/Browse | Runtime-only structure definition would not survive Read/Browse discovery by clients |
| Reverse Connect adds a second TCP connection path | OPC-10000-6 §7.5 Reverse Connect is required for servers behind NAT/firewalls; without it the profile is incomplete | Disabling Reverse Connect keeps the server unreachable from certain network topologies; the flag lets users opt out |
