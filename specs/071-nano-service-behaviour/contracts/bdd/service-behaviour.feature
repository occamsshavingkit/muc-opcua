Feature: Server service behaviour conformance unit contracts

  Scenario: SCN-001 nano discovery and view claims
    Given a nano profile build uses manifest defaults
    When discovery and view service requests covered by CUs 2317, 2328, 2352, and 3530 are processed
    Then responses and operation results follow OPC-10000-4 sections 5.5.2, 5.5.4, 5.9.2, 5.9.3, and 5.9.4
    And non-nano-default write, session change-user, and diagnostics CUs are not claimed
    And a custom build disabling one dedicated Discovery or View CU disables only that CU behaviour

  Scenario: SCN-002 optional write and session claims
    Given a profile enables write or session change-user CUs
    When valid and invalid Write or ActivateSession change-user requests are processed
    Then enabled behaviour follows OPC-10000-4 sections 5.11.4 and 5.7.3
    And disabled or invalid requests return explicit OPC UA StatusCodes
    And rejected or disabled behavior preserves prior value, user identity, and nonce state
    And opc_cu_3147 IndexRange behavior remains unsupported and unclaimed under S071-D1

  Scenario: SCN-003 diagnostics claims
    Given diagnostics CUs are enabled by profile or custom configuration
    When service activity changes diagnostics counters or a request asks for returnDiagnostics
    Then address-space diagnostics and service-return diagnostics match the enabled claims
    And disabling one dedicated diagnostics CU makes only that CU behavior unavailable and unclaimed

  Scenario: SCN-004 malformed and boundary requests
    Given an in-scope service receives malformed arrays, excessive counts, invalid NodeIds, or truncated binary data
    When the service decodes and validates the request
    Then the service returns a cited OPC UA failure StatusCode without unsafe state changes
