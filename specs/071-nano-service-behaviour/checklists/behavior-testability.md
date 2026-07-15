<!-- markdownlint-disable MD013 MD060 -->

# Behavior Testability Checklist: Server Nano Core Service Behaviour CUs

**Purpose**: Plan-entry quality gate for behavior projection from requirements  
**Created**: 2026-07-14  
**Feature**: [spec.md](../spec.md)  
**Depth**: Formal Spec Kit planning gate  
**Audience/Timing**: Author and reviewer before `/speckit-plan`

## User Story Readiness

- [x] CHK001 Are all user stories independently testable from observable OPC UA service or manifest/conformance outcomes? [Completeness, Spec User Stories]
- [x] CHK002 Are actors and stakeholder value explicit for nano integrators, micro-or-higher integrators, clients, and test harnesses? [Clarity, Spec User Stories]
- [x] CHK003 Are priorities consistent with roadmap dependency order and conformance risk? [Consistency, Spec User Stories]

## Acceptance Criteria Quality

- [x] CHK004 Are acceptance scenarios written as Given/When/Then statements with observable outcomes? [Measurability, Spec Acceptance Scenarios]
- [x] CHK005 Are service behaviours tied to cited OPC UA sections instead of implementation-only expectations? [Traceability, Spec OPC-001..OPC-010]
- [x] CHK006 Are optional CU behaviours distinguished from nano-default mandatory behaviours? [Clarity, Spec FR-003]

## Scenario Coverage

- [x] CHK007 Are primary success paths specified for discovery, view, write, session change-user, and diagnostics behaviours? [Coverage, Spec User Stories]
- [x] CHK008 Are malformed request, unsupported feature, and not-enabled CU behaviours specified as requirement-level edge cases? [Coverage, Spec Edge Cases]
- [x] CHK009 Are profile-default and independent custom-profile dedicated-symbol paths specified for each claim-ready CU? [Coverage, Spec FR-002..FR-007, FR-011]

## Case Coverage Matrix

| Case ID | Story/Capability | Case Type | Status | Source | Planning Coverage |
|---------|------------------|-----------|--------|--------|-------------------|
| CASE-001 | Discovery FindServers/GetEndpoints | Positive | Required | Spec US1 AC1-AC2 | Assign scenario in plan |
| CASE-002 | View Browse/BrowseNext/TranslateBrowsePaths | Positive | Required | Spec US1 AC3 | Assign scenario in plan |
| CASE-003 | Write Value/StatusCode/Timestamp; deferred IndexRange unsupported | Positive | Required | Spec US2 AC1-AC3 | Assign scenario in plan |
| CASE-004 | ActivateSession change user | Positive | Required | Spec US2 AC4 | Assign scenario in plan |
| CASE-005 | Diagnostics exposure and returnDiagnostics | Positive | Required | Spec US3 AC1-AC2 | Assign scenario in plan |
| CASE-006 | Malformed or truncated service request | Negative | Required | Spec Edge Cases | Assign scenario in plan |
| CASE-007 | Independently unsupported or disabled dedicated CU behaviour | Permission | Required | Spec US1 AC4, US2 AC5, US3 AC3-AC4, FR-011, OPC-009 | Assign scenario in plan |
| CASE-008 | Excessive operation counts or browse path depth | Boundary | Required | Spec Edge Cases | Assign scenario in plan |
| CASE-009 | Invalid write target, attribute, or type; deferred IndexRange unsupported | Validation | Required | Spec Edge Cases, FR-010 | Assign scenario in plan |
| CASE-010 | Manifest/profile-default or dedicated-gate drift | State conflict | Required | Spec FR-003, FR-011, Assumptions | Assign scenario in plan |
| CASE-011 | Visual UI behaviour | Visual | Not Applicable | No UI or visual requirements in spec | No scenario needed |

## Given Readiness

- [x] CHK010 Are required starting states explicit enough for later fixture setup, including profile selection, CU enabled/disabled state, active Session state, and diagnostics counters? [Completeness, Spec User Stories and Key Entities]
- [x] CHK011 Are entity states explicit for conformance claims, service requests/responses, session user identity, and diagnostics surfaces? [Clarity, Spec Key Entities]
- [x] CHK012 Are permissions and profile gates explicit where a CU is optional or not enabled by nano defaults? [Coverage, Spec FR-003]

## When Readiness

- [x] CHK013 Are triggers expressed as executable request cases or system triggers rather than vague implementation activities? [Clarity, Spec Acceptance Scenarios]
- [x] CHK014 Are artifact-generation and manifest-validation triggers specified for claim changes? [Completeness, Spec FR-008]

## Then Readiness

- [x] CHK015 Are outcomes mapped to response contents, StatusCode semantics, manifest state, generated artifacts, or diagnostics values? [Measurability, Spec Acceptance Scenarios]
- [x] CHK016 Are failure outcomes specified without requiring implementation design choices during requirements validation? [Clarity, Spec Edge Cases]

## Visual Fidelity Readiness

- [x] CHK017 Are visual requirements explicitly absent from the feature scope? [Not Applicable, Spec Scope Boundaries]

## Visual Fidelity Evidence Matrix

| Visual ID | Requirement / Proof Obligation | Screenshot Evidence | Visual Proof Required | Provider Evidence | Status | Blocking Item |
|-----------|--------------------------------|---------------------|-----------------------|-------------------|--------|---------------|
| VIS-N/A | No UI or visual fidelity requirements are in scope | Not Applicable | Not Applicable | Not Applicable | Not Applicable | none |

## Non-Functional Requirement Readiness

- [x] CHK018 Are performance and size requirements explicit at product/conformance level? [Completeness, Spec Application Headroom Goal and SC-006]
- [x] CHK019 Are security and conformance honesty constraints explicit, including profile-targeting rather than CTT-verified claims? [Completeness, Spec OPC-010]
- [x] CHK020 Are reliability and malformed-input behaviours covered through edge cases and StatusCode expectations? [Coverage, Spec Edge Cases]
- [x] CHK021 Are observability requirements covered through diagnostics surfaces and returnDiagnostics behaviour? [Coverage, Spec US3]
- [x] CHK022 Are accessibility, data lifecycle, and UI visual NFR dimensions marked not applicable by feature scope? [Not Applicable, Spec Scope Boundaries]

## Gate Status

Gate Status: PASS

## Blocking Items

- none
