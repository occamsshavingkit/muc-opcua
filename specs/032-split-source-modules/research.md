# Research: Split Monolithic Source Files

**Feature**: 032-split-source-modules
**Date**: 2026-07-04

## Decision: Module boundary for subscription.c

**Decision**: Split into 3 modules: `subscription_monitor.c`, `subscription_publish.c`, `subscription_aggregate.c`.

**Rationale**: The existing function organization in subscription.c cleanly divides into three concerns:

1. **Monitor** (~500 lines): Monitored item lifecycle (`monitored_item_*`), sampling timer (`advance_sample_timer`), deadband evaluation, queue management. These functions operate on `mu_monitored_item_t` and `mu_subscription_t` structs but don't produce wire output.

2. **Publish** (~600 lines): Publish cycle (`mu_subscriptions_tick`), publish timer (`advance_publish_timer`), data-change notification encoding (`write_data_change_notification`), publish request management, republish. These produce wire output via `mu_binary_writer_t`.

3. **Aggregate** (~300 lines): Aggregate evaluation (`monitored_item_accumulate_aggregate`, `monitored_item_publish_aggregate`), triggered items, resend data, standard subscription feature functions. All guarded by `#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD`.

The remaining lines are the top-level `#if MUC_OPCUA_SUBSCRIPTIONS` guard and shared includes — these go away when the file is deleted.

**Alternatives considered**: Two-way split (monitor + publish) — aggregate functions would get lost in publish. Four-way split — over-fragmentation, aggregate is already behind `#ifdef` guards and doesn't warrant its own module.

---

## Decision: Module boundaries for service_dispatch.c

**Decision**: Extract 7 per-service-set handler files from service_dispatch.c, retaining the dispatch table and `mu_dispatch_service_request()` in the original file.

**Rationale**: The dispatch table at the top of the file binds handler function names to NodeIds. Each handler is self-contained — it receives the dispatch_writer, server, session, and request struct, and returns a status. Handlers for the same service set:
- Share data structures and validation logic
- Are gated by the same profile `#ifdef` macros
- Can be compiled out independently

The dispatch table provides a natural dependency injection — it already calls handlers by function pointer (or direct call), so moving handler bodies to separate translation units just requires forward declarations.

**Alternatives considered**: One file per handler — 14+ files, over-fragmentation. Grouped by line count instead of service set — loses semantic grouping. Extract everything including dispatch table — dispatch table and handler lookup belong together.

---

## Decision: Cross-module symbol approach

**Decision**: Add `extern` function declarations to existing internal headers (`server_internal.h`, `subscription.h`) rather than creating new headers per module.

**Rationale**: The project already uses `server_internal.h` for cross-module declarations in test contexts. Adding prototypes there is the least invasive approach. The existing `subscription.h` already declares the public API; internal declarations that were `static` and must become non-static can go there or in `server_internal.h`.

**Alternatives considered**: New header per module — adds 10 new `.h` files, creates a mini include web. Remove `static` and rely on external linkage — functions would be visible everywhere, losing encapsulation. Add prototypes in the source files that call them — duplicates declarations, violates DRY.

---

## Decision: CMakeLists.txt approach

**Decision**: Replace the single `subscription.c` source entry with 3 entries in the CMake source list. Add 7 entries for the new dispatch files. Profile-gated files use conditional source list additions.

**Rationale**: The project uses explicit source file lists. Subscription source files are conditional on `MUC_OPCUA_SUBSCRIPTIONS`. Per-service-set dispatch files use their respective feature macros (`MUC_OPCUA_SUBSCRIPTIONS`, `MUC_OPCUA_NODE_MANAGEMENT`, etc.).

**Alternatives considered**: `file(GLOB)` — the project explicitly avoids globs for source lists per the constitution (reproducible builds). Separate CMakeLists.txt per service directory — avoids the existing project structure.

---

## Decision: Function renaming policy

**Decision**: No function renames. Keep all existing function names exactly as-is. New files are purely a reorganization of code, not a rename.

**Rationale**: Renaming adds risk: miss one call site, get a linker error. The purpose of this refactor is file-size reduction and module cohesion, not naming improvements. Renames can be done separately if desired.

**Alternatives considered**: Add per-module prefixes — would affect the dispatch table entries and all call sites. Rejected for risk.
