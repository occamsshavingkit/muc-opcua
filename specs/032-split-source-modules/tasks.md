# Tasks: Split Monolithic Source Files

**Input**: Design documents from `specs/032-split-source-modules/`
**Prerequisites**: plan.md, spec.md, research.md, quickstart.md

**Tests**: No new tests — this is a behavior-preserving refactor. All 108 existing tests serve as regression gates.

---

## Phase 1: Baseline Capture

**Purpose**: Record pre-split sizes and test results for comparison.

- [x] T001 Capture pre-split ARM size baseline: `BUILD_ROOT=build/size-032-baseline scripts/measure_size.sh all`. Record results for comparison after splits.

- [x] T002 Verify all 108 tests pass before any changes: `ctest -V`. All must be green.

**Checkpoint**: Baseline captured. All tests green. Proceed to splits.

---

## Phase 2: User Story 1 — Split subscription.c (Priority: P1) [MVP]

**Goal**: Replace `src/services/subscription.c` (1,825 lines) with 3 modules.

**Independent Test**: All 108 tests pass. Archive sizes match baseline.

### Implementation for User Story 1

- [x] T003 [US1] Create `src/services/subscription_monitor.c` by extracting monitored item lifecycle, sampling, deadband, and queue functions from `src/services/subscription.c`. Include: `variant_scalar_equal`, `variant_numeric_to_double`, `monitored_item_change_reportable`, `monitored_item_record_reported`, `monitored_item_effective_queue_size`, `monitored_item_queue_next`, `monitored_item_queue_previous`, `monitored_item_mark_overflow`, `monitored_item_enqueue_report`, `monitored_item_prepare_pending_queue`, `read_monitored_item_value`, `monitored_item_sample_changed`, `advance_sample_timer`. Add needed `static`→extern prototypes to `src/core/server_internal.h`. Wrap the file in `#if MUC_OPCUA_SUBSCRIPTIONS`.

- [x] T004 [US1] Create `src/services/subscription_publish.c` by extracting publish cycle, publish timer, notification encoding, and publish request functions from `src/services/subscription.c`. Include: `subscription_id_in_use`, `allocate_subscription_id`, `monitored_item_id_in_use`, `allocate_monitored_item_id`, `publish_request_dequeue`, `write_publish_response_prefix`, `backpatch_int32`, `monitored_item_reportable`, `count_queue_entries`, `write_data_change_notification`, `advance_publish_timer`, `mu_subscriptions_tick`. Add needed prototypes to `src/core/server_internal.h`. Wrap in `#if MUC_OPCUA_SUBSCRIPTIONS`.

- [x] T005 [US1] Create `src/services/subscription_aggregate.c` by extracting aggregate, triggered items, resend data, and STANDARD-only functions from `src/services/subscription.c`. Include: `monitored_item_accumulate_aggregate`, `monitored_item_publish_aggregate`, `find_monitored_item_in_subscription`, `monitored_item_in_subscription`, `monitored_item_reports_by_trigger`, and any other STANDARD-guarded helpers. Add needed prototypes to `src/core/server_internal.h`. Wrap in `#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD`.

- [x] T006 [US1] Add `subscription_monitor.c`, `subscription_publish.c`, `subscription_aggregate.c` to the CMake source list in `src/CMakeLists.txt`. Remove `subscription.c` from the list. Ensure profile-gated compilation: `subscription_aggregate.c` only compiled when `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` is enabled; the other two when `MUC_OPCUA_SUBSCRIPTIONS` is enabled.

- [x] T007 [US1] Delete `src/services/subscription.c`. Verify clean build with all profiles: `cmake --build build -j$(nproc)`. Run `ctest -V`. Run `BUILD_ROOT=build/size-032-us1 scripts/measure_size.sh all` and compare with baseline.

**Checkpoint**: subscription.c deleted. 3 new modules compile. All 108 tests pass. Sizes match baseline.

---

## Phase 3: User Story 2 — Extract dispatch handlers (Priority: P2)

**Goal**: Extract 7 service-set handler modules from `src/core/service_dispatch.c` (4,509 lines), retaining the dispatch table and `mu_dispatch_service_request()`.

**Independent Test**: All 108 tests pass. `service_dispatch.c` under 1,000 lines. Archive sizes match baseline.

### Implementation for User Story 2

- [x] T008 [US2] Create `src/core/dispatch_session.c` by extracting session service handlers from `src/core/service_dispatch.c`. Include: `handle_create_session` and all its static helpers, `handle_activate_session` and its helpers, `handle_close_session`. Add needed prototypes to `src/core/server_internal.h`. Wrap in appropriate `#ifdef` guards (session services are always compiled).

- [x] T009 [US2] Create `src/core/dispatch_discovery.c` by extracting discovery service handlers from `src/core/service_dispatch.c`. Include: `handle_get_endpoints`, `handle_find_servers`, `handle_register_server`. Add needed prototypes. Wrap in appropriate guards.

- [x] T010 [US2] Create `src/core/dispatch_attribute.c` by extracting Read and Write handlers from `src/core/service_dispatch.c`. Include: `handle_read`, `handle_write`. Add needed prototypes.

- [x] T011 [US2] Create `src/core/dispatch_view.c` by extracting Browse-family handlers from `src/core/service_dispatch.c`. Include: `handle_browse`, `handle_browse_next`, `handle_translate_browse_paths_to_node_ids`. Add needed prototypes.

- [x] T012 [US2] Create `src/core/dispatch_subscription.c` by extracting subscription service handlers from `src/core/service_dispatch.c`. Include: `handle_create_subscription`, `handle_modify_subscription`, `handle_delete_subscriptions`, `handle_set_publishing_mode`, `handle_publish`, `handle_republish`. Add needed prototypes. Wrap in `#if MUC_OPCUA_SUBSCRIPTIONS`.

- [x] T013 [US2] Create `src/core/dispatch_node_mgmt.c` by extracting node management handlers from `src/core/service_dispatch.c`. Include: `handle_add_nodes`, `handle_add_references`, `handle_delete_nodes`, `handle_delete_references`. Add needed prototypes. Wrap in `#if MUC_OPCUA_NODE_MANAGEMENT`.

- [x] T014 [US2] Create `src/core/dispatch_method.c` by extracting Call handler from `src/core/service_dispatch.c`. Include: `handle_call`. Add needed prototypes. Wrap in `#if MUC_OPCUA_METHOD_CALL`.

- [x] T015 [US2] Add all 7 new `dispatch_*.c` files to CMake source list in `src/CMakeLists.txt`. Ensure profile-gated compilation: subscription dispatch only with `MUC_OPCUA_SUBSCRIPTIONS`, node mgmt with `MUC_OPCUA_NODE_MANAGEMENT`, method with `MUC_OPCUA_METHOD_CALL`, others always compiled.

- [x] T016 [US2] Verify `service_dispatch.c` is under 1,000 lines after extraction and contains only: the dispatch table `g_supported_services[]`, `mu_dispatch_service_request()`, and shared helper functions (profile validation, endpoint helpers). Verify clean build with all profiles. Run `ctest -V`. Run `BUILD_ROOT=build/size-032-us2 scripts/measure_size.sh all` and compare with baseline.

**Checkpoint**: 7 dispatch modules extracted. service_dispatch.c < 1,000 lines. All 108 tests pass. Sizes match baseline.

---

## Phase 4: Polish & Cross-Cutting Validation

- [x] T017 Run full host test suite: `ctest -V`. All 108 tests must pass.

- [x] T018 Run ASAN + UBSan build: `cmake -B build-san -DMUC_OPCUA_SANITIZE=ON -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build build-san -j$(nproc) && ctest --test-dir build-san`. Zero failures.

- [x] T019 Run ARM sizes: `BUILD_ROOT=build/size-032-final scripts/measure_size.sh all`. Compare against baseline. Each profile archive `.text` must differ by <100 B.

- [x] T020 Run clang-format and static analysis on all new files: `clang-format --dry-run src/services/subscription_*.c src/core/dispatch_*.c src/core/service_dispatch.c`.

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 → Phase 2 → Phase 3 → Phase 4 (strictly sequential within each source file)
- Phase 2 tasks T003-T005 can run in parallel (they extract non-overlapping functions from the same source, but to different output files)
- Phase 3 tasks T008-T014 can run in parallel (different output files, non-overlapping handler extraction)

### Parallel Opportunities

| Can run in parallel | Tasks |
|---------------------|-------|
| US1 module creation | T003 ∥ T004 ∥ T005 (different output files) |
| US2 handler extraction | T008 ∥ T009 ∥ T010 ∥ T011 ∥ T012 ∥ T013 ∥ T014 (different output files) |

### Within Each Phase (Sequential)

- T006 (CMake) depends on T003-T005 (modules created first)
- T007 (delete + verify) depends on T006 (build config updated)
- T015 (CMake) depends on T008-T014 (handler files created first)
- T016 (verify) depends on T015 (build config updated)

### Suggested MVP Scope

**MVP = Phase 1 + Phase 2** (split subscription.c only). This delivers the simpler refactor first, validates the approach, and can be merged independently.

---

## Notes

- No function renames. All functions keep their current names exactly.
- `static` functions called across module boundaries must become non-static with an extern declaration in `server_internal.h`. Functions that stay within their module remain `static`.
- The dispatch table in `service_dispatch.c` already calls handler functions by name. After extraction, these functions must be declared extern in a header included by `service_dispatch.c`.
- Subscription STANDARD-guarded functions (`#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD`) move to `subscription_aggregate.c`. The other two modules use `#if MUC_OPCUA_SUBSCRIPTIONS`.
- Verify each module compiles for its target profiles before deleting the original file.
