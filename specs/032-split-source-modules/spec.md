# Feature Specification: Split Monolithic Source Files

**Feature Branch**: `032-split-source-modules`  
**Created**: 2026-07-04  
**Status**: Draft  
**Input**: "Split subscription.c (1,825 lines) into publish, monitor, filter modules and service_dispatch.c (4,509 lines) into per-service-set dispatch modules"

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Split subscription.c into concern-based modules (Priority: P1)

A maintainer reading or modifying subscription behavior should find related code in
one place. Currently all subscription logic — monitored item lifecycle, sampling,
deadband evaluation, queue management, publish cycle, data-change notification
encoding, triggering, resend data — lives in a single 1,825-line file. Splitting
into modules grouped by concern makes each file independently understandable.

**Split**: Three modules:
- `subscription_monitor.c` — monitored item lifecycle, sampling timer, deadband/filter evaluation, queue management (~500 lines)
- `subscription_publish.c` — publish cycle, publish timer, data-change notification encoding, publish request management (~600 lines)
- `subscription_aggregate.c` — aggregate/triggered item support, resend data, standard subscription features (~300 lines)

**Why this priority**: subscription.c is the smaller of the two monoliths and the
most self-contained. Its internal helpers (monitored item functions, publish
engine) already form natural boundaries. Splitting it first validates the approach
before tackling the larger service_dispatch.c.

**Independent Test**: All 108 existing tests pass with zero behavioral changes.
`arm-none-eabi-size` shows identical `.text`, `.data`, `.bss` across all profiles
(modulo link-order noise).

**Acceptance Scenarios**:

1. **Given** the split modules, **When** compiled for each profile (nano, micro, embedded, full), **Then** all 108 tests pass with identical archive `.text` totals.
2. **Given** `subscription_monitor.c`, **When** read, **Then** it contains only monitored item lifecycle, sampling, deadband, and queue functions.
3. **Given** `subscription_publish.c`, **When** read, **Then** it contains only publish cycle, publish timer, notification encoding, and publish request functions.
4. **Given** `subscription_aggregate.c`, **When** read, **Then** it contains only aggregate evaluation, triggered items, and standard-subscription features.

---

### User Story 2 — Extract per-service-set dispatch from service_dispatch.c (Priority: P2)

A maintainer adding or modifying an OPC UA service handler should find it in a
module grouped by service set, not buried among 4,500 lines of heterogeneous
handlers. The dispatch table and shared helpers remain in service_dispatch.c; each
service-set's handler functions move to dedicated files.

**Split**: Extract handlers into per-service-set files:
- `dispatch_session.c` — CreateSession, ActivateSession, CloseSession (~600 lines)
- `dispatch_discovery.c` — GetEndpoints, FindServers, RegisterServer (~350 lines)
- `dispatch_attribute.c` — Read, Write (~300 lines)
- `dispatch_view.c` — Browse, BrowseNext, TranslateBrowsePathsToNodeIds (~400 lines)
- `dispatch_subscription.c` — CreateSubscription, ModifySubscription, DeleteSubscriptions, SetPublishingMode, Publish, Republish (~500 lines)
- `dispatch_node_mgmt.c` — AddNodes, AddReferences, DeleteNodes, DeleteReferences (~350 lines)
- `dispatch_method.c` — Call (~150 lines)

The dispatch table (`g_supported_services[]`) and `mu_dispatch_service_request()`
remain in `service_dispatch.c`.

**Why this priority**: P2 because service_dispatch.c is the larger monolith and the
extraction is more mechanical (handler → file). Each handler is already a standalone
function called only through the dispatch table. The risk is low but the surface
area is larger.

**Independent Test**: All 108 existing tests pass. Each new dispatch file includes
only handlers for its service set. Profile `#ifdef` guards are preserved per-file.

**Acceptance Scenarios**:

1. **Given** the split dispatch modules, **When** compiled for all profiles, **Then** all 108 tests pass with identical archive `.text` totals.
2. **Given** `dispatch_session.c`, **When** read, **Then** it contains only CreateSession, ActivateSession, and CloseSession handlers.
3. **Given** each dispatch file, **When** a profile excludes a service set (e.g., `MUC_OPCUA_SUBSCRIPTIONS` off), **Then** the corresponding dispatch file is not compiled or compiles to empty.
4. **Given** `service_dispatch.c`, **When** read after extraction, **Then** it contains only the dispatch table, `mu_dispatch_service_request()`, and shared validation helpers.

---

### Edge Cases

- **Symbol visibility**: Internal helper functions currently `static` in the original
  files must not break calling conventions when moved to new translation units. Functions
  called across module boundaries must be declared in internal headers or made non-static.
- **Profile `#ifdef` guards**: Each new module must carry the correct profile guards
  (e.g., `MUC_OPCUA_SUBSCRIPTIONS_STANDARD`, `MUC_OPCUA_NODE_MANAGEMENT`) so profiles
  that exclude a service set don't link the extracted module.
- **Link-order sensitivity**: Functions moved between translation units must not change
  the effective link order for any profile. All profiles must produce identical archive
  `.text` totals (modulo alignment noise).
- **Internal header proliferation**: Avoid creating a new header per extracted file.
  Use existing internal headers (`server_internal.h`, `subscription.h`) where possible.
- **CMakeLists**: New `.c` files must be added to the build. The project uses explicit
  file lists (not globs) for source files.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `subscription_monitor.c` MUST contain only monitored item lifecycle, sampling timer, deadband evaluation, and queue management functions.
- **FR-002**: `subscription_publish.c` MUST contain only publish cycle, publish timer, notification encoding, and publish request functions.
- **FR-003**: `subscription_aggregate.c` MUST contain only aggregate evaluation, triggered items, resend data, and standard subscription feature functions.
- **FR-004**: `dispatch_session.c` MUST contain only CreateSession, ActivateSession, and CloseSession handler functions.
- **FR-005**: `dispatch_discovery.c` MUST contain only GetEndpoints, FindServers, and RegisterServer handler functions.
- **FR-006**: `dispatch_attribute.c` MUST contain only Read and Write handler functions.
- **FR-007**: `dispatch_view.c` MUST contain only Browse, BrowseNext, and TranslateBrowsePathsToNodeIds handler functions.
- **FR-008**: `dispatch_subscription.c` MUST contain only CreateSubscription, ModifySubscription, DeleteSubscriptions, SetPublishingMode, Publish, and Republish handler functions.
- **FR-009**: `dispatch_node_mgmt.c` MUST contain only AddNodes, AddReferences, DeleteNodes, and DeleteReferences handler functions.
- **FR-010**: `dispatch_method.c` MUST contain only the Call handler function.
- **FR-011**: `service_dispatch.c` MUST retain the dispatch table (`g_supported_services[]`), `mu_dispatch_service_request()`, and shared helper functions after extraction.
- **FR-012**: All 108 existing tests MUST pass with zero failures across all four profiles.
- **FR-013**: ARM profile archive `.text` totals MUST be within 0.1% of pre-split totals (link-order noise only; no code change).
- **FR-014**: `.data`, `.bss`, and heap MUST remain 0 across all profiles.

### Scope Boundaries *(mandatory)*

- **In Scope**: Extract functions from `subscription.c` (1,825 lines) into 3 modules and from `service_dispatch.c` (4,509 lines) into 7 handler modules. Update CMakeLists.txt. Declare cross-module function prototypes in existing internal headers.
- **Out of Scope**: Renaming functions, changing function signatures, modifying logic, adding features, changing public API in `include/muc_opcua/`. No new conformance claims.
- **Compatibility Claim**: Nano/Micro/Embedded 2017 + full profiles unchanged. All profile `#ifdef` guards preserved.
- **Application Headroom Goal**: Archive `.text` within 0.1% of pre-split (link-order noise only). `.data`, `.bss`, heap = 0.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `subscription.c` is replaced by 3 files each under 700 lines.
- **SC-002**: `service_dispatch.c` is under 1,000 lines after extraction (currently 4,509).
- **SC-003**: 108/108 tests pass across all four ARM profiles.
- **SC-004**: Archive `.text` per profile differs from pre-split by <100 bytes (link-order noise only).
- **SC-005**: No new `.data`, `.bss`, or heap symbols across any profile.

## Assumptions

- All internal helper functions called across module boundaries can be declared in existing headers (`server_internal.h`, `subscription.h`) without creating new header files.
- Profile `#ifdef` guards are self-contained at the function level — functions guarded by `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` only call other functions behind the same guard or non-guarded functions.
- The CMakeLists.txt uses explicit source file lists. New files must be added individually.
- No function is called by name through a function pointer outside its source file (all cross-module calls are direct).
