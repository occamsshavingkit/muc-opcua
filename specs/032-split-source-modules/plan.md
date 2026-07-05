# Implementation Plan: Split Monolithic Source Files

**Branch**: `032-split-source-modules` | **Date**: 2026-07-04 | **Spec**: [spec.md](./spec.md)

## Summary

Split `subscription.c` (1,825 lines) into 3 concern-based modules and extract 7 service-set handler modules from `service_dispatch.c` (4,509 lines). This is a behavior-preserving refactor — zero logic changes, zero wire-visible changes. All internal functions move as-is; cross-module prototypes added to existing internal headers.

**Technical approach**: Cut-and-paste functions into new `.c` files, declare prototypes for cross-module calls in existing headers, add new files to CMakeLists.txt. Verify with `ctest` and `measure_size.sh all` at each checkpoint.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged)
**Primary Dependencies**: None new
**Storage**: N/A
**Testing**: Existing ctest suite (108 tests). No new tests — this is purely a refactor.
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Project Type**: Portable C library (unchanged)
**Performance Goals**: Archive `.text` within 0.1% of pre-split baseline (link-order noise only)
**Constraints**: No code changes, no signature changes, no new headers, `.data`/`.bss`/heap = 0
**Scale/Scope**: 2 source files split into 10 total; CMakeLists.txt updated; 2 new internal header declarations

## Embedded Size Budget

**Flash Impact**: 0 bytes (modulo link-order noise, <100 B)
**RAM Impact**: 0 bytes
**Heap Use**: None (unchanged)
**Static Tables Added**: None
**Transport Buffers**: Unchanged
**Crypto Dependency Impact**: None

## Constitution Check

- **I. Spec Fidelity**: PASS — no spec-visible behavior changes.
- **II. Embedded-First C Core**: PASS — all new files are freestanding C11, same platform constraints.
- **III. Minimal OPC UA Surface**: PASS — no new services, no API changes.
- **IV. Protocol Correctness Gates**: PASS — all 108 existing tests serve as regression gates.
- **V. Security and Conformance Honesty**: PASS — no security or conformance changes.
- **VI. Fixed Toolchain**: PASS — CMake/ctest unchanged; new files added to existing file lists.
- **VII. Size Discipline**: PASS — size verified at each checkpoint.

**Result: PASS (no violations).**

## Project Structure

### Source Code

```text
src/services/
├── subscription.c          → DELETED after split
├── subscription_monitor.c  # NEW: monitored item lifecycle, sampling, deadband, queues
├── subscription_publish.c  # NEW: publish cycle, timer, notification encoding
├── subscription_aggregate.c # NEW: aggregate, triggered, resend data, standard features
src/core/
├── service_dispatch.c      # RETAINED: dispatch table, mu_dispatch_service_request, helpers
├── dispatch_session.c      # NEW: CreateSession, ActivateSession, CloseSession
├── dispatch_discovery.c    # NEW: GetEndpoints, FindServers, RegisterServer
├── dispatch_attribute.c    # NEW: Read, Write
├── dispatch_view.c         # NEW: Browse, BrowseNext, TranslateBrowsePathsToNodeIds
├── dispatch_subscription.c # NEW: subscription service set handlers
├── dispatch_node_mgmt.c    # NEW: AddNodes, AddReferences, DeleteNodes, DeleteReferences
├── dispatch_method.c       # NEW: Call handler
src/core/server_internal.h  # Updated: add cross-module prototypes
src/services/subscription.h # Updated: if needed for cross-module declarations
```
