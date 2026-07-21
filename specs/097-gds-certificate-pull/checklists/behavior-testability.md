# Behavior Testability Checklist: GDS Certificate Pull Management

**Purpose**: BDD readiness gate for GDS Certificate Pull Management (OPC-10000-12 §7)
**Created**: 2026-07-20
**Feature**: [spec.md](../spec.md)
**Gate Status**: PASS
**Blocking Items**: none

## User Story Readiness

- [x] US1 (Browse CertificateManager Type Hierarchy): Accepted by observable Browse behavior — type nodes discoverable via HasSubtype refs. [Spec §US1]
- [x] US2 (StartSigningRequest Method): Accepted by observable Method call → requestId + StatusCode. [Spec §US2]
- [x] US3 (FinishRequest Method): Accepted by observable Method call → certificate bytes / Bad_NotFound. [Spec §US3]
- [x] US4 (GetRejectedList Method): Accepted by observable Method call → rejected request list. [Spec §US4]
- [x] US5 (StartNewKeyPairRequest Method): Accepted by observable Method call → requestId. [Spec §US5]

## Acceptance Criteria Quality

- [x] Are all acceptance scenarios expressed with Given/When/Then structure? [Completeness, Spec §US1-US5]
- [x] Are error-path outcomes explicitly named (Bad_InvalidArgument, Bad_NotFound, Bad_InternalError)? [Clarity, Spec §US2-US5]
- [x] Are compile-out expectations quantified (zero bytes in nano image)? [Measurability, Spec §US1 Scenario 3, FR-009]
- [x] Is adapter-null behavior defined (type nodes exist but Methods return error)? [Completeness, Spec §Edge Cases]

## Scenario Coverage

- [x] Primary: Browse type hierarchy, start+finish certificate request — covered [Spec §US1-US3]
- [x] Alternate: StartNewKeyPairRequest instead of StartSigningRequest — covered [Spec §US5]
- [x] Exception: Invalid/empty signing request → Bad_InvalidArgument, unknown requestId → Bad_NotFound, missing adapter → Bad_InternalError — covered [Spec §US2-US3, Edge Cases]
- [x] Boundary: Concurrent requests yield distinct requestIds, stale requestIds across restarts — covered [Spec §Edge Cases]
- [x] Permission: Not applicable (cert management is authenticated-session-level, no additional role gating in this scope)
- [x] Validation: Non-null args, byte string size > 0 → Bad_InvalidArgument — covered [Spec §FR-008]
- [x] State Conflict: Deferred to adapter; server only validates input — covered by adapter delegation [Spec §Assumptions]

## Case Coverage Matrix

| Case ID | Case Type | Status | Source |
|---------|-----------|--------|--------|
| CM-POS-01 | positive | Required | Spec §US1 — Browse type hierarchy |
| CM-POS-02 | positive | Required | Spec §US2 — StartSigningRequest valid |
| CM-POS-03 | positive | Required | Spec §US3 — FinishRequest valid |
| CM-NEG-01 | negative | Required | Spec §US2 — StartSigningRequest invalid |
| CM-NEG-02 | negative | Required | Spec §US3 — FinishRequest unknown requestId |
| CM-BND-01 | boundary | Required | Spec §Edge Cases — Concurrent requests |
| CM-VAL-01 | validation | Required | Spec §FR-008 — Input argument validation |
| CM-SCN-01 | state_conflict | Not Applicable | Adapter owns state; server is stateless dispatcher |

## Given Readiness

- [x] Are required roles/permissions for Method calls explicit? [Spec §Edge Cases — authenticated session required; no additional role gating in this scope] — N/A, gated by session
- [x] Are starting states defined? [Spec §US2-US5] — adapter provides mock state for test; CU enabled/disabled for compile-out tests
- [x] Are entity states explicit? [Spec §FR-006] — adapter manages certificate lifecycle

## When Readiness

- [x] Are trigger actions executable? [Spec §US2-US5] — Method calls via standard OPC UA Method service
- [x] Are request parameters specified? [Spec §US2] — DER-encoded PKCS#10 ByteString for StartSigningRequest; key spec for StartNewKeyPairRequest

## Then Readiness

- [x] Are outcomes mapped to specific StatusCodes? [Clarity, Spec §US2-US5] — Good, Bad_InvalidArgument, Bad_NotFound, Bad_InternalError
- [x] Are response data specified? [Completeness, Spec §US2] — requestId (UInt32), certificate bytes (ByteString), rejected list entries

## Visual Fidelity Readiness

- [x] Not Applicable — protocol library with no UI; no visual requirements in spec

## Non-Functional Requirement Readiness

| NFR Dimension | Status | Evidence |
|---------------|--------|----------|
| Performance | Not Applicable | Embedded library — performance is size-bound (≤3.0 KB .text target, SC-006) |
| Security & Privacy | Not Applicable | Certificate operations delegated to integrator adapter; library provides only type system + dispatch |
| Reliability & Recovery | Not Applicable | Adapter owns certificate lifecycle; server validates input and delegates |
| Accessibility | Not Applicable | Protocol library with no user-facing interface |
| Compliance & Auditability | Required | OPC-10000-12 §7 normative citations present in FR-010 + OPC-001-OPC-005 |
| Observability | Not Applicable | No logging or metrics requirements in this scope |
| Compatibility | Required | Kconfig compile-gating confirmed (FR-007, FR-009) |
| Data Lifecycle | Not Applicable | Adapter manages certificate storage lifetime |
| Cost / Operational | Not Applicable | Pure library feature |

- [x] Are compliance requirements traceable to spec sections? [Spec §OPC-002] — 10 OPC section citations across OPC-10000-12 §7.8-7.9
- [x] Are compatibility requirements verified by nano compile-out test? [Spec §FR-009, SC-004]

## Gate Status: PASS

All readiness items checked. No blocking items. Ready for `/speckit.plan`.
