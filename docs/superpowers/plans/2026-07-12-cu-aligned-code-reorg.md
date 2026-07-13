# CU-Aligned Code Reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move every source file into a per-CU directory under `src/cu/<facet>/<cu>/`, replace all legacy gating flags with CU-level Kconfig symbols, remove the dual-symbol bridge, and delete legacy compatibility symbols from Kconfig.

**Architecture:** Five-phase approach: (1) create directory structure with stubs, (2) move files and update CMakeLists.txt gating, (3) rename #ifdef guards across all C source, (4) update manifest/Kconfig symbol set, (5) verify builds and tests.

**Tech Stack:** C11, CMake, Kconfig (kconfiglib), bash, Python 3 stdlib.

## Global Constraints

- Worker agents must not run `git add` or `git commit`; controller-only checkpoints.
- Do not modify `include/`, `tests/`, `src/platform/` (crypto backends), or `src/client/` (client code) except where explicitly listed.
- Do not change any C function signatures, logic, or behavior -- rename-and-move only.
- `src/security/security_policy.c` stays in place (shared policy enums, no crypto).
- All verification gates must pass: `validate.py --all`, `test_profile_gating.sh`, standard/full CTest, `git diff --check`.
- The Kconfig internal cascade (`INTERN_PROFILE_*` symbols) and profile hierarchy must not change.

---

## Task 1: Create CU Directory Structure and Stub Files

**Files:**
- Create: `src/cu/` tree with per-facet/per-CU directories
- Create: stub `.c` files for unimplemented CUs

**Interfaces:**
- Consumes: Kconfig facet/CU symbol list from `Kconfig`
- Produces: directory tree `src/cu/` with empty stub files

This task creates the directory skeleton and stub files only -- no source files are moved yet.

### Directory Map

```
src/cu/
  core_2022_server/
    attribute_read/
    view_basic_translate/
    discovery/
    view_registernodes/
    attribute_write/
    historical_access/
    nodemanagement/
    query/
    subscription/
      basic/
      standard/
    security/
      key_derivation/
      certificate/
      asym_chunk/
      sym_chunk/
      ecc/
    event_filter_where/
    redundancy/
    diagnostics/
  standard_datachange_subscription/
  embedded_datachange_subscription/
  global_certificate_management/
  user_token_username_password/
  user_token_x509/
  exposes_type_system/
```

### Stub Content

For unimplemented CUs (those with `kconfig_symbol: null` in the manifest), create `stub.c`:

```c
/* Stub: CU not yet implemented */
#ifdef MUC_OPCUA_CU_<NAME>
#error "This CU is not yet implemented"
#endif
```

### Steps

- [ ] **Step 1: Create all directories**

```bash
mkdir -p src/cu/core_2022_server/{attribute_read,view_basic_translate,discovery,view_registernodes,attribute_write,historical_access,nodemanagement,query,subscription/{basic,standard},security/{key_derivation,certificate,asym_chunk,sym_chunk,ecc},event_filter_where,redundancy,diagnostics}
mkdir -p src/cu/standard_datachange_subscription
mkdir -p src/cu/embedded_datachange_subscription
mkdir -p src/cu/global_certificate_management
mkdir -p src/cu/user_token_username_password
mkdir -p src/cu/user_token_x509
mkdir -p src/cu/exposes_type_system
```

- [ ] **Step 2: Create stubs for all 60+ unimplemented CUs**

Use `grep 'comment.*NOT IMPLEMENTED' Kconfig` to list all unimplemented CU names. For each, create a directory under the matching facet and add `stub.c`.

- [ ] **Step 3: Controller checkpoint**

```bash
git add src/cu/
```

## Task 2: Move Source Files to CU Directories

**Files:**
- Move: gated source files from current locations to `src/cu/` tree
- Modify: `src/CMakeLists.txt` -- rewrite gating sections

**Interfaces:**
- Consumes: `src/cu/` directory tree from Task 1
- Produces: moved files, rewritten `src/CMakeLists.txt` with one `if(CU_SYMBOL)` block per CU

### File Move Map

Move each file using `git mv`. For every source-in-gating source file:
```
git mv src/<old-path> src/cu/<facet>/<cu>/<basename>
```

### Exact Move Table

| Old path | New path | CU symbol |
|----------|----------|-----------|
| `services/read/read_attribute.c` | `cu/core_2022_server/attribute_read/read_attribute.c` | `MUC_OPCUA_CU_ATTRIBUTE_READ` |
| `services/read/read_process.c` | `cu/core_2022_server/attribute_read/read_process.c` | `MUC_OPCUA_CU_ATTRIBUTE_READ` |
| `services/read/cache.c` | `cu/core_2022_server/attribute_read/cache.c` | `MUC_OPCUA_READ_CACHE` |
| `services/browse/browse.c` | `cu/core_2022_server/view_basic_translate/browse.c` | `MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH` |
| `services/browse/browse_next.c` | `cu/core_2022_server/view_basic_translate/browse_next.c` | `MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH` |
| `services/browse/translate_paths.c` | `cu/core_2022_server/view_basic_translate/translate_paths.c` | `MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH` |
| `core/service_dispatch/view_handler.c` | `cu/core_2022_server/view_basic_translate/view_handler.c` | `MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH` |
| `core/dispatch_discovery/common.c` | `cu/core_2022_server/discovery/common.c` | `MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS` |
| `core/dispatch_discovery/get_endpoints.c` | `cu/core_2022_server/discovery/get_endpoints.c` | same |
| `core/dispatch_discovery/find_servers.c` | `cu/core_2022_server/discovery/find_servers.c` | same |
| `core/dispatch_discovery/register_server.c` | `cu/core_2022_server/discovery/register_server.c` | same |
| `services/write.c` | `cu/core_2022_server/attribute_write/write.c` | `MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE` |
| `services/history.c` | `cu/core_2022_server/historical_access/history.c` | `MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET` |
| `core/service_dispatch/history_handler.c` | `cu/core_2022_server/historical_access/history_handler.c` | same |
| `services/node_management/add_nodes.c` | `cu/core_2022_server/nodemanagement/add_nodes.c` | `MUC_OPCUA_CU_NODEMANAGEMENT` |
| `services/node_management/add_references.c` | `cu/core_2022_server/nodemanagement/add_references.c` | same |
| `services/node_management/delete.c` | `cu/core_2022_server/nodemanagement/delete.c` | same |
| `core/dispatch_node_mgmt.c` | `cu/core_2022_server/nodemanagement/dispatch_node_mgmt.c` | same |
| `services/query.c` | `cu/core_2022_server/query/query.c` | `MUC_OPCUA_CU_QUERY` |
| `encoding/binary_query.c` | `cu/core_2022_server/query/binary_query.c` | same |
| `services/subscription_monitor.c` | `cu/core_2022_server/subscription/basic/subscription_monitor.c` | `MUC_OPCUA_SUBSCRIPTIONS` |
| `services/subscription_publish/publish_due.c` | `cu/core_2022_server/subscription/basic/publish_due.c` | same |
| `services/subscription_publish/tick.c` | `cu/core_2022_server/subscription/basic/tick.c` | same |
| `services/subscription_publish/notification.c` | `cu/core_2022_server/subscription/basic/notification.c` | same |
| `services/subscription_publish/deadband.c` | `cu/core_2022_server/subscription/basic/deadband.c` | same |
| `services/subscription_publish/retransmit.c` | `cu/core_2022_server/subscription/basic/retransmit.c` | same |
| `core/service_dispatch/subscription_crud.c` | `cu/core_2022_server/subscription/basic/subscription_crud.c` | same |
| `core/service_dispatch/monitored_items.c` | `cu/core_2022_server/subscription/basic/monitored_items.c` | same |
| `core/service_dispatch/filter_reader.c` | `cu/core_2022_server/subscription/basic/filter_reader.c` | same |
| `core/service_dispatch/subscription_helpers.c` | `cu/core_2022_server/subscription/basic/subscription_helpers.c` | same |
| `services/subscription_aggregate.c` | `cu/core_2022_server/subscription/standard/subscription_aggregate.c` | `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` |
| `core/service_dispatch/triggering.c` | `cu/core_2022_server/subscription/standard/triggering.c` | same |
| `core/dispatch_method.c` | `cu/core_2022_server/subscription/standard/dispatch_method.c` | `MUC_OPCUA_SUBSCRIPTIONS AND MUC_OPCUA_SUBSCRIPTIONS_STANDARD AND MUC_OPCUA_BASE_TYPE_SYSTEM` |
| `security/key_derivation.c` | `cu/core_2022_server/security/key_derivation.c` | `MUC_OPCUA_SECURITY` |
| `security/certificate.c` | `cu/core_2022_server/security/certificate.c` | same |
| `security/asym_chunk/wrap.c` | `cu/core_2022_server/security/asym_chunk/wrap.c` | same |
| `security/asym_chunk/unwrap.c` | `cu/core_2022_server/security/asym_chunk/unwrap.c` | same |
| `security/sym_chunk.c` | `cu/core_2022_server/security/sym_chunk.c` | same |
| `security/trustlist.c` | `cu/core_2022_server/security/trustlist.c` | same |
| `security/asym_ecc.c` | `cu/core_2022_server/security/ecc.c` | `MUC_OPCUA_ECC` |
| `services/event_filter.c` | `cu/core_2022_server/event_filter_where/event_filter.c` | `MUC_OPCUA_EVENT_FILTER_WHERE` |
| `core/service_dispatch/transfer_handler.c` | `cu/core_2022_server/redundancy/transfer_handler.c` | `MUC_OPCUA_REDUNDANCY` |
| `services/diagnostics.c` | `cu/core_2022_server/diagnostics/diagnostics.c` | `MUC_OPCUA_SERVER_DIAGNOSTICS` |
| `address_space/complex_types.c` | `cu/core_2022_server/security/../complex_types.c` | -- see below |
| `encoding/binary_complex.c` | `cu/core_2022_server/security/../binary_complex.c` | -- see below |
| `services/audit_events.c` | `cu/core_2022_server/security/../audit_events.c` | -- see below |
| `core/pubsub.c` | `cu/core_2022_server/subscription/basic/pubsub.c` | `MUC_OPCUA_PUBSUB` |
| `encoding/uadp_encoder.c` | `cu/core_2022_server/subscription/basic/uadp_encoder.c` | `MUC_OPCUA_PUBSUB` |

*Note: `complex_types.c`, `binary_complex.c`, and `audit_events.c` don't map to a specific CU name in the facet hierarchy yet. Place them under `cu/core_2022_server/` with a suitable subdirectory name matching their purpose (`complex_types/`, `auditing/`).*

*Note: `core/service_dispatch/attribute_handler.c` is shared between two CUs -- keep at `src/core/service_dispatch/attribute_handler.c` and update its gate to `MUC_OPCUA_CU_ATTRIBUTE_READ OR MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE` (unchanged pattern).*

### CMakeLists.txt Rewrite Pattern

Replace each gating block with a CU-directory glob:

```cmake
# -- CU: Attribute Read (facet: Core 2022 Server) --
if(MUC_OPCUA_CU_ATTRIBUTE_READ)
    file(GLOB_RECURSE _sources src/cu/core_2022_server/attribute_read/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ATTRIBUTE_READ=1)
endif()
```

**Critical rule:** one `if()` block per CU symbol. Each block:
1. Globs all `.c` files in the CU directory
2. Adds them to `muc_opcua`
3. Emits exactly one `target_compile_definitions` for the CU symbol
4. **No bridge defines** (`MUC_OPCUA_SERVICE_*` are gone)

Combined gates use `AND`/`OR`:
```cmake
if(MUC_OPCUA_CU_ATTRIBUTE_READ OR MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE)
    target_sources(muc_opcua PRIVATE core/service_dispatch/attribute_handler.c)
endif()
```

### Define-only symbols (legacy flags with no source files)

For flags that currently only emit `target_compile_definitions` but add no source files (`BASE_NODES`, `USER_AUTH`, `MULTIPLE_CONNECTIONS`, `EVENTS`, `DATA_ACCESS`, `METHOD_SERVER`, `CUSTOM_METHODS`, `NAMESPACES`, `MARKER_STANDARD_PROFILE`, `REVERSE_CONNECT`, `DYNAMIC_NODES`, `MULTI_CHUNK`, `SESSION_TIMEOUT`, `TIME_SYNC`, `EXTENDED_NODEIDS`, `AGGREGATE_FULL`):

- Keep the `target_compile_definitions` call unchanged for now
- They will be migrated to CU symbols in Task 5 when Kconfig items are created

Remove ALL `MUC_OPCUA_SERVICE_*` bridge defines from every block. No dual-symbol emission.

### Steps

- [ ] **Step 1: Move all files with `git mv`**

Execute every move from the table above using `git mv`.

- [ ] **Step 2: Rewrite src/CMakeLists.txt gating sections**

Replace all gating `if()` blocks (lines 43-388) with CU-directory-glob patterns. Keep always-compiled sources (lines 2-41) unchanged.

- [ ] **Step 3: Verify structure**

```bash
find src/cu -type f -name '*.c' | sort
grep -c 'if(MUC_OPCUA_CU_' src/CMakeLists.txt
```

- [ ] **Step 4: Controller checkpoint**

```bash
git add src/cu/ src/CMakeLists.txt
git add -u src/   # stage deletions of moved files
```

## Task 3: Rename #ifdef Guards in src/ (.c Files)

**Files:**
- Modify: all `.c` files under `src/` that contain `MUC_OPCUA_` guards

**Interfaces:**
- Consumes: moved source files from Task 2, legacy-to-CU mapping table
- Produces: all `#ifdef`/`#if` guards renamed to CU or facet symbols

### Mapping Rules

For every `#ifdef`/`#if` in `src/`, apply these renames. Files that were moved into `cu/` directories use the directory's CU symbol. Shared infrastructure files (`core/server/*.c`, `core/service_dispatch/*.c`, etc.) use the nearest applicable CU or facet symbol.

**Legacy flags to replace entirely:**

| Legacy flag | New symbol | Reason |
|---|---|---|
| `MUC_OPCUA_SERVICE_READ` | `MUC_OPCUA_CU_ATTRIBUTE_READ` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_BROWSE` | `MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_DISCOVERY` | `MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_REGISTER_NODES` | `MUC_OPCUA_CU_VIEW_REGISTERNODES` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_WRITE` | `MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_HISTORY` | `MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_NODEMANAGEMENT` | `MUC_OPCUA_CU_NODEMANAGEMENT` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_QUERY` | `MUC_OPCUA_CU_QUERY` | Bridge define, remove |
| `MUC_OPCUA_SERVICE_ALARMS_CONDITIONS` | `MUC_OPCUA_FACET_CORE_2022_SERVER` | Legacy service flag, gated by core facet |
| `MUC_OPCUA_SUBSCRIPTIONS` | `MUC_OPCUA_CU_SUBSCRIPTION_BASIC` | No CU in manifest yet; use temp name, formalize in Task 5 |
| `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` | `MUC_OPCUA_CU_SUBSCRIPTION_STANDARD` | Same |
| `MUC_OPCUA_SECURITY` | `MUC_OPCUA_FACET_CORE_2022_SERVER` | Security files are under core_2022 facet |
| `MUC_OPCUA_ECC` | `MUC_OPCUA_CU_SECURITY_ECC` | Separate CU for ECC |
| `MUC_OPCUA_EVENTS` | `MUC_OPCUA_CU_EVENTS` | Temp, formalize in Task 5 |
| `MUC_OPCUA_DATA_ACCESS` | `MUC_OPCUA_CU_DATA_ACCESS` | Temp, formalize in Task 5 |
| `MUC_OPCUA_METHOD_SERVER` | `MUC_OPCUA_CU_METHOD_SERVER` | Temp, formalize in Task 5 |
| `MUC_OPCUA_CUSTOM_METHODS` | `MUC_OPCUA_CU_CUSTOM_METHODS` | Temp, formalize in Task 5 |
| `MUC_OPCUA_BASE_NODES` | `MUC_OPCUA_FACET_CORE_2022_SERVER` | Base nodes gated by core facet |
| `MUC_OPCUA_BASE_TYPE_SYSTEM` | `MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER` | Type system facet |
| `MUC_OPCUA_NAMESPACES` | `MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER` | Namespace metadata under type system |
| `MUC_OPCUA_USER_AUTH` | `MUC_OPCUA_CU_USER_AUTH` | Temp, formalize in Task 5 |
| `MUC_OPCUA_MULTIPLE_CONNECTIONS` | `MUC_OPCUA_CU_MULTIPLE_CONNECTIONS` | Temp |
| `MUC_OPCUA_EVENT_FILTER_WHERE` | `MUC_OPCUA_CU_EVENT_FILTER_WHERE` | Already a CU-like name |
| `MUC_OPCUA_REDUNDANCY` | `MUC_OPCUA_CU_REDUNDANCY` | Temp |
| `MUC_OPCUA_SERVER_DIAGNOSTICS` | `MUC_OPCUA_CU_DIAGNOSTICS` | Temp |
| `MUC_OPCUA_COMPLEX_TYPES` | `MUC_OPCUA_CU_COMPLEX_TYPES` | Temp |
| `MUC_OPCUA_AUDITING` | `MUC_OPCUA_CU_AUDITING` | Temp |
| `MUC_OPCUA_DYNAMIC_NODES` | `MUC_OPCUA_CU_DYNAMIC_NODES` | Temp |
| `MUC_OPCUA_MULTI_CHUNK` | `MUC_OPCUA_CU_MULTI_CHUNK` | Temp |
| `MUC_OPCUA_SESSION_TIMEOUT` | `MUC_OPCUA_CU_SESSION_TIMEOUT` | Temp |
| `MUC_OPCUA_TIME_SYNC` | `MUC_OPCUA_CU_TIME_SYNC` | Temp |
| `MUC_OPCUA_EXTENDED_NODEIDS` | `MUC_OPCUA_CU_EXTENDED_NODEIDS` | Temp |
| `MUC_OPCUA_AGGREGATE_FULL` | `MUC_OPCUA_CU_AGGREGATE_FULL` | Temp |
| `MUC_OPCUA_PUBSUB` | `MUC_OPCUA_CU_PUBSUB` | Temp |
| `MUC_OPCUA_REVERSE_CONNECT` | `MUC_OPCUA_CU_REVERSE_CONNECT` | Temp |
| `MUC_OPCUA_READ_CACHE` | `MUC_OPCUA_CU_ATTRIBUTE_READ` opt-in | Keep as separate symbol or merge |
| `MUC_OPCUA_MARKER_STANDARD_PROFILE` | `MUC_OPCUA_MARKER_STANDARD_PROFILE` | Unchanged |
| `MUC_OPCUA_MDNS_DISCOVERY` | `MUC_OPCUA_MDNS_DISCOVERY` | Platform flag, unchanged |
| `MUC_OPCUA_ALLOW_HEAP` | `MUC_OPCUA_ALLOW_HEAP` | Platform flag, unchanged |
| `MUC_OPCUA_IS_HOST` | `MUC_OPCUA_IS_HOST` | Platform flag, unchanged |
| `MUC_OPCUA_STATUS_STRINGS` | `MUC_OPCUA_STATUS_STRINGS` | Unchanged |
| `MUC_OPCUA_HAVE_OPENSSL` | `MUC_OPCUA_HAVE_OPENSSL` | Platform, unchanged |
| `MUC_OPCUA_HAVE_MBEDTLS` | `MUC_OPCUA_HAVE_MBEDTLS` | Platform, unchanged |
| `MUC_OPCUA_HAVE_WOLFSSL` | `MUC_OPCUA_HAVE_WOLFSSL` | Platform, unchanged |
| `MUC_OPCUA_CLIENT_NANO` | `MUC_OPCUA_CLIENT_NANO` | Client, unchanged |
| `MUC_OPCUA_CLIENT_STANDARD` | `MUC_OPCUA_CLIENT_STANDARD` | Client, unchanged |

### Execution

Use `sed` to perform all replacements. For combined guards like `#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM`, the sed must handle the compound expression.

- [ ] **Step 1: Rename all `#ifdef`/`#if` guards in `src/**/*.c`**

Apply every mapping from the table above using `sed`.

- [ ] **Step 2: Rename all `#ifdef`/`#if` guards in `src/**/*.h`**

Same mapping, apply to headers.

- [ ] **Step 3: Verify no legacy guards remain**

```bash
grep -r 'MUC_OPCUA_SERVICE_\|MUC_OPCUA_SUBSCRIPTIONS\b\|MUC_OPCUA_SECURITY\b' src/ | grep -v 'FACET_CORE_2022' | grep -v 'CU_SECURITY'
```

Expected: no output (all legacy flags replaced). Some replacements are facet symbols (`FACET_CORE_2022_SERVER`) which are fine.

- [ ] **Step 4: Controller checkpoint**

```bash
git add src/
```

## Task 4: Add New CU Kconfig Symbols to Manifest + Regenerate

**Files:**
- Modify: `profiles/opcua-profile-manifest.yaml` -- add project-level CU items
- Modify: `CMakeLists.txt` -- update Kconfig features list
- Modify: `Kconfig` -- regenerated
- Modify: all generated files

**Interfaces:**
- Consumes: renamed guards from Task 3 (defines the new symbol names needed)
- Produces: manifest with new CU items, regenerated Kconfig, all symbols exportable

### New CU Items to Add

For every new CU symbol introduced in Task 3 (temp names), add a corresponding item to the manifest under `opc_facet_1322` (Core 2022 Server Facet):

| id | kconfig_symbol | kind | opc_display_name |
|---|---|---|---|
| `opc_cu_subscription_basic` | `MUC_OPCUA_CU_SUBSCRIPTION_BASIC` | conformance_unit | Subscription Basic |
| `opc_cu_subscription_standard` | `MUC_OPCUA_CU_SUBSCRIPTION_STANDARD` | conformance_unit | Subscription Standard |
| `opc_cu_security_ecc` | `MUC_OPCUA_CU_SECURITY_ECC` | conformance_unit | Security ECC |
| `opc_cu_events` | `MUC_OPCUA_CU_EVENTS` | conformance_unit | Event Notifications |
| `opc_cu_data_access` | `MUC_OPCUA_CU_DATA_ACCESS` | conformance_unit | Data Access Server |
| `opc_cu_method_server` | `MUC_OPCUA_CU_METHOD_SERVER` | conformance_unit | Method Server |
| `opc_cu_custom_methods` | `MUC_OPCUA_CU_CUSTOM_METHODS` | conformance_unit | Custom Methods |
| `opc_cu_user_auth` | `MUC_OPCUA_CU_USER_AUTH` | conformance_unit | User Authentication |
| `opc_cu_multiple_connections` | `MUC_OPCUA_CU_MULTIPLE_CONNECTIONS` | conformance_unit | Multiple Connections |
| `opc_cu_event_filter_where` | `MUC_OPCUA_CU_EVENT_FILTER_WHERE` | conformance_unit | Event Filter Where |
| `opc_cu_redundancy` | `MUC_OPCUA_CU_REDUNDANCY` | conformance_unit | Client Redundancy |
| `opc_cu_diagnostics` | `MUC_OPCUA_CU_DIAGNOSTICS` | conformance_unit | Server Diagnostics |
| `opc_cu_complex_types` | `MUC_OPCUA_CU_COMPLEX_TYPES` | conformance_unit | Complex Types |
| `opc_cu_auditing` | `MUC_OPCUA_CU_AUDITING` | conformance_unit | Audit Events |
| `opc_cu_dynamic_nodes` | `MUC_OPCUA_CU_DYNAMIC_NODES` | conformance_unit | Dynamic Nodes |
| `opc_cu_multi_chunk` | `MUC_OPCUA_CU_MULTI_CHUNK` | conformance_unit | Multi-Chunk Messages |
| `opc_cu_session_timeout` | `MUC_OPCUA_CU_SESSION_TIMEOUT` | conformance_unit | Session Timeout |
| `opc_cu_time_sync` | `MUC_OPCUA_CU_TIME_SYNC` | conformance_unit | Time Synchronization |
| `opc_cu_extended_nodeids` | `MUC_OPCUA_CU_EXTENDED_NODEIDS` | conformance_unit | Extended NodeIds |
| `opc_cu_aggregate_full` | `MUC_OPCUA_CU_AGGREGATE_FULL` | conformance_unit | Aggregate Full |
| `opc_cu_pubsub` | `MUC_OPCUA_CU_PUBSUB` | conformance_unit | PubSub |
| `opc_cu_reverse_connect` | `MUC_OPCUA_CU_REVERSE_CONNECT` | conformance_unit | Reverse Connect |
| `opc_cu_namespaces` | `MUC_OPCUA_CU_NAMESPACES` | conformance_unit | Namespace Metadata |

Each item gets:
- `implementation_state: "claimed"` (for items that have working code) or `"unimplemented"`
- `profile_defaults` derived from the owning facet's minimum profile
- `opc_reference` with appropriate facet/section
- Entry in `facet_containment` under `opc_facet_1322`

### CMakeLists.txt Update

Replace the legacy symbol list in `MUC_OPCUA_KCONFIG_FEATURES` (current lines 56-126) with the new CU symbols. Keep only symbols that exist in the regenerated Kconfig.

### Steps

- [ ] **Step 1: Add new CU items to manifest**

Manually edit `profiles/opcua-profile-manifest.yaml` -- add each new CU item to the `items` array and add containment to `facet_containment.opc_facet_1322`.

- [ ] **Step 2: Set correct profile_defaults**

items that were previously controlled by `MUC_OPCUA_SECURITY` (now `FACET_CORE_2022_SERVER`) get defaults from that facet's profile range. Items that were `MUC_OPCUA_BASE_TYPE_SYSTEM` get defaults from `FACET_EXPOSES_TYPE_SYSTEM_SERVER`. Items that were available from nano up get `nano=true` through `full=true`.

- [ ] **Step 3: Regenerate all artifacts**

```bash
python3 scripts/profile_manifest/generate.py \
  --manifest profiles/opcua-profile-manifest.yaml \
  --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
```

- [ ] **Step 4: Update CMakeLists.txt Kconfig features list**

Replace `CMakeLists.txt` lines 56-126 with only the new CU symbols plus platform flags.

- [ ] **Step 5: Validate manifest**

```bash
python3 scripts/profile_manifest/validate.py --manifest-only
```

Expected: `manifest: OK`.

- [ ] **Step 6: Controller checkpoint**

```bash
git add profiles/opcua-profile-manifest.yaml CMakeLists.txt Kconfig configs/ include/ tests/conformance/ docs/
```

## Task 5: Delete Legacy Compatibility Symbols from Kconfig

**Files:**
- Modify: `profiles/opcua-profile-manifest.yaml` -- remove legacy standalone facets
- Modify: `Kconfig` -- regenerated (removes the legacy compatibility section)

**Interfaces:**
- Consumes: regenerated Kconfig from Task 4
- Produces: Kconfig with only CU and facet symbols, no legacy short names

### Items to Remove

Delete all legacy standalone items from the manifest that had short `kconfig_symbol` names like `SECURITY`, `ECC`, `BASE_NODES`, `SUBSCRIPTIONS`, etc. These were the bridge/compatibility layer. Their functionality is now covered by CU-level symbols.

Remove these items from `items` array in the manifest:
- All items with `kconfig_symbol` that is NOT `MUC_OPCUA_CU_*`, NOT `MUC_OPCUA_FACET_*`, NOT `MUC_OPCUA_MARKER_*`
- Specifically: `security`, `ecc`, `user_auth`, `base_nodes`, `base_type_system`, `extended_nodeids`, `namespaces`, `dynamic_nodes`, `complex_types`, `multiple_connections`, `session_timeout`, `multi_chunk`, `time_sync`, `auditing`, `subscriptions`, `subscriptions_standard`, `events`, `aggregate_full`, `event_filter_where`, `redundancy`, `method_server`, `custom_methods`, `pubsub`, `data_access`, `reverse_connect`, `server_diagnostics`

Also remove their entries from `facet_containment`.

### Steps

- [ ] **Step 1: Remove legacy items from manifest**

Edit manifest, delete legacy items.

- [ ] **Step 2: Regenerate Kconfig**

```bash
python3 scripts/profile_manifest/generate.py \
  --manifest profiles/opcua-profile-manifest.yaml \
  --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
```

Verify: `grep -c 'config SECURITY\b\|config ECC\b\|config BASE_NODES\b' Kconfig` returns 0.

- [ ] **Step 3: Validate**

```bash
python3 scripts/profile_manifest/validate.py --all
```

Expected: `manifest: OK`.

- [ ] **Step 4: Controller checkpoint**

```bash
git add profiles/opcua-profile-manifest.yaml Kconfig configs/ include/ tests/conformance/ docs/
```

## Task 6: Update Gating Tests and CMakeLists.txt's Features List

**Files:**
- Modify: `scripts/test_profile_gating.sh` -- update symbol names
- Modify: `CMakeLists.txt` -- sync `MUC_OPCUA_KCONFIG_FEATURES` list

### Steps

- [ ] **Step 1: Update gating test symbols**

Replace all legacy symbol references with new CU symbols. The test patterns remain the same (profile cascade, facet/CU defaults, superseded absence, custom profile).

- [ ] **Step 2: Run gating tests**

```bash
bash scripts/test_profile_gating.sh
```

Expected: all passing (count may change from 96 due to structural changes).

- [ ] **Step 3: Sync CMakeLists.txt Kconfig features**

Ensure `MUC_OPCUA_KCONFIG_FEATURES` in the top-level `CMakeLists.txt` lists all new CU symbols. Remove legacy entries.

- [ ] **Step 4: Controller checkpoint**

```bash
git add scripts/test_profile_gating.sh CMakeLists.txt
```

## Task 7: End-to-End Build Verification

**Files:**
- No source edits expected.
- Build outputs under `build/cu-reorg-standard` and `build/cu-reorg-full` are not committed.

### Steps

- [ ] **Step 1: Standard profile**

```bash
cmake -S . -B build/cu-reorg-standard -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/cu-reorg-standard
ctest --test-dir build/cu-reorg-standard --output-on-failure
```

Expected: 100% tests passed.

- [ ] **Step 2: Full profile**

```bash
cmake -S . -B build/cu-reorg-full -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/cu-reorg-full
ctest --test-dir build/cu-reorg-full --output-on-failure
```

Expected: 100% tests passed.

- [ ] **Step 3: Final validation**

```bash
python3 scripts/profile_manifest/validate.py --all
bash scripts/test_profile_gating.sh
git diff --check
```

Expected: all pass.

- [ ] **Step 4: Controller checkpoint for docs**

Stage any verification-only doc changes.

## Self-Review

Spec coverage:
- Directory structure → Task 1
- File moves + CMakeLists.txt rewrite → Task 2
- #ifdef guard rename → Task 3
- Kconfig symbol set update → Tasks 4, 5
- Gating test update → Task 6
- Build verification → Task 7

Placeholder scan: no TBD/TODO placeholders. All mappings are explicit. Some new CU symbol names use temp names that will be formalized in Task 4.

Type consistency: CU symbol names introduced in Task 3 are used consistently in Tasks 4-7.
