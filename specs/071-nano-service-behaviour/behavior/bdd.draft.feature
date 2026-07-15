Feature: Server service behaviour conformance unit claims

  Scenario: Nano profile claims mandatory discovery and view CUs only when backed by behaviour
    Given the server is configured for the nano profile
    And the in-scope discovery and view CUs are nano-default CUs in the manifest
    When clients invoke FindServers, GetEndpoints, Browse, BrowseNext, and TranslateBrowsePathsToNodeIds within documented limits
    Then the server returns successful service responses and operation results matching the cited OPC UA sections
    And optional write, session change-user, and diagnostics CUs are not claimed by nano defaults
    And disabling one dedicated discovery or view CU in a custom profile disables only that CU behaviour

  Scenario: Optional write and session CUs are enabled only by matching profile gates
    Given a micro-or-higher or custom profile enables the relevant write and session CUs
    When clients write values, StatusCodes, timestamps, or change Session user identity
    Then valid requests succeed according to the enabled CU support
    And unsupported, invalid, or disabled cases return explicit OPC UA StatusCodes
    And disabling one dedicated CU disables only that CU behaviour and preserves prior state
    And IndexRange writes remain explicitly unsupported and unclaimed under follow-up S071-D1

  Scenario: Diagnostics CUs expose observable diagnostic information
    Given diagnostics CUs are enabled
    When services create, close, reject, or time out sessions or requests
    Then ServerDiagnostics address-space values reflect live counters
    And supported returnDiagnostics requests include available diagnostic information
    And disabling one dedicated diagnostics CU disables only that CU surface and claim

  Scenario: Malformed and boundary requests fail safely
    Given an enabled in-scope service receives malformed arrays, excessive counts, invalid NodeIds, or truncated data
    When the service decodes or validates the request
    Then the service returns a cited OPC UA failure StatusCode
    And no partial conformance claim is recorded for unimplemented behaviour
