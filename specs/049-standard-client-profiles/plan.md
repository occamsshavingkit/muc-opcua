# Implementation Plan: Standard UA Client 2025 Profile

**Branch**: `049-standard-client-profile` | **Date**: 2026-07-07 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/049-standard-client-profiles/spec.md`

## Summary

Implement the Standard UA Client 2025 Profile (`/Client/Standard2025`, UACore 1.05)
on top of the existing Minimum UA Client Profile (nano client). Adds client-side
subscription management (data change + event), monitored items, write service,
and method call service. All new code is gated on `MUC_OPCUA_CLIENT_PROFILE=standard`;
the nano profile continues unchanged.

## Technical Context

**Language/Version**: C11 (freestanding subset)
**Primary Dependencies**: Unity test framework, project-internal API headers
**Storage**: Client-side state structures in caller-provided static buffers (no heap)
**Testing**: ctest via Unity, `cmake --build . && ctest`
**Target Platform**: Host (Linux) for tests; same MCU targets as server core
**Project Type**: C library (client library built into the same archive)
**Performance Goals**: Subscription notifications processed within a single
`mu_server_poll` cycle; client API calls complete within 1 server poll cycle
**Constraints**: No heap allocation; all client state is static or caller-provided.
Nano profile binary size must not increase.
**Scale/Scope**: Single session, multiple subscriptions per session, multiple
monitored items per subscription. Expected: ~2000 lines new code.
**OPC UA Normative References**: OPC-10000-4 §5.11.4 (Write), §5.12.2 (Call),
§5.13 (MonitoredItem), §5.14 (Subscription). OPC-10000-5 §6.4.2 (Event).
OPC-10000-6 §5.2.2.15 (ExtensionObject), §5.2.2.17 (DataValue).
OPC-10000-7 §4.8 (Standard UA Client 2025 Profile).
**Target OPC UA Profile/Conformance Units**: Standard UA Client 2025 Profile
(`/Client/Standard2025`).
**Conformance Status Target**: experimental (tests added, no CTT verification).

## Embedded Size Budget

**Flash Impact**: Estimated +3–5 KB when `MUC_OPCUA_CLIENT_PROFILE=standard` is ON
(subscription state machine ~1.5 KB, monitored item queue ~1 KB, write/call
serialization ~1 KB, event notification parsing ~0.5 KB). +0 KB when OFF.
**RAM Impact**: Client subscription state arrays: ~200 bytes per subscription ×
max subscriptions + ~64 bytes per monitored item × max items. Zero when OFF.
**Heap Use**: 0 (all static or caller-provided).
**Static Tables Added**: Event field name-to-select-clause mapping table ~200 bytes.
**Transport Buffers**: Reuses existing client send/receive buffers (no increase).
**Crypto Dependency Impact**: None (no new crypto paths).

## Constitution Check

- **Spec Fidelity**: PASS. Every new client API maps to an OPC-10000-4 service.
  Subscriptions, Write, Call, and EventFilter behaviors cite exact sections.
- **Embedded C Core**: PASS. Client state is static arrays embedded in client struct.
  No RTOS or OS dependencies in the client core.
- **Memory Model**: PASS. Notification buffering uses caller-provided queue storage.
  No heap allocation in the client subscription path.
- **Minimal OPC UA Surface**: PASS. All new features gate on
  `MUC_OPCUA_CLIENT_PROFILE=standard`. Nano profile sees zero new code.
- **Profile Research**: PASS. Standard UA Client 2025 Profile identified from the
  OPC Foundation profiles registry (`/Client/Standard2025`).
- **Correctness Gates**: PASS. Each capability has a dedicated test (subscription
  round-trip, write-read-verify, method call, event subscription). Server-side
  tests unchanged.
- **Security Honesty**: PASS. No new security claims. Security is inherited from
  the existing client secure channel implementation.
- **Toolchain Discipline**: PASS. CMake build with `MUC_OPCUA_CLIENT_PROFILE`
  option. Existing `profile-tests` matrix unaffected.
- **Size Discipline**: PASS. Estimated +3–5 KB flash when enabled, 0 when disabled.

## Project Structure

### Source Code

New files:
```
src/client/
├── client.c               (existing — connect/discover/read/browse)
├── client_subscription.c  (NEW — subscription create/delete/modify)
├── client_monitored_item.c (NEW — monitored item create/delete/modify)
├── client_publish.c       (NEW — Publish request parking + response dispatch)
├── client_write.c         (NEW — Write service request/response)
├── client_call.c          (NEW — Call service request/response)
├── client_event.c         (NEW — EventNotificationList parsing)
├── client_internal.h      (existing — internal state structures)
├── client_errors.c        (existing)
├── client_read.c          (existing)
├── client_browse.c        (existing)
├── client_session.c       (existing)
├── client_secure_channel.c (existing)
└── client_state.c         (existing)
include/muc_opcua/
└── client.h               (extend with subscription/write/call/event APIs)
tests/unit/
└── test_nano_client.c     (extend with standard-profile tests)
```

## Complexity Tracking

No constitution violations. Justifications not needed.

## Phases

Phase 0 (Research), Phase 1 (Design), Phase 2 (Tasks), Phase 3 (Implementation).
