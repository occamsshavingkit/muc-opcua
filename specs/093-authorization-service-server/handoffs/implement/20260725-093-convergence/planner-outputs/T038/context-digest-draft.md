# Context Digest: Shard S01-test-validation-01 (Task T038)

## Assigned Task
- **Task ID**: T038
- **Task Text**: Add type-system tests in `tests/unit/test_type_system.c` proving NodeIds 17852-17855, their NodeClasses, DataTypes, PropertyType references, and Mandatory modelling rules are present only when `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` is enabled per OPC-10000-12 §9.7.4 Table 158 and spec.md FR-009/OPC-005

## Reference Document Headings (from context-index.json)
- **specs/093-authorization-service-server/spec.md**
  - User Scenarios & Testing
  - Requirements
  - Success Criteria
  - OPC UA Specification Requirements
- **specs/093-authorization-service-server/plan.md**
  - Technical Context
  - Constitution Check
  - Data Model
  - Validation Path
  - Task Sequence
  - Size Budget
- **specs/093-authorization-service-server/data-model.md**
  - Entities
  - Kconfig Symbols
  - File Map
- **specs/093-authorization-service-server/research.md**
  - JWT Library Decision
  - Key Configuration
  - Claims Processing
  - ActivateSession Integration
  - Kconfig Gating
  - Algorithm Support
- **specs/093-authorization-service-server/quickstart.md**
  - Build
  - Test
  - Size Measurement

## Technical Context & Node Specifications
The type-system InstanceDeclarations for `AuthorizationServiceConfigurationType` (OPC UA Part 12 §9.7.4 Table 158) are defined as follows:
- **NodeId 17852**: `AuthorizationServiceConfigurationType` (ObjectType)
  - **NodeClass**: `ObjectType`
  - **Modelling Rule**: None (Type definition)
- **NodeId 17853**: `ServiceUri`
  - **NodeClass**: `Variable` (Property)
  - **DataType**: `String`
  - **Modelling Rule**: `Mandatory`
  - **Reference**: `HasProperty` from 17852
- **NodeId 17854**: `ServiceCertificate`
  - **NodeClass**: `Variable` (Property)
  - **DataType**: `ByteString`
  - **Modelling Rule**: `Mandatory`
  - **Reference**: `HasProperty` from 17852
- **NodeId 17855**: `IssuerEndpointUrl`
  - **NodeClass**: `Variable` (Property)
  - **DataType**: `String`
  - **Modelling Rule**: `Mandatory`
  - **Reference**: `HasProperty` from 17852

### Gating Conditions
These nodes MUST only be visible/present in the address space if the Kconfig symbol `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` is defined and enabled. When undefined, querying these NodeIds from the type system must return that they do not exist or are not exposed.

## Validation Path & Test Requirements
The tests in `tests/unit/test_type_system.c` must verify both states:
1. **Gating Enabled**: With `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` defined, verify that NodeIds 17852-17855 exist and have the correct NodeClass, DataType, Property references, and Mandatory ModellingRules.
2. **Gating Disabled**: With `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` undefined, verify that NodeIds 17852-17855 are NOT present in the type system.

## Context Gaps
- None.
