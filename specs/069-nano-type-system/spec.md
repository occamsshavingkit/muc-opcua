# Spec 005 — Server: Nano Core Type System & Base Info CUs

**Source**: Roadmap spec 005. Input: `profiles/opcua-profile-manifest.yaml` + OPC API data.

## Goal

Add the ~16 genuinely new nano-profile type system and Base Info
conformance unit nodes to the server's static address space. 4 are mandatory for
nano (enabled by defconfig), 12+ are optional (off by default, user opt-in).
Each CU requires
new node entries in `src/address_space/base_nodes.c` with correct OPC UA
NodeIds, BrowseNames, type references, and inverse references.

## CUs in scope

### Mandatory for nano (must be enabled in nano defconfig)

| CU ID | Name | What to add |
|-------|------|-------------|
| 2809 | Address Space Atomicity | NonatomicRead / NonatomicWrite flags on AccessLevelEx; document in code |
| 2820 | Address Space Full Array Only | WriteFullArrayOnly flag support; document in code |
| 3808 | Documentation - Core Capacities | (documentation-only; code for docs/build-and-gating.md already exists) |
| 3080 | Security Default ApplicationInstance Certificate | (deployment/doc; server already creates default cert) |

### Optional for nano (code exists, off by default)

| CU ID | Name | What to add |
|-------|------|-------------|
| 2446 | Address Space AddIn Reference | HasAddIn ReferenceType node (ns=0;i=17604) under References |
| 2447 | AddIn DefaultInstanceBrowsename | DefaultInstanceBrowseName Property node |
| 2476 | Base Info LocalTime | LocalTime Variable on Server object (ns=0;i=...); TimeZoneDataType node |
| 2711 | Base Info Selection List | SelectionListType VariableType nodes |
| 2809 | (covered above) | |
| 2820 | (covered above) | |
| 2969 | Base Info ValueAsText | ValueAsText Property on enumeration Variables |
| 3127 | Base Info OptionSet | OptionSetType VariableType node (ns=0;i=11487) |
| 3198 | Base Info Estimated Return Time | EstimatedReturnTime Property on Server object (ns=0;i=...); |
| 3560 | Address Space Interfaces | InterfaceTypes folder, BaseInterfaceType, HasInterface ReferenceType |
| 3808 | (covered above) | |
| 4053 | Base Info Locations Object | Locations folder under Objects (ns=0;i=...) |
| 4237 | Address Space NonVolatile and Constant | Flag documentation in AccessLevelEx |
| 5240 | Base Info Currency | CurrencyUnit Property on DataVariables representing currency |
| 5592 | Base Info Engineering Units | EUInformation / EngineeringUnits Property support |
| 3201 | Base Info Custom Type System | (optional — gating strategy for custom types; documentation) |

## Acceptance criteria

1. Each CU's NodeId, BrowseName, and type references match OPC-10000-3/OPC-10000-5.
2. New nodes are gated behind `#ifdef MUC_OPCUA_CU_*` guards so unused CUs compile out.
3. `MUC_OPCUA_CU_*` symbols are added to Kconfig/defconfig with correct optional defaults.
4. Existing tests pass: `profile-tests nano`, `interop`, `freestanding-core`.
5. New `cu_optional` fields in the manifest govern Kconfig defaults (optional CUs: default n; mandatory: default y if nano cascade).
