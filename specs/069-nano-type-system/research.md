# Research: Nano Core Type System & Base Info CUs

## Decision 1: NodeId sourcing strategy

**Decision**: Use OPC-10000-5 to look up each CU's required NodeId(s). When
an instance-declaration NodeId is needed (e.g. Server.LocalTime), use the
well-known Property NodeId from OPC-10000-5 §5 (AddressSpace NodeIds).

**Rationale**: OPC-10000-5 is the canonical NodeId registry. Each node we add
must use the exact OPC-defined numeric NodeId in namespace 0.

**Alternatives considered**:
- NodeIds.csv: outdated, not maintained in this repo
- Guessing from consecutive ranges: unsafe, would create non-conformant nodes

## Decision 2: Kconfig symbol naming

**Decision**: Use `MUC_OPCUA_CU_<name>` names derived from the OPC CU display
name, converted to uppercase with underscores. Example: Address Space AddIn
Reference → `MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE`.

**Rationale**: Consistent with existing `MUC_OPCUA_CU_*` convention established
in the manifest and build-and-gating docs.

## Decision 3: Gate placement

**Decision**: Place all new nodes inside the existing
`#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER` block in `base_nodes.c`, with
additional per-CU `#ifdef MUC_OPCUA_CU_*` inner guards.

**Rationale**: Core 2022 Server Facet is the parent facet; individual CU gates
let users drop CUs they don't need even within a named profile.

## Decision 4: Optional vs mandatory default strategy

**Decision**: Per OPC API `isOptional` data:
- `cu_optional: false` → `default y if MUC_OPCUA_INTERN_PROFILE_NANO`
- `cu_optional: true` → `default n` (user must explicitly enable)

**Rationale**: Matches OPC spec. Optional CUs should not bloat nano for
constrained devices.

## Decision 5: NodeId lookup priority

**Decision**: For each CU, look up NodeIds in this order:
1. `opc-ua-reference` MCP tool (search_nodes with OPC-10000-5 prefix)
2. Existing `opcua_ids.h` constants
3. OPC-10000-5 spec sections for the relevant ObjectType/VariableType
4. OPC Foundation NodeSets2 XML schema as fallback

**Rationale**: Fastest path is MCP query. Fallbacks ensure we always find
the correct NodeId before adding a node.

## Known NodeIds

| OPC CU | Display Name | NodeId |
|--------|-------------|--------|
| 2446 | Address Space AddIn Reference | HasAddIn ReferenceType = i=17604 |
| 3560 | Address Space Interfaces | BaseInterfaceType = i=17602 |
| 3560 | Address Space Interfaces | HasInterface ReferenceType = i=17603 |
| 3127 | Base Info OptionSet | OptionSetType VariableType = i=11487 |
| 2711 | Base Info Selection List | SelectionListType VariableType = i=16309 |
| 2476 | Base Info LocalTime | TimeZoneDataType = i=8912 |
| 2476 | Base Info LocalTime | Server.LocalTime Property = OPC-10000-5 §6.3.1, ServerType InstanceDeclaration |
| 3198 | Base Info Estimated Return Time | Server.EstimatedReturnTime Property = OPC-10000-5 §6.3.1, ServerType InstanceDeclaration |
| 4053 | Base Info Locations Object | Locations Folder = OPC-10000-5 §5.3.2 (Objects → Locations) |
| 5240 | Base Info Currency | CurrencyUnit Property = OPC-10000-5 §12.2.12.2 |
| 2447 | AddIn DefaultInstanceBrowsename | DefaultInstanceBrowseName = OPC-10000-5 §/ObjectType definitions |
| 2969 | Base Info ValueAsText | ValueAsText Property = OPC-10000-5 §7.x or §6.3.1 |
| 3201 | Base Info Custom Type System | documentation-only, no NodeId needed |
| 2809 | Address Space Atomicity | AccessLevelEx flags only, no NodeId |
| 2820 | Address Space Full Array Only | AccessLevelEx flags only, no NodeId |
| 4237 | Address Space NonVolatile and Constant | AccessLevelEx flags, no NodeId |
| 3808 | Documentation Core Capacities | doc-only, already exists in docs/build-and-gating.md |
| 3080 | Security Default AppInstance Cert | deployment/doc, no code change |
| 5592 | Base Info Engineering Units | EUInformation DataType = i=887 (OPC-10000-5 §12.2.12.1); EngineeringUnits Property = look up in §6.3.1 or §12.2.12.1 |
| 5814 | Security No App Authentication | doc/configuration, no code change |

**Note on Server Property NodeIds**: All Server Object Properties (LocalTime,
EstimatedReturnTime, CurrencyUnit, etc.) are instance declarations defined in the
ServerType (OPC-10000-5 §6.3.1). Their exact numeric NodeIds are assigned in the
OPC UA NodeIds specification §5 (AddressSpace NodeIds). When implementing, use
the OPC-10000-5 normative reference to look up each Property's exact NodeId.
If a NodeId cannot be found in the spec, these Properties may be defined
conceptually (the type and BrowseName are sufficient for the CU) without a
numeric instance NodeId.
