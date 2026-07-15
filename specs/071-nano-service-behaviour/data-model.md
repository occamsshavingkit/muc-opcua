<!-- markdownlint-disable MD013 MD060 -->

# Data Model: Server Nano Core Service Behaviour CUs

## ConformanceUnitClaim

Represents one in-scope OPC UA conformance-unit claim in the manifest and generated artifacts.

| Field | Description | Validation Rules |
|-------|-------------|------------------|
| `id` | Manifest id such as `opc_cu_2317` | Must match an in-scope CU id from `spec.md` |
| `opc_display_name` | OPC Foundation CU name | Must match source manifest/API naming |
| `kconfig_symbol` | Dedicated `MUC_OPCUA_CU_*` symbol | Required before `implementation_state` can become implemented |
| `implementation_state` | Claim state | Cannot become implemented without backing tests |
| `profile_defaults` | Profile enablement map | Must preserve manifest defaults unless a separate roadmap/spec changes them |
| `backing_tests` | Tests proving the claim | At least one required for each claimed CU |
| `opc_reference` | Normative section or source evidence | Must include CU id and relevant OPC-10000 sections in planning/tasks |

## ServiceBehaviorProof

Represents an observable service behaviour needed to support a CU claim.

| Field | Description | Validation Rules |
|-------|-------------|------------------|
| `service_family` | Discovery, View, Write, Session, or Diagnostics | Must map to at least one in-scope CU |
| `request_case` | Valid, malformed, unsupported, disabled, boundary, or state-conflict case | Must map to a Case ID from `behavior-testability.md` |
| `expected_service_result` | Service-level StatusCode | Must cite OPC-10000-4 where protocol-visible |
| `expected_operation_result` | Per-operation StatusCode when applicable | Required for Write and TranslateBrowsePaths cases |
| `encoded_response_shape` | Response type and required arrays/diagnostics | Required for wire-visible fixture tests |
| `test_reference` | Unit/integration/fixture test path | Required before claim completion |

## ProfileGate

Represents how an in-scope CU is enabled or disabled by profile/default settings.

| Field | Description | Validation Rules |
|-------|-------------|------------------|
| `profile` | nano, micro, embedded, standard, full, or custom | Must match manifest profile names |
| `cu_id` | In-scope CU id | Must match `ConformanceUnitClaim.id` |
| `default_enabled` | Manifest default for the profile | Must not be changed for nano without explicit roadmap/spec update |
| `compile_out_expected` | Whether code must compile out when disabled | Required for optional and micro+ CUs |

## SessionUserIdentity

Represents the user identity associated with an active Session.

| Field | Description | Validation Rules |
|-------|-------------|------------------|
| `session_id` | Session being activated or re-activated | Must refer to a valid Session for change-user success |
| `current_user` | Identity before ActivateSession change-user call | Required for state-conflict tests |
| `requested_user` | Identity from the new ActivateSession request | Must be valid for success or rejected with cited StatusCode |
| `server_nonce` | Nonce returned after activation | Must change on successful activation per OPC-10000-4 section 5.7.3 |

## DiagnosticsSurface

Represents diagnostics exposed through address-space nodes and response diagnostics.

| Field | Description | Validation Rules |
|-------|-------------|------------------|
| `counter_source` | Server-owned live diagnostics counters | Must remain bounded and server-owned |
| `address_space_node` | ServerDiagnostics node such as `ServerDiagnosticsSummary` or `EnabledFlag` | Required for Base Info Diagnostics claim |
| `return_diagnostics_mask` | RequestHeader diagnostics flags | Required for Base Services Diagnostics claim |
| `diagnostic_info` | Returned diagnostics payload | Must be available, empty, or unsupported explicitly; no fabricated data |
