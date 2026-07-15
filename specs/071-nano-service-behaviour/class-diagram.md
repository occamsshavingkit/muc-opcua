<!-- markdownlint-disable MD013 -->

# Internal Object Design

The feature uses the existing CU-aligned service modules and manifest generator. No new public object model is required.

```mermaid
classDiagram
    class ConformanceUnitClaim {
      id
      opc_display_name
      kconfig_symbol
      implementation_state
      profile_defaults
      backing_tests
    }
    class ProfileGate {
      profile
      cu_id
      default_enabled
      compile_out_expected
    }
    class ServiceBehaviorProof {
      service_family
      request_case
      expected_service_result
      expected_operation_result
      test_reference
    }
    class SessionUserIdentity {
      session_id
      current_user
      requested_user
      server_nonce
    }
    class DiagnosticsSurface {
      counter_source
      address_space_node
      return_diagnostics_mask
      diagnostic_info
    }

    ConformanceUnitClaim "1" --> "1..*" ServiceBehaviorProof : proven_by
    ConformanceUnitClaim "1" --> "1..*" ProfileGate : gated_by
    ServiceBehaviorProof --> SessionUserIdentity : session_cases
    ServiceBehaviorProof --> DiagnosticsSurface : diagnostics_cases
```

## Responsibilities

- `ConformanceUnitClaim`: keeps manifest state, symbol, profile defaults, tests, and normative evidence together.
- `ProfileGate`: prevents optional or micro+ CUs from becoming nano-default claims accidentally.
- `ServiceBehaviorProof`: connects service-level behaviour to tests and OPC UA StatusCode expectations.
- `SessionUserIdentity`: constrains ActivateSession change-user state transitions.
- `DiagnosticsSurface`: separates address-space diagnostics from service-return diagnostics.
