# Feature Specification: User Role Management Server Facet

**Created**: 2026-07-20 | **Status**: Draft
**Input**: PG19 — User Role Management Server Facet (CU 2080, OPC-10000-7 v1.05.02). Server-side CRUD for user roles. Enables creating, removing, and managing OPC UA Roles and their identity-to-role mappings.

## User Scenarios

### US1 — Role CRUD via Address Space Methods (P1)
A security administrator calls AddRole via the RoleSet's standard OPC UA Methods. The server creates a new Role node with the given name and NodeId. RemoveRole deletes it.

**Independent Test**: AddRole → verify Role node appears in address space. RemoveRole → verify node is gone.

1. **Given** a RoleSet instance with a configured role adapter, **When** AddRole is called with a valid name, **Then** the Role is created in the address space and returns Good.
2. **Given** an existing Role, **When** RemoveRole is called, **Then** the Role is removed and returns Good.

### US2 — Identity-to-Role Assignment (P2)
An administrator adds or removes identity mappings (e.g., "operator1" → "Operator" role).

**Independent Test**: AddIdentity → verify the role's identities list includes the new mapping.

1. **Given** an existing Role, **When** AddIdentity is called with a user identity, **Then** the identity is added to the Role.
2. **Given** an identity on a Role, **When** RemoveIdentity is called, **Then** the identity is removed.

### Edge Cases
- AddRole with duplicate name → Bad_DuplicateName
- RemoveRole on non-existent role → Bad_NoEntryExists
- Methods called without adapter → Bad_NotSupported
- Methods called on non-RoleSet object → Bad_MethodInvalid

## Requirements

- **FR-001**: Server MUST implement User Role Management Server Facet per OPC-10000-12 §9.5-9.6, gated on `MUC_OPCUA_CU_USER_ROLE_MANAGEMENT`.
- **FR-002**: RoleSetType(15607) InstanceDeclarations with AddRole(15997)/RemoveRole(16000) Methods on the type and AddIdentity(15612)/RemoveIdentity(15614) on the Role placeholder.
- **FR-003**: Role storage MUST be integrator-provided via `mu_role_management_adapter_t`. Missing adapter → Bad_NotSupported.
- **FR-004**: Methods auto-registered via existing Method Server dispatch (CU METHOD_SERVER dependency).
- **FR-005**: Type nodes gated on `MUC_OPCUA_CU_USER_ROLE_MANAGEMENT && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`.

## OPC UA Scope
- OPC-10000-12 §9.5-9.6, OPC-10000-7 CU 2080
- RoleSetType (15607), AddRole (15997), RemoveRole (16000), AddIdentity (15612), RemoveIdentity (15614)
- StatusCodes per OPC-10000-4 §7.38.2

## Scope Boundaries
- **In**: AddRole, RemoveRole, AddIdentity, RemoveIdentity Methods. Type-system nodes. Adapter interface.
- **Out**: Permission-based access control enforcement. Role membership evaluation at browse/read time. Full GDS role synchronization.
- **Size**: ≤2 KB `.text`.

## Success Criteria
- SC-001: AddRole creates a Role node visible in the address space.
- SC-002: RemoveRole deletes a Role node.
- SC-003: Missing adapter → Bad_NotSupported for all methods.
- SC-004: Code compiles out when CU undefined.
- SC-005: Type nodes present when CU + TYPE_INFORMATION enabled.
