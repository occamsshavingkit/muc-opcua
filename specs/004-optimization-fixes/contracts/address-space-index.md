# Contract — Address-Space NodeId Index

**Finding**: T3 (also T4, T8 benefit) · **FR**: FR-009/010/011 · **Decision**: D3 · **Files**: `include/micro_opcua/address_space.h`, `src/address_space/node_id.c`

## Intent

Make `mu_address_space_find_node` sub-linear without changing its signature,
semantics, or the caller-provided const node array. No heap.

## Public behaviour (unchanged signature)

```c
const mu_node_t *mu_address_space_find_node(
    const mu_address_space_t *address_space, const mu_nodeid_t *node_id);
```

## Contract rules

- **Result identity**: for every input, returns the SAME node pointer (or NULL) as the
  current linear implementation. No observable change to Read/Browse/Translate.
- **Complexity**: O(log N) when indexed; O(N) fallback when `node_count >
  MU_MAX_ADDRESS_SPACE_NODES` (correctness preserved).
- **Index build**: performed once during `mu_address_space_validate` / server init;
  the index is stored in static/server storage, not the const node array.
- **Sort key**: deterministic over `(namespace_index, identifier_type, numeric value
  | string hash)`. A binary-search candidate MUST be confirmed with `mu_nodeid_equal`
  (handles hash collisions and string NodeIds exactly).
- **No heap / no mutation**: the const `nodes` array is never reordered or written.
- **Determinism**: duplicate NodeIds (already rejected by `mu_address_space_validate`)
  remain rejected; index build MUST NOT mask existing validation errors.

## Acceptance

- Property test: for a randomized address space, indexed lookup === linear lookup for
  all member and non-member NodeIds.
- A lookup-count probe shows comparisons grow ~log N (not N) as node count rises.
- Browse single-pass output is byte-identical to the two-pass output (SC-003).
- MonitoredItem sampling performs zero `mu_address_space_find_node` calls per tick
  (resolved-node cached — E3).
