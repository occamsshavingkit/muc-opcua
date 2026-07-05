# Implementation Plan: TypeDefinition Cache

**Branch**: `034-typedef-cache` | **Date**: 2026-07-04 | **Spec**: [spec.md](./spec.md)

## Summary

Add `type_definition` field to `mu_node_t` struct, populated at node definition time
via a new `.type_definition` initializer in every static node definition. Remove the
O(R*T) HasTypeDefinition scan from `browse.c:360-368`, replacing with direct field read.

## Technical Context

**Language/Version**: Freestanding C11
**Testing**: Existing ctest suite (108 tests)
**Size Impact**: ~+2.4 KB flash (mu_nodeid_t per node in static address space). No RAM impact.

## Constitution Check

- **I. Spec Fidelity**: PASS — Browse response unchanged.
- **II. Embedded-First C Core**: PASS — static data, no runtime change.
- **VII. Size Discipline**: PASS — ~2.4 KB flash for ~200 nodes. Acceptable trade for browse path speedup.

## Project Structure

```text
include/muc_opcua/address_space.h — add type_definition to mu_node_t
src/services/browse.c — replace scan with direct read
src/address_space/base_nodes.c — add .type_definition initializers
examples/minimal_server/static_address_space.c — add .type_definition initializers
```
