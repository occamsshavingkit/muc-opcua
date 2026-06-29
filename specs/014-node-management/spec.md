# Feature Specification: NodeManagement Services

**Feature Branch**: `014-node-management`  
**Created**: 2026-06-29  
**Status**: Draft  
**Input**: User description: "Feature 014: NodeManagement Services Implement AddNodes, DeleteNodes, AddReferences, and DeleteReferences. Allow authorized clients to dynamically build the address space at runtime."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Add and Delete Nodes (Priority: P1)

Authorized clients need to add new instances (nodes) to the address space at runtime, and later remove them, to represent dynamic assets that connect and disconnect without requiring a firmware restart.

**Why this priority**: Node creation and deletion are the primary primitives for dynamic address spaces.

**Independent Test**: Can be fully tested by a client sending an `AddNodesRequest` to create a node, verifying it appears in Browse/Read, and then sending a `DeleteNodesRequest` to remove it.

**Acceptance Scenarios**:

1. **Given** a running server, **When** an authorized client sends a valid `AddNodesRequest`, **Then** the server creates the node, assigns a NodeId (if requested), and returns `Good`.
2. **Given** an existing dynamic node, **When** an authorized client sends a valid `DeleteNodesRequest`, **Then** the server deletes the node and its target references, and returns `Good`.
3. **Given** an unauthorized client, **When** they send an `AddNodesRequest`, **Then** the server rejects it with `Bad_UserAccessDenied`.

---

### User Story 2 - Add and Delete References (Priority: P2)

Authorized clients need to create new relationships (references) between existing nodes at runtime, and later remove them, to model dynamic associations (like plugging a sensor into a port).

**Why this priority**: References provide the structural context for nodes. Without them, new nodes are orphaned.

**Independent Test**: Can be fully tested by creating a reference between two nodes using `AddReferencesRequest` and observing the new relationship in a `BrowseResponse`, then removing it with `DeleteReferencesRequest`.

**Acceptance Scenarios**:

1. **Given** two existing nodes, **When** a client sends a valid `AddReferencesRequest`, **Then** the server creates a directional reference between them and returns `Good`.
2. **Given** an existing reference, **When** a client sends a valid `DeleteReferencesRequest`, **Then** the server removes the reference and returns `Good`.

### Edge Cases

- What happens when an `AddNodesRequest` tries to create a node with a `NodeId` that already exists? -> Returns `Bad_NodeIdExists`.
- What happens when a client tries to delete a static (ROM/const) node? -> Returns `Bad_UserAccessDenied` or `Bad_NotSupported`.
- What happens when memory is exhausted while adding a node or reference? -> Returns `Bad_OutOfMemory`.
- How does the system handle invalid `NodeClass` or `ReferenceTypeId` in Add requests? -> Returns appropriate Bad status code.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST process `AddNodesRequest` and create dynamic nodes in RAM if space permits.
- **FR-002**: System MUST process `DeleteNodesRequest` and remove dynamic nodes from RAM, including all references targeting them.
- **FR-003**: System MUST process `AddReferencesRequest` to create relationships between nodes.
- **FR-004**: System MUST process `DeleteReferencesRequest` to remove specific relationships.
- **FR-005**: System MUST validate user authorization before allowing modifications to the address space.
- **FR-006**: System MUST prevent modification or deletion of static (ROM-based) nodes.
- **FR-007**: System MUST track dynamic memory usage for nodes/references and return `Bad_OutOfMemory` when exhausted.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA profile is the NodeManagement Service Set (OPC 10000-4, Section 5.7).
- **OPC-002**: Implemented services are AddNodes (5.7.2), AddReferences (5.7.3), DeleteNodes (5.7.4), and DeleteReferences (5.7.5).
- **OPC-003**: Unsupported features in these requests (e.g. unsupported node classes) return `Bad_NotSupported` or `Bad_NodeClassInvalid`.
- **OPC-004**: SecurityPolicy and conformance status are profile-targeting (Micro/Embedded profiles).

### Scope Boundaries *(mandatory)*

- **In Scope**: Dynamic node creation/deletion, reference creation/deletion, authorization checks for these services, memory limits for dynamic address spaces.
- **Out of Scope**: Persistent storage of dynamic nodes across reboots (they reside in RAM only for now).
- **Compatibility Claim**: Claiming support for the NodeManagement Service Set.
- **Application Headroom Goal**: The RAM overhead for dynamic nodes and references must be strictly bounded and pre-allocated to avoid heap fragmentation (Zero-Heap principle).

### Key Entities *(include if feature involves data)*

- **Dynamic Node**: A node structure allocated in RAM rather than ROM.
- **Dynamic Reference**: A reference structure allocated in RAM, linking nodes.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Clients can successfully add and delete up to a configurable maximum number of nodes and references at runtime.
- **SC-002**: `Browse` and `Read` services correctly interact with dynamically added nodes without modification.
- **SC-003**: Attempting to exceed the pre-allocated dynamic node/reference capacity gracefully returns `Bad_OutOfMemory` without crashing.
- **SC-004**: Static address space remains immutable; attempts to delete ROM nodes fail correctly.
- **SC-005**: Flash overhead for the NodeManagement Service implementations is minimized (target < 5kB).

## Assumptions

- Dynamic nodes do not need to persist across server restarts (ephemeral).
- A flat pre-allocated array of dynamic nodes and references will be used to adhere to the zero-heap policy.
- Role-based access control or authorization is minimal or uses a global admin role flag for NodeManagement operations.
