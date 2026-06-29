# Research: NodeManagement Services

## OPC UA Normative Sections
- **Decision**: Target OPC 10000-4, Section 5.7 (NodeManagement Service Set) including `AddNodes` (5.7.2), `AddReferences` (5.7.3), `DeleteNodes` (5.7.4), and `DeleteReferences` (5.7.5).
- **Rationale**: The core services required to dynamically update an address space at runtime.
- **Alternatives considered**: None, as this is mandated by the OPC UA protocol.

## Zero-Heap Pre-Allocation for Dynamic Nodes
- **Decision**: Pre-allocate an array of dynamic nodes and references on server initialization, constrained by `MU_MAX_DYNAMIC_NODES` and `MU_MAX_DYNAMIC_REFERENCES`.
- **Rationale**: Meets the zero-heap requirement from the constitution. Allocating memory dynamically via `malloc` during `AddNodes` can cause heap fragmentation and out-of-memory errors on microcontrollers. Pre-allocation ensures deterministic memory usage.
- **Alternatives considered**: Using `malloc`/`free` was rejected due to strict constitutional constraints.

## NodeId Assignment
- **Decision**: For `AddNodes`, if the client requests a specific NodeId, use it. If the client leaves it null (namespace index 0, numeric id 0), the server auto-generates a unique numeric NodeId in a designated dynamic namespace (e.g., Namespace 1).
- **Rationale**: The specification says servers can assign NodeIds if none is provided.
- **Alternatives considered**: Forcing clients to assign NodeIds, but that limits interoperability with clients that expect the server to generate them.

## Deletion Cascading
- **Decision**: When `DeleteNodes` is called, automatically clean up any incoming or outgoing references associated with the deleted node from the dynamic reference pool. If `DeleteTargetReferences` is true, remove all target references.
- **Rationale**: Matches OPC UA 5.7.4 specification.
- **Alternatives considered**: Leaving dangling references, which violates the address space integrity.
