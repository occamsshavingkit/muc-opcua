# Data Model: Nano Core Type System & Base Info CUs

## Entity: OPC UA Static Node

No new data structures. All CUs add entries to the existing static node table
in `src/address_space/base_nodes.c`.

### Per-node fields (existing `mu_node_t` structure)

| Field | Type | Description |
|-------|------|-------------|
| node_id | `mu_nodeid_t` | Numeric NodeId (ns=0;i=NNNN) from OPC-10000-5 |
| browse_name | `mu_qualified_name_t` | Standard BrowseName |
| display_name | `mu_localized_text_t` | Same as BrowseName |
| node_class | enum | `MU_NODECLASS_OBJECT`, `_VARIABLE`, `_OBJECTTYPE`, `_VARIABLETYPE`, `_DATATYPE`, `_REFERENCETYPE`, `_METHOD` |
| type_definition | `mu_nodeid_t` | Parent type NodeId |
| parent | `mu_reference_t` | Hierarchical parent reference (e.g., HasComponent from Objects) |
| references | `mu_reference_t[]` | Additional references (e.g., inverse references) |

### CU categories

**ReferenceType CUs** (2446, 3560):
- Add ReferenceType nodes under the References folder
- Subtype of NonHierarchicalReferences
- Inverse reference entry from References folder to the new ReferenceType

**DataType CUs** (2476, 5592):
- Add DataType node under the DataTypes folder
- Subtype of Structure (or BaseDataType for TimeZoneDataType which is a Structure)
- Encoding Object for structured types

**VariableType CUs** (3127, 2711):
- Add VariableType node under VariableTypes folder
- Subtype of BaseDataVariableType

**ObjectType CUs** (3560):
- Add ObjectType node (BaseInterfaceType) under ObjectTypes folder
- Subtype of BaseObjectType

**Property/Folder CUs** (2476, 3198, 4053, 5240, 2969, 2447, 5592):
- Add Object (Folder) or Variable (Property) as child of the parent node
- TypeDefinition: FolderType for folders, PropertyType for properties
- Properties on the Server Object: LocalTime, EstimatedReturnTime
- Properties on DataVariables: CurrencyUnit, ValueAsText
- Properties on ObjectTypes: DefaultInstanceBrowseName

**AccessLevelEx flag CUs** (2809, 2820, 4237):
- No new nodes. Document the NonatomicRead, NonatomicWrite, WriteFullArrayOnly
  NonVolatile, and Constant flags in AccessLevelEx.
- Add `MU_ACCESS_LEVEL_EX_*` bitmask constants to `include/muc_opcua/types.h`.
- Existing AccessLevelEx handling in `src/address_space/base_nodes.c` needs no
  change if flags are already storable in the attribute field.

**Documentation-only CUs** (3808, 3080, 3201, 5814):
- 3808: `docs/build-and-gating.md` already documents nano capacities — sufficient.
- 3080: Server already creates a default ApplicationInstanceCertificate on init.
- 3201: Document custom type system gating strategy.
- 5814: Document No Application Authentication configuration.
