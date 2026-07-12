# CU-Aligned Code Reorganization Design

**Goal:** Reorganize source code into per-Conformance-Unit directories, replace all
legacy gating flags with CU-level Kconfig symbols, and remove the dual-symbol
bridge system.  One CU → one symbol → one directory.

**Status:** approved  
**Date:** 2026-07-12

## Architecture

### Directory Layout

```
src/
  cu/                                    # per-CU implementation tree
    core_2022_server/                    # facet 1322
      attribute_read/                    # read_attribute.c, read_process.c
      view_basic_translate/              # browse.c, browse_next.c, translate_paths.c
      discovery/                         # get_endpoints.c, find_servers.c, register_server.c, common.c
      view_registernodes/                # stub (define-only in current impl)
      attribute_write/                   # write.c
      historical_access/                 # history.c
      nodemanagement/                    # add_nodes.c, add_references.c, delete.c
      query/                             # query.c, binary_query.c
      subscription/
        basic/                           # subscription_monitor.c, publish/*.c, subscription_crud.c, etc.
        standard/                        # subscription_aggregate.c, triggering.c
      security/
        key_derivation/                  # key_derivation.c
        certificate/                     # certificate.c, trustlist.c
        asym_chunk/                      # asym_chunk/*.c
        sym_chunk/                       # sym_chunk.c
        ecc/                             # asym_ecc.c
      event_filter_where/                # event_filter.c
      redundancy/                        # transfer_handler.c
      diagnostics/                       # diagnostics.c
    standard_datachange_subscription/    # facet 1324 (stubs for now)
    embedded_datachange_subscription/    # facet 2250 (stubs for now)
    global_certificate_management/       # facet 1631 (stubs for now)
    user_token_username_password/        # facet 1695 (stubs for now)
    user_token_x509/                     # facet 1696 (stubs for now)
    exposes_type_system/                 # facet 1219 (stubs for now)
    <stubs for remaining 60+ CUs>

  core/               # infrastructure shared across CUs
  encoding/           # shared encoding primitives
  address_space/      # shared address space, base_nodes.c
  security/           # shared security_policy.c (policy enums, no crypto)
  services/           # shared services not owned by a single CU
  generated/          # generated opcua_ids.c
```

All shared infrastructure that uses `#ifdef` blocks (`base_nodes.c`,
`server_init.c`, `secure_channel.c`, `service_header.c`, etc.) remains in its
current location and gets its `#ifdef` guards renamed from legacy flags to CU or
facet symbols.

### Gating Model

**One rule:** every source file and every `#ifdef` block gates on exactly one
`MUC_OPCUA_CU_*` or `MUC_OPCUA_FACET_*` Kconfig symbol.  No legacy names.
No dual-symbol bridge.

**Build gating** (src/CMakeLists.txt):

```cmake
if(MUC_OPCUA_CU_ATTRIBUTE_READ)
    file(GLOB_RECURSE sources src/cu/core_2022_server/attribute_read/*.c)
    target_sources(muc_opcua PRIVATE ${sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ATTRIBUTE_READ=1)
endif()
```

One `if` block per CU.  No bridge defines.

**Source gating** (#ifdef blocks in shared files):

```c
// Facet symbol — a block that serves the whole facet
#if MUC_OPCUA_FACET_CORE_2022_SERVER
    server->capabilities |= SERVER_CAP_CORE;
#endif

// CU symbol — a block owned by a single CU
#if MUC_OPCUA_CU_SECURITY_KEY_DERIVATION
    derive_keys(session);
#endif
```

### Legacy-to-CU Mapping

Each legacy flag is replaced by one or more new CU symbols.
The Kconfig manifest already defines the facet/CU hierarchy; we add project-level
CU items for features not yet tracked as discrete OPC CUs (e.g., subscriptions,
security sub-components).

The mapping is applied mechanically: move source files into `cu/<facet>/<cu>/`,
update `src/CMakeLists.txt` with the new paths and symbols, then `sed` every
`#ifdef` in the tree.  No behavioral changes.

### Kconfig Changes

- Delete the legacy compatibility symbols section (lines 971-1195 in current
  Kconfig: `BASE_NODES`, `SECURITY`, `ECC`, `SUBSCRIPTIONS`, etc.)
- Add new project-level CU items to the manifest for features that don't yet
  have discrete OPC CU entries (subscriptions, security sub-components, events,
  data access, method server, pubsub, reverse connect, diagnostics, etc.)
- Regenerate Kconfig — the `default y if INTERN_*` cascade automatically seeds
  the correct defaults

### Unimplemented CUs

Every unimplemented 2025 CU gets a stub directory under its facet:
```
src/cu/core_2022_server/address_space_addin_reference/
    stub.c    // #error "CU not implemented"
```

The Kconfig shows these as `comment "(NOT IMPLEMENTED)"` entries.
The build never reaches the `#error` because the CU symbol is `n`.
When a CU is claimed, the stub is replaced with real implementation and the
Kconfig symbol becomes selectable.

### Non-Goals

- Not rewriting any `#ifdef` block content — rename-only
- Not changing the Kconfig cascade or profile hierarchy
- Not touching `include/`, `tests/`, `src/platform/`, or `src/client/`
  (those get a follow-up pass)
- Not creating new implementations for unimplemented CUs

## Verification

After reorganization, these gates must still pass:

| Gate | Target |
|------|--------|
| `validate.py --all` | manifest: OK |
| `test_profile_gating.sh` | all passing (updated symbols) |
| Standard CTest | 100% |
| Full CTest | 100% |
| `git diff --check` | no output |
