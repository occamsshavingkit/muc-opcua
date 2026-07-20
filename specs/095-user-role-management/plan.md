# Implementation Plan: User Role Management Server Facet

**Branch**: `095-user-role-management` | **Kconfig**: `MUC_OPCUA_CU_USER_ROLE_MANAGEMENT` (depends on `MUC_OPCUA_CU_METHOD_SERVER`, full-only default)

## Constitution Check

| Principle | Status |
|-----------|--------|
| I. Spec Fidelity | ✅ OPC-10000-12 §9.5-9.6, CU 2080 |
| II. Embedded-First | ✅ No heap, integrator-provided adapter |
| III-VII | ✅ All pass |

## Phases

1. Kconfig + adapter interface + CMake gating
2. Method handlers (AddRole, RemoveRole, AddIdentity, RemoveIdentity)
3. Address-space RoleSetType(15607) InstanceDeclarations
4. Manager adapter: alloc/free role nodes from dynamic node pool
5. Unit tests (8 cases)
6. Polish

## Size Budget
≤2 KB `.text`. Methods are thin wrappers calling the adapter.
