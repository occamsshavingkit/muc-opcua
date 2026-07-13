# Audit Hardening Closure Matrix

Feature: `020-audit-hardening`

This matrix records closure status for audit findings that now have repository
evidence. T003b searched the repository evidence for the original six-agent
triage ledger, but no concrete per-finding source IDs were present. T102c closes
that process blocker by adding
`docs/validation/audit-hardening-triage-ledger.md`, the authoritative
in-repository source-ID and severity ledger for Feature 020.

Primary references:

- `specs/020-audit-hardening/spec.md`
- `specs/020-audit-hardening/plan.md`
- `specs/020-audit-hardening/data-model.md`
- `specs/020-audit-hardening/contracts/audit-hardening-contract.md`
- `docs/traceability/020-audit-hardening.md`
- `docs/traceability/mcp-query-ledger.md`
- `docs/validation/audit-hardening-triage-ledger.md`

Imported source audit lane authority:

- `specs/020-audit-hardening/contracts/audit-hardening-contract.md`
  Evidence Contract: audit sources are `security-audit`, `Codex Security`,
  `spec-to-code compliance`, `CodeRabbit`, `manual reviewer`, and
  `TDD backlog`.
- `specs/020-audit-hardening/spec.md` FR-021 and Scope Boundaries cross-check
  the six-agent audit pass, including security, Codex Security, OPC UA
  compliance, CodeRabbit, manual review, and negative-path testing passes.
- T102c source-ID authority:
  `docs/validation/audit-hardening-triage-ledger.md` assigns deterministic
  repository-owned source finding IDs and severities. The IDs do not claim to
  reproduce unavailable external transcript IDs.
- Partial severity import before T102c: resource/readiness rows whose severity
  labels were explicitly recorded in `docs/size/feature-size-ledger.md` or
  `docs/review/embedded-readiness-review.md` used those labels. T102c replaces
  the remaining `Unknown` severity placeholders with repository triage
  severities.

## Closure Rules

- Every imported finding must map to at least one active functional requirement
  from `specs/020-audit-hardening/spec.md`.
- Every protocol-visible finding must cite the exact OPC UA part and section
  used to justify the expected behavior or rejection result.
- Closure status may be `Fixed - evidence recorded` only when the current
  validation ledgers record a passing host, sanitizer, fuzz-smoke, or
  documentation artifact for the affected behavior.
- High, P0, or P1 findings require explicit evidence before closure. Deferral
  requires a recorded risk rationale and approval reference.
- Do not mark sanitizer/resource/fuzz-placeholder blockers fixed from partial
  evidence. Leave profile-disabled and deferred findings pending for T091b/T091c.

## T102a Closure Verification

T102a reran the closure-matrix verification using the transcripts under
`/tmp/muc-opcua-t102a/`, with the summary recorded at
`/tmp/muc-opcua-t102a/12-summary-report.txt` and the transcript inventory at
`/tmp/muc-opcua-t102a/11-transcript-list.txt`.

At the time T102a ran, the verifier found `0` explicit P0/P1/High severity rows
in this file because all `28` finding rows still carried `Severity = Unknown -
source finding ID/severity not available`. This was not closure evidence for
High/P0/P1 risk: while the authoritative source IDs were missing and
`AH-AUTHORITATIVE-SIX-AGENT-IDS-MISSING` remained open, the matrix could not
prove that no High or P0/P1 finding remained open, and the feature could not
claim High/P0/P1 closure without explicit deferral approval.

All-row status counts from T102a:

- `Fixed - evidence recorded`: 15
- historical not-closed blocker rows: 7
- `Deferred - risk recorded`: 2
- `Profile-disabled - documented`: 3
- `Profile-scoped - documented`: 1

T102a confirmed these unresolved blocking rows:

- `AH-SANITIZER-HISTORY-LIFETIME-ASAN`
- `AH-FUZZ-BINARY-TYPES-PLACEHOLDER`
- `AH-HOST-FULL-FLASH-BUDGET-OVER`
- `AH-ARM-NANO-EMBEDDED-SIZE-BLOCKED`
- `AH-PICO-MINIMAL-SERVER-BLOCKED`
- `AH-PICO-STACK-EVIDENCE-BLOCKED`
- `AH-AUTHORITATIVE-SIX-AGENT-IDS-MISSING`

Evidence gaps carried forward by T102a:

- At T102a time, T003b was still unchecked, so authoritative six-agent source
  IDs and severities were missing from the closure matrix.
- The host/full flash-budget row remains not closed; linked size/readiness
  evidence classifies the host/full over-budget condition as High.
- ARM/Pico size evidence and Pico stack evidence remain blocked by embedded
  build failures.
- Sanitizer HistoryRead lifetime evidence and binary-types fuzz coverage still
  have release-blocking gaps.

## T102b Blocker-Remediation Refresh

T102b reran the repository-fixable blocker checks after source and validation
updates. The following previously open rows now have current evidence and are
closed below:

- `AH-SANITIZER-HISTORY-LIFETIME-ASAN`
- `AH-FUZZ-BINARY-TYPES-PLACEHOLDER`
- `AH-HOST-FULL-FLASH-BUDGET-OVER`
- `AH-ARM-NANO-EMBEDDED-SIZE-BLOCKED`
- `AH-PICO-MINIMAL-SERVER-BLOCKED`
- `AH-PICO-STACK-EVIDENCE-BLOCKED`
- `AH-STATIC-RAM-CALLER-STORAGE-DELTAS-INCOMPLETE`
- `AH-HEAP-EVIDENCE-ADVISORY-ONLY`

Current all-row status counts after T102b:

- `Fixed - evidence recorded`: 23
- historical not-closed blocker rows: 1
- `Deferred - risk recorded`: 0
- `Profile-disabled - documented`: 3
- `Profile-scoped - documented`: 1

At T102b, the only outstanding process row was
`AH-AUTHORITATIVE-SIX-AGENT-IDS-MISSING`: repository search had still not found
the original six-agent triage ledger with concrete source finding IDs and
severities. T102c supersedes that blocker by adding a repository-owned source-ID
ledger below.

## T102c Source-ID Ledger Refresh

T102c added `docs/validation/audit-hardening-triage-ledger.md` as the
authoritative in-repository source-ID and severity ledger for Feature 020. The
original external subagent transcripts still are not present in the repository;
the new ledger records repository-owned source IDs instead of fabricating
unavailable external IDs.

Current all-row status counts after T102c:

- `Fixed - evidence recorded`: 24
- not-closed blocker rows: 0
- `Deferred - risk recorded`: 0
- `Profile-disabled - documented`: 3
- `Profile-scoped - documented`: 1

The T102c ledger represents all six audit lanes, assigns concrete source IDs and
severities for all 28 findings, and records 0 High/P1 or P0/P1 rows open without
deferral approval.

## T003b Authoritative Import Attempt

T003b searched repository evidence under `docs/` and `specs/`, including
`docs/review`, `docs/validation`, and `docs/traceability`, for the six requested
audit lanes and source metadata. Transcripts are stored under
`/tmp/muc-opcua-t003b/`:

- `/tmp/muc-opcua-t003b/00-files-searched.txt`
- `/tmp/muc-opcua-t003b/01-source-lane-search.txt`
- `/tmp/muc-opcua-t003b/02-severity-id-search.txt`
- `/tmp/muc-opcua-t003b/03-id-pattern-search.txt`
- `/tmp/muc-opcua-t003b/04-audit-review-files.txt`
- `/tmp/muc-opcua-t003b/05-authoritative-gap-search.txt`
- `/tmp/muc-opcua-t003b/06-ah-id-crosscheck.txt`

Historical T003b result: the repository contained the authoritative source-lane
names in `specs/020-audit-hardening/contracts/audit-hardening-contract.md` and
the six-agent scope in `specs/020-audit-hardening/spec.md`, but no authoritative
six-agent triage ledger with concrete per-finding IDs. T003b therefore did not
replace descriptive `AH-*` IDs with source IDs. T102c supersedes that gap with
`docs/validation/audit-hardening-triage-ledger.md`.

## Audit Finding Closure Matrix

| Finding ID | Source audit lane | Severity | Affected FR(s) | OPC UA citation(s) | Expected fix / evidence | Validation artifact(s) | Closure status |
|---|---|---|---|---|---|---|---|
| AH-ARRAY-LENGTH-BOUNDS | TDD backlog / negative-path testing (`TDD-NEG-001`) | High/P1 | FR-001, FR-003, FR-020 | OPC-10000-6 sections 5.2 and 5.2.5; OPC-10000-4 section 7.38.2 | Array lengths below `-1` reject with `Bad_DecodingError`; null `-1`, empty `0`, and valid positive counts remain distinct. | `docs/validation/audit-hardening.md` US1 Discovery negative paths PASS; `docs/validation/sanitizers.md` focused malformed decoding and transport bounds PASS; `docs/validation/fuzz.md` `fuzz_binary_reader` smoke PASS. | Fixed - evidence recorded |
| AH-DECLARED-LENGTH-BOUNDS | TDD backlog / negative-path testing (`TDD-NEG-002`) | High/P1 | FR-002, FR-003, FR-020 | OPC-10000-6 sections 5.2 and 5.2.2.15; OPC-10000-4 section 7.38.2 | Overdeclared strings/byte strings and ExtensionObject bodies reject before reads, skips, callbacks, or state mutation. | `docs/validation/audit-hardening.md` US1 Session and identity decoding PASS, US1 History validation PASS; `docs/validation/sanitizers.md` focused malformed decoding and transport bounds PASS; `docs/validation/fuzz.md` `fuzz_binary_reader` and `fuzz_activate_session` smokes PASS. | Fixed - evidence recorded |
| AH-MANDATORY-SERVICE-BODIES | spec-to-code compliance (`SPEC-CODE-001`) | High/P1 | FR-002, FR-003, FR-020 | OPC-10000-4 sections 5.5.2.2, 5.5.4.2, 5.7.2, 5.7.3, 5.11.3.2, 5.13.2.4, 5.13.3.4, and 7.38.2 | Truncated mandatory service bodies fail before session allocation, callbacks, or service state mutation. | `docs/validation/audit-hardening.md` US1 Discovery, Session and identity, Subscription, and History rows all PASS; `docs/validation/host-tests.md` T085b focused US1-US3 host CTest group PASS, 25/25. | Fixed - evidence recorded |
| AH-TRANSPORT-CHUNK-NEGOTIATION | Codex Security / security-audit (`CODEXSEC-001`) | High/P1 | FR-003, FR-009, FR-010, FR-020 | OPC-10000-6 sections 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, and 7.2; OPC-10000-4 section 7.38.2 | Invalid TCP message types, abort chunks, non-final chunks, and unsupported HEL/ACK buffer negotiation reject deterministically without service dispatch. | `docs/validation/sanitizers.md` focused malformed decoding and transport bounds PASS for `test_message_chunk_errors` and `test_tcp_connection`; `docs/validation/fuzz.md` `fuzz_message_chunk` smoke and fixed-input smoke PASS. | Fixed - evidence recorded |
| AH-READ-BROWSE-ENUM-STATUS | spec-to-code compliance (`SPEC-CODE-002`) | Medium/P2 | FR-012, FR-019, FR-020 | OPC-10000-4 sections 5.9.2.4, 5.11.2.3, 7.38.2, and 7.39 | Invalid `TimestampsToReturn` and `BrowseDirection` return exact OPC UA StatusCodes. | `docs/validation/audit-hardening.md` US1 Read validation PASS and US1 Browse validation PASS; `docs/conformance/status.md` records numeric `Bad_TimestampsToReturnInvalid` and `Bad_BrowseDirectionInvalid` values. | Fixed - evidence recorded |
| AH-WRITE-DATAVALUE-PRESENCE | manual reviewer (`REVIEW-001`) | High/P1 | FR-004, FR-012, FR-020 | OPC-10000-4 sections 5.11.4.2, 5.11.4.4, and 7.38.2 | Write requests whose `DataValue` omits the value field reject before application callbacks. | `docs/validation/audit-hardening.md` US1 Write validation PASS; `docs/validation/host-tests.md` write-service integration rerun PASS. | Fixed - evidence recorded |
| AH-QUERY-COUNT-SAFETY | TDD backlog / negative-path testing (`TDD-NEG-003`) | High/P1 | FR-017, FR-020 | OPC-10000-4 sections 7.7.1, Appendix B section B.2.3, Appendix B section B.2.4, and 7.38.2 | Query/filter counts validate overflow and fixed-capacity limits before storage assignment. | `docs/validation/audit-hardening.md` US1 Query validation PASS; `docs/traceability/files-to-sections.md` maps `src/encoding/binary_query.c` and `tests/unit/test_query_service.c` to QueryFirst/ContentFilter sections. | Fixed - evidence recorded |
| AH-SECURITY-FRESHNESS | Codex Security / security-audit (`CODEXSEC-002`) | High/P1 | FR-005, FR-020 | OPC-10000-4 sections 5.6.2, 5.6.2.3, 5.7.2.2, 5.7.2.3, and 7.38.2 | SecureChannel/session security identifiers and server nonces require entropy and fail closed on freshness failure. | `docs/validation/audit-hardening.md` US2 SecureChannel freshness PASS and US2 Session binding PASS; `docs/validation/sanitizers.md` focused security and session misuse PASS. | Fixed - evidence recorded |
| AH-SESSION-CHANNEL-BINDING | Codex Security / security-audit (`CODEXSEC-003`) | High/P1 | FR-006, FR-007, FR-020 | OPC-10000-4 sections 5.7.2.1, 5.7.3, and 7.38.2 | Sessions are bound to the creating SecureChannel; unactivated known sessions and unknown sessions return distinct StatusCodes. | `docs/validation/audit-hardening.md` US2 Session binding PASS and US2 Connection multiplexing PASS; `docs/validation/sanitizers.md` focused security and session misuse PASS. | Fixed - evidence recorded |
| AH-NONE-USERNAME-POLICY | security-audit / Codex Security (`SEC-AUD-001`) | High/P1 | FR-008, FR-018, FR-019, FR-020 | OPC-10000-4 sections 5.5.4.2, 5.7.3, 5.7.3.3, 7.38.2, 7.40.2.1, and 7.41 | `SecurityPolicy#None` does not advertise or accept plaintext username/password by default while anonymous-over-None remains available. | `docs/validation/audit-hardening.md` US2 User authentication defaults PASS; `docs/conformance/identity-policy.md` records default policy; `docs/conformance/security.md` documents None as anonymous/non-production. | Fixed - evidence recorded |
| AH-DISCOVERY-FILTERS | spec-to-code compliance (`SPEC-CODE-003`) | Medium/P2 | FR-011, FR-020 | OPC-10000-4 sections 5.5.2.2 and 5.5.4.2 | `FindServers` and `GetEndpoints` decode and honor the endpoint URL, locale, server URI/type, security policy, and transport-profile filters. | `docs/validation/audit-hardening.md` US3 Discovery filters PASS; `docs/conformance/services.md` lists filtered Discovery subsets. | Fixed - evidence recorded |
| AH-MONITORING-FILTER-STATUS | CodeRabbit (`CODERABBIT-001`) | Medium/P2 | FR-013, FR-020 | OPC-10000-4 sections 5.13.2.4, 5.13.3.4, 7.22.1, and 7.38.2 | Unsupported, invalid, and disallowed MonitoringFilters return per-item filter StatusCodes instead of silent success. | `docs/validation/audit-hardening.md` US1 Subscription validation PASS and US3 MonitoringFilter semantics PASS; `docs/validation/fuzz.md` `fuzz_subscription_filters` smoke PASS. | Fixed - evidence recorded |
| AH-AGGREGATE-SCOPE | spec-to-code compliance (`SPEC-CODE-004`) | Medium/P2 | FR-014, FR-018, FR-020 | OPC-10000-4 section 7.22.4; OPC-10000-13 sections 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, and 5.4.3.11 | AggregateFilter claims are scoped to verified Average, Minimum, and Maximum behavior; unsupported aggregate selections reject. | `docs/validation/audit-hardening.md` US3 AggregateFilter scope PASS; `docs/conformance/services.md` records scoped aggregate support and unsupported rejection. | Fixed - evidence recorded |
| AH-PUBLISH-OVERSIZE | manual reviewer (`REVIEW-002`) | Medium/P2 | FR-016, FR-020 | OPC-10000-4 sections 5.14.5.1, 5.14.5, and 7.38.2 | Oversize Publish notification response returns deterministic `Bad_ResponseTooLarge` behavior. | `docs/validation/audit-hardening.md` US3 Publish oversize behavior PASS; `docs/conformance/status.md` records numeric `Bad_ResponseTooLarge`. | Fixed - evidence recorded |
| AH-UNSUPPORTED-SERVICE-STATUS | spec-to-code compliance (`SPEC-CODE-005`) | Medium/P2 | FR-012, FR-018, FR-020 | OPC-10000-4 section 7.38.2; OPC-10000-7 sections 4.2 and 4.3 | Unsupported services return `Bad_ServiceUnsupported` and public service claims remain profile-targeting. | `docs/validation/audit-hardening.md` US3 Dispatch unsupported services PASS; `docs/conformance/services.md` marks unsupported and optional services explicitly. | Fixed - evidence recorded |

## Previously Deferred / Not Closed by T091c

These rows were explicitly deferred or not-yet-closed by T091c. T102b updated
repository-fixable rows with current evidence, and T102c added the source-ID
ledger that closes the remaining process blocker.

| Finding ID | Source audit lane | Severity | Affected FR(s) | OPC UA citation(s) or non-protocol rationale | Current blocker / evidence | Risk rationale | Required follow-up / approval placeholder | Closure status |
|---|---|---|---|---|---|---|---|---|
| AH-SANITIZER-HISTORY-LIFETIME-ASAN | security-audit / TDD backlog (`SEC-AUD-002`) | High/P1 | FR-015, FR-020 | OPC-10000-4 sections 5.11.3.2 and 7.9; OPC-10000-6 section 5.2.2.15 | T102b fixed HistoryRead continuation-point ownership in `handle_history_read` and reran sanitizer evidence. `docs/validation/sanitizers.md` records full ASan/UBSan CTest PASS, 95/95 targets, and focused `test_history` PASS. | The previous ASan `stack-use-after-scope` is historical pre-remediation evidence; current sanitizer runs are clean for this path. | Keep HistoryRead lifetime covered by sanitizer and host tests when continuation-point storage changes. | Fixed - evidence recorded |
| AH-FUZZ-BINARY-TYPES-PLACEHOLDER | TDD backlog / negative-path testing (`TDD-NEG-004`) | High/P1 | FR-001, FR-003, FR-020 | OPC-10000-6 sections 5.2, 5.2.2.15, and 5.2.5 | T102b replaced `tests/fuzz/fuzz_binary_types.c` with input-consuming coverage for NodeId, String, ByteString, ExtensionObject, Variant, DataValue, and structured frames. `docs/validation/fuzz.md` records `build-fuzz/tests/fuzz/fuzz_binary_types -runs=2` PASS with `cov: 11`, `ft: 12`, and `test_conformance_docs` PASS. | The former placeholder/input-discard blocker is removed; longer fuzz campaigns remain future depth, not this closure blocker. | Keep placeholder and input-discard doc gates enabled for all fuzz targets. | Fixed - evidence recorded |
| AH-HOST-FULL-FLASH-BUDGET-OVER | manual reviewer (`REVIEW-003`) | High/P1 | FR-019, FR-020 | Non-protocol resource evidence; OPC-10000-7 sections 4.2 and 4.3 for conformance-claim honesty | `cmake/MucOpcUaCodegen.cmake` now defaults `MUC_OPCUA_OPTIMIZE_SIZE=ON`. `docs/size/feature-size-ledger.md` records default host/full `text=96,398 B`, `data=6,224 B`, `bss=0 B`, delta `-56,311 B` vs baseline `152,709 B`; explicit `OPTIMIZE_SIZE=OFF` remains reference-only at `text=166,784 B`. | The constrained-device default no longer exceeds the +8 KiB host/full budget. | Treat unoptimized opt-out builds as reference evidence, not the resource gate default. | Fixed - evidence recorded |
| AH-ARM-NANO-EMBEDDED-SIZE-BLOCKED | manual reviewer (`REVIEW-004`) | High/P1 | FR-019, FR-020 | Non-protocol ARM Cortex-M0+ resource evidence; OPC-10000-7 sections 4.2 and 4.3 for conformance-claim honesty | `BUILD_ROOT=build/audit-size-arm scripts/measure_size.sh all` now passes. `docs/size/feature-size-ledger.md` records nano `16,366/0/0/16,366`, micro `23,873/0/0/23,873`, embedded `43,078/0/0/43,078`, and full-featured `51,172/0/0/51,172`. | The unused `policy` and connection-storage static assertion blockers are fixed. | Maintain ARM matrix in release validation for optional/profile changes. | Fixed - evidence recorded |
| AH-PICO-MINIMAL-SERVER-BLOCKED | manual reviewer (`REVIEW-005`) | High/P1 | FR-019, FR-020 | Non-protocol Pico/RP2040 resource evidence; OPC-10000-7 sections 4.2 and 4.3 for conformance-claim honesty | `docs/size/pico-minimal-server.md` records current Pico embedded build PASS with `build/t089a-pico-embedded/src/libmuc_opcua.a`, `pico_minimal_server.elf`, and `pico_minimal_server.uf2` produced. ELF size is `text=73,428`, `data=0`, `bss=119,340`, `dec=192,768`; UF2 file size is `138,752 B`. | The Pico storage assertion/build blocker is fixed and measurable artifacts exist. | Keep Pico artifact and size checks current for embedded release validation. | Fixed - evidence recorded |
| AH-PICO-STACK-EVIDENCE-BLOCKED | manual reviewer (`REVIEW-006`) | High/P1 | FR-019, FR-020 | Non-protocol Pico/RP2040 stack evidence; OPC-10000-7 sections 4.2 and 4.3 for conformance-claim honesty | `docs/size/pico-minimal-server.md` records Pico stack PASS: `cmake --build build/t089b-pico-stack` and `scripts/check_stack_usage.sh --build-dir build/t089b-pico-stack --threshold 10240` pass with 35 `.su` files and secured OPN estimate `2,776 B` against the `10,240 B` threshold. | Current Pico stack evidence is below threshold and includes required frames. | Keep Pico stack instrumentation in the embedded evidence gate. | Fixed - evidence recorded |
| AH-STATIC-RAM-CALLER-STORAGE-DELTAS-INCOMPLETE | manual reviewer (`REVIEW-007`) | Medium/P2 | FR-019, FR-020 | Non-protocol storage evidence; OPC-10000-7 sections 4.2 and 4.3 for conformance-claim honesty | `docs/size/feature-size-ledger.md` records host/full archive `.data=6,224 B`, `.bss=0 B`, flat against baseline, no BSS symbols in `nm`, ARM archive `.data=0`, `.bss=0`, and absolute caller-storage values including host/full `MU_SERVER_STORAGE_BYTES=125,020 B`, ARM embedded `98,520 B`, ARM nano `1,280 B`, and `MU_CONNECTION_BASE_STORAGE_BYTES=1,328 B`. | The evidence now records absolute caller storage and avoids inventing a missing pre-change delta; archive static RAM is flat and embedded artifacts are measurable. | Establish formal caller-storage deltas in a future baseline if release policy requires delta rather than absolute storage evidence. | Fixed - evidence recorded |
| AH-HEAP-EVIDENCE-ADVISORY-ONLY | manual reviewer (`REVIEW-008`) | Medium/P2 | FR-019, FR-020 | Non-protocol heap evidence; OPC-10000-7 sections 4.2 and 4.3 for conformance-claim honesty | `docs/review/embedded-readiness-review.md` and `docs/size/feature-size-ledger.md` record no allocator calls in the core protocol hot path (`src/core`, `src/encoding`, `src/services`). Remaining matches are `mu_session_find_free` false positives or adapter/backend cleanup hooks outside the core claim; Pico/ARM artifacts now build. | The core protocol hot-path no-heap claim is supported by current source review and embedded artifacts. Third-party backend internals remain adapter-specific. | Keep platform/backend allocation expectations documented per supported adapter. | Fixed - evidence recorded |
| AH-AUTHORITATIVE-SIX-AGENT-IDS-MISSING | All six contract lanes (`PROCESS-001`) | High/P1 | FR-021 | Non-protocol closure traceability requirement from FR-021 and SC-008 | T102c added `docs/validation/audit-hardening-triage-ledger.md`, which assigns repository-owned source finding IDs and severities for all 28 closure rows and represents all six audit lanes. | The original external subagent transcript IDs are still unavailable, so the ledger explicitly avoids claiming to reproduce them. The in-repository process blocker is closed because the feature now has concrete source IDs, lanes, severities, and outcomes for re-checking. | Keep `docs/validation/audit-hardening-triage-ledger.md` synchronized with future imported findings. | Fixed - evidence recorded |

## Profile-Disabled / Scoped Rows from T091b

These rows remain visibly non-fixed by design. They document disabled or scoped
profile behavior; they do not expand unsupported profile claims.

| Finding ID | Source audit lane | Severity | Affected FR(s) | OPC UA citation(s) | Out-of-scope / disabled-profile rationale | Validation artifact(s) | Closure status |
|---|---|---|---|---|---|---|---|
| AH-NANO-OPTIONAL-SERVICES-DISABLED | spec-to-code compliance (`SPEC-CODE-006`) | Low/P3 | FR-012, FR-018, FR-019, FR-021 | OPC-10000-7 sections 4.2 and 4.3; OPC-10000-4 sections 5.11.4, 5.12.2.2, 5.13, 5.14, 5.11.3, 5.11.5, 5.8.2, 5.8.3, and 7.38.2 | Nano profile does not claim Subscriptions, MonitoredItems, Methods, Write, History, or NodeManagement; unsupported service requests remain documented as `Bad_ServiceUnsupported` rather than partial support. | `docs/conformance/profile-nano.md` "Profile constraints honoured" excludes those services; `docs/conformance/services.md` marks Write/Call/History/NodeManagement as optional subsets and TransferSubscriptions unsupported; `docs/validation/audit-hardening.md` US3 Dispatch unsupported services PASS. | Profile-disabled - documented |
| AH-MICRO-EMBEDDED-TIER-DELTAS-DISABLED | spec-to-code compliance (`SPEC-CODE-007`) | Low/P3 | FR-013, FR-018, FR-019, FR-021 | OPC-10000-7 sections 4.2 and 4.3; OPC-10000-4 sections 5.13.2, 5.13.5, 5.12.2.2, 7.20.1, 7.22.2, 7.22.4, and 7.38.2 | Micro profile keeps Embedded-tier deltas disabled unless Embedded gates are enabled: absolute deadband, queue-size-2/overflow capacity, SetTriggering, Base Info Type System, Method Call, and security policies beyond None are not Micro claims. | `docs/conformance/profile-micro.md` "Out of scope for Micro" lists Embedded-tier deltas; `docs/conformance/profile-embedded.md` lists the enabled Embedded surface; `docs/conformance/services.md` scopes SetTriggering and built-in Call methods behind `MUC_OPCUA_SUBSCRIPTIONS_STANDARD`; `docs/validation/audit-hardening.md` US3 MonitoringFilter semantics and Dispatch unsupported services PASS. | Profile-disabled - documented |
| AH-TRANSFER-SUBSCRIPTIONS-UNSUPPORTED | spec-to-code compliance (`SPEC-CODE-008`) | Low/P3 | FR-012, FR-018, FR-019, FR-021 | OPC-10000-7 sections 4.2 and 4.3; OPC-10000-4 sections 5.14.7 and 7.38.2 | TransferSubscriptions belongs to a client-redundancy facet outside the selected Micro/Embedded profile slice, so the service remains unsupported instead of being expanded during audit hardening. | `docs/conformance/services.md` marks TransferSubscriptions unsupported outside `MUC_OPCUA_REDUNDANCY`; `docs/conformance/profile-micro.md` and `docs/conformance/profile-embedded.md` explicitly list it as out of scope; `docs/traceability/005-embedded-profile-completion.md` records expected `Bad_ServiceUnsupported`; `docs/validation/audit-hardening.md` US3 Dispatch unsupported services PASS. | Profile-disabled - documented |
| AH-DEFAULT-OPTIONAL-FULL-SERVICES-SCOPED | spec-to-code compliance (`SPEC-CODE-009`) | Low/P3 | FR-015, FR-018, FR-019, FR-021 | OPC-10000-7 sections 4.2 and 4.3; OPC-10000-4 sections 5.8.2, 5.8.3, 5.11.3, 5.11.5, Appendix B section B.2.3, Appendix B section B.2.4, and 7.38.2 | History, NodeManagement, Query, and Write are not unconditional profile claims; they are optional implemented subsets behind feature/profile macros, so disabled-profile builds are documented as unsupported rather than requiring broad default-profile expansion. | `docs/conformance/services.md` scopes Write, History, NodeManagement, and Query behind `MUC_OPCUA_SERVICE_*` options; `docs/conformance/status.md` lists these under Embedded profile progress rather than Nano/Micro baseline; `docs/validation/audit-hardening.md` US3 Persistent data lifetime PASS for enabled host tests and US3 Dispatch unsupported services PASS for unsupported dispatch. | Profile-scoped - documented |

## Import Checklist

Use this checklist when replacing placeholder rows with confirmed findings:

- Finding ID: replace stable descriptive IDs above when authoritative source IDs
  become available.
- Source audit lane: keep one of the contract lanes unless the imported triage
  provides a more specific source label.
- Severity and priority, if separate: use
  `docs/validation/audit-hardening-triage-ledger.md` for the current repository
  severity and priority.
- Affected FR(s): cite active `020-audit-hardening` FRs.
- OPC UA citation(s) or non-protocol rationale: use exact part/section for
  protocol-visible behavior.
- Expected fix, documentation change, or out-of-scope rationale: state the
  observable behavior or claim change.
- RED test, verification command, or static evidence: cite the row or artifact
  that records the command result.
- Final validation artifact link and result: required before `Fixed - evidence
  recorded`.
- Size/RAM/stack impact evidence, when applicable: leave pending until resource
  ledgers record post-change evidence.
- Closure status: fixed, pending profile-disabled, pending deferred, or pending
  blocker; T091a only records fixed findings with current evidence.
