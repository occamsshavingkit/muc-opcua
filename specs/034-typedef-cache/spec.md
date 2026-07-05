# Feature Specification: TypeDefinition Cache on mu_node_t

**Feature Branch**: `034-typedef-cache`  
**Created**: 2026-07-04  
**Status**: Draft  
**Input**: "Cache TypeDefinition on mu_node_t at node creation to eliminate O(R*T) lookup in Browse reference loop (audit T22)"
**Source**: `docs/review/five-lens-audit-2026-07-04.md` T22

## User Scenarios & Testing

### User Story 1 — Eliminate O(R*T) TypeDefinition lookup in Browse (P1)

Every Browse response includes a `type_definition` field for each reference description.
The current code finds this by scanning ALL references of the target node looking for
HasTypeDefinition (reference_type_id=40). For N matched references across nodes with T
references each, this is O(N * T). Caching it on the node struct makes it O(1).

**Independent Test**: All 108 existing tests pass. Browse results identical to before.

**Acceptance Scenarios**:
1. Browse response `type_definition` field is identical to current (eager scan) behavior.
2. No new allocations, no heap use.

---

### Edge Cases

- **Nodes with no TypeDefinition**: `type_definition` field is zero-initialized (ns=0, id=0, type=NUMERIC).
- **Nodes with multiple HasTypeDefinition refs**: First one wins (same as current scan behavior).
- **Size impact**: `mu_nodeid_t` is 12 bytes per node. With ~200 nodes in full profile, ~2.4 KB extra flash (.rodata).

## Requirements

- **FR-001**: `mu_node_t` MUST include a `type_definition` field populated at node creation time.
- **FR-002**: Browse `type_definition` populates from `target->type_definition` instead of scanning target->references.
- **FR-003**: All 108 existing tests MUST pass with identical results.
- **FR-004**: No new heap, no new .bss, no new .data.

## Success Criteria

- **SC-001**: Browse path uses `target->type_definition` (no HasTypeDefinition scan loop).
- **SC-002**: 108/108 tests pass.

## Scope

- **In Scope**: Add field to mu_node_t, populate at node definition, use in browse.c.
- **Out of Scope**: Other TypeDefinition consumers (if any). Dynamic node creation.
