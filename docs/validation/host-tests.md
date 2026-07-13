# Host Tests Validation

This file contains historical host-test evidence plus the active required-gate
status for `020-audit-hardening`. The legacy summary immediately below is not a
full-feature pass claim for `020-audit-hardening`; detailed host-test evidence
for that feature is recorded separately by T085b.

- Total elapsed time: < 15 minutes.
- Tests executed: 40 tests passed out of 40.
- Target SC-001 met: Yes.

## Feature 005 US1

- Default host suite: `ctest --test-dir build/test --output-on-failure`
  - Result: 58 tests passed out of 58.
- Standard DataChange host suite:
  `ctest --test-dir build/us1-standard --output-on-failure`
  - Build flags: `MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON`,
    `MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
    `MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`.
  - Result: 58 tests passed out of 58.

## Feature 005 US2

- Default type-system regression:
  `cmake --build build/test --target test_type_system && ctest --test-dir build/test --output-on-failure -R test_type_system`
  - Result: 1 test passed out of 1.
- Base Type System focused suite:
  `cmake --build build/us2-types --target test_type_system && ctest --test-dir build/us2-types --output-on-failure -R test_type_system`
  - Build flag: `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`.
  - Result: 1 test passed out of 1.
- Default host suite: `ctest --test-dir build/test --output-on-failure`
  - Result: 59 tests passed out of 59.
- Base Type System host suite:
  `ctest --test-dir build/us2-types --output-on-failure`
  - Build flag: `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`.
  - Result: 59 tests passed out of 59.

## Feature 005 US3

- Default Call gated-placeholder tests:
  `ctest --test-dir build/test --output-on-failure -R 'test_method_call'`
  - Result: 2 tests passed out of 2.
- Embedded-capacity Call focused suite:
  `ctest --test-dir build/us3-call --output-on-failure -R 'test_method_call'`
  - Build flags: `MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON`,
    `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`, `MU_MAX_MONITORED_ITEMS=100`,
    `MU_MAX_SUBSCRIPTIONS=2`, `MU_MAX_PUBLISH_REQUESTS=5`,
    `MU_MONITORED_QUEUE_DEPTH=2`, `MU_MAX_TRIGGER_LINKS=4`.
  - Result: 2 tests passed out of 2.
- Default host suite: `ctest --test-dir build/test --output-on-failure`
  - Result: 61 tests passed out of 61.
- Embedded-capacity host suite:
  `ctest --test-dir build/us3-call --output-on-failure`
  - Result: 61 tests passed out of 61.

## Feature 005 US4 / Polish

- Host unit + integration suite:
  `make test`
  - Result: 61 tests passed out of 61.
- Profile Makefile builds:
  `make nano`, `make micro`, `make embedded`
  - Result: all built `examples/minimal_server` successfully.
- Embedded profile Makefile flags:
  `make embedded`
  - Confirmed: `MUC_OPCUA_PROFILE=embedded`,
    `MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
    `MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`,
    `MU_MAX_TRIGGER_LINKS=4`.

## Feature 020 Audit Hardening Required Gate

- Required-gate status: host-test evidence PASS for the blocker-remediation
  refresh; the full release gate still depends on companion ledgers such as
  formatting/static-analysis and latency.
- Conformance status: profile-targeting only; no external CTT evidence is
  recorded here.
- Scope: FR-020 and SC-006 require release validation to fail closed until the
  required gates detect regressions in malformed-input handling, exact
  StatusCodes, conformance documentation, memory-safety behavior,
  formatting/static-analysis, and embedded build readiness.
- Host-test gate: required for malformed-input and exact-StatusCode regression
  coverage, including StatusCodes cited by OPC-10000-4 section 7.38.2. T085b
  records the command output, counts, failures, duration, and log paths.
- Companion required gates: conformance documentation remains governed by
  OPC-10000-7 sections 4.2 and 4.3; memory-safety, formatting/static-analysis,
  and embedded readiness are tracked in their validation ledgers before release
  validation can succeed.
- Current outcome: the host CTest and conformance-doc blocker rows are cleared
  by the 2026-06-30 remediation refresh. Companion required gates remain tracked
  in their own ledgers.

### T085b Host-Test Evidence: 2026-06-30

- Host build:
  `cmake --build build/test`
  - Result: PASS, exit code `0`.
- Focused US1-US3 host CTest group:
  `ctest --test-dir build/test -R '^(test_discovery_services|test_discovery_endpoint|test_dispatch_services|test_session|test_security_identity_errors|test_read_service|test_write_errors|test_write_decoder|test_write_service_integration|test_browse_service|test_browse_limits|test_subscriptions_errors|test_subscriptions|test_query_service|test_query_encoding|test_history|test_secure_channel|test_secure_handshake_modern|test_connection_multiplex|test_single_client_limit|test_user_auth_plaintext|test_aggregate|test_node_management_errors|test_node_management|test_service_dispatch)$' --output-on-failure`
  - Result: PASS, exit code `0`; 25/25 CTest targets passed, 0 failed, total
    test time `0.53 sec`.
  - Log path: `build/test/Testing/Temporary/LastTest.log` records the latest
    CTest run; CTest overwrites this path on subsequent runs.
  - Detailed US1-US3 focused PASS rows remain in
    `docs/validation/audit-hardening.md`, the authoritative feature host-test
    ledger for this task.
- Write-service integration rerun:
  `ctest --test-dir build/test -R '^test_write_service_integration$' --output-on-failure`
  - Result: PASS, exit code `0`; 1/1 CTest target passed, 0 failed, total
    test time `0.00 sec`.
  - Resolved note: the previous write integration failure is no longer current
    after the stale hardcoded authentication-token fixtures were fixed in test
    support/integration files.
- Conformance documentation subgroup:
  `ctest --test-dir build/test -R '^test_(conformance_docs|traceability_docs)$' --output-on-failure`
  - Historical T085b result: FAIL, exit code `8`; `test_traceability_docs`
    passed and `test_conformance_docs` failed only on the former
    `fuzz_binary_types` placeholder/input-discard blocker.
  - T102b remediation refresh: after replacing `fuzz_binary_types` with an
    input-consuming harness, `cmake --build build/test --target
    test_conformance_docs` and `ctest --test-dir build/test -R
    '^test_conformance_docs$' --output-on-failure` passed. The broader
    conformance-doc subgroup should remain the standard command when both
    `test_conformance_docs` and `test_traceability_docs` need a fresh combined
    transcript.
- Host/conformance blocker outcome: PASS for the host rows covered here. The
  full `020-audit-hardening` release gate still depends on companion ledgers
  that are outside this host-test file.
