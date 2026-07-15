<!-- markdownlint-disable MD013 -->

# Feature Specification: Server Nano Core Service Behaviour CUs

**Feature Branch**: `071-nano-service-behaviour`  
**Created**: 2026-07-14  
**Status**: Refined  
**Refined**: 2026-07-14 — Recorded the approved `opc_cu_3147` deferral and made dedicated-symbol build/dispatch gating explicit for every claimed CU.  
**Input**: Roadmap item 006, manifest entries for `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, `opc_cu_2389`, `opc_cu_2400`, `opc_cu_2936`, `opc_cu_3147`, `opc_cu_3192`, `opc_cu_3530`, and `opc_cu_3983`

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Claim Existing Mandatory Discovery and View Behaviour (Priority: P1)

As an embedded integrator selecting the nano server profile, I need the server's mandatory discovery and view behaviour to be claimed only when it is backed by observable service behaviour and tests, so that profile declarations match what OPC UA clients can actually use.

**Why this priority**: The nano profile cannot honestly progress while mandatory CUs remain unclaimed in the manifest despite existing service handlers and tests.

**Independent Test**: Build the nano profile and verify that the mandatory discovery/view CUs are represented by selectable symbols, backing tests, and manifest state; disable each dedicated symbol in a custom build and verify the corresponding behavior is unavailable without disabling unrelated Discovery/View CUs.

**Acceptance Scenarios**:

1. **Given** a nano-profile build, **When** a client invokes `FindServers` against the server's own endpoint, **Then** the server returns only its own application description according to OPC-10000-4 section 5.5.2.
2. **Given** a nano-profile build, **When** a client invokes `GetEndpoints` with endpoint and transport profile filters, **Then** matching endpoint descriptions are returned and non-matching endpoints are excluded according to OPC-10000-4 section 5.5.4.
3. **Given** a nano-profile build, **When** a client invokes `TranslateBrowsePathsToNodeIds` or Browse/BrowseNext within advertised operation limits, **Then** the result StatusCodes and continuation-point behaviour match OPC-10000-4 sections 5.9.2, 5.9.3, and 5.9.4.
4. **Given** one dedicated Discovery or View CU symbol is disabled in a custom build, **When** a client invokes only that CU's service behavior, **Then** the behavior is unavailable with an explicit OPC UA StatusCode while other enabled CUs remain available.

---

### User Story 2 - Complete Optional Write and Session Behaviour (Priority: P2)

As an integrator enabling micro or higher server profiles, I need Write and ActivateSession change-user behaviour to be explicit, tested, and gated by named CU symbols, so that optional service support can be included without broad feature aliases or hidden conformance claims.

**Why this priority**: The roadmap item includes write and session CUs that the manifest defaults on for micro and higher, but not for nano; those CUs need independent claims and tests before profile defaults can be trusted.

**Independent Test**: Enable the claim-ready CU symbols in a micro-or-higher build and verify Write value, Write StatusCode/Timestamp, and ActivateSession change-user requests produce the expected per-operation and service-level results; disable each symbol independently and verify its behavior is unavailable. `opc_cu_3147` remains explicitly unsupported and unclaimed under the approved deferral.

**Acceptance Scenarios**:

1. **Given** write CUs are enabled, **When** a client writes one or more Value attributes, **Then** the server returns per-node Write results and does not report success for invalid targets or unsupported attributes according to OPC-10000-4 section 5.11.4.
2. **Given** StatusCode/Timestamp write support is enabled, **When** a client writes a DataValue containing supported StatusCode and timestamp fields, **Then** the server preserves or rejects those fields according to documented support rather than silently ignoring them.
3. ~~**Given** IndexRange write support is enabled, **When** a client writes an indexed element or range to an array value where partial updates are allowed, **Then** the server updates only the requested range or returns the cited OPC UA error for unsupported or invalid ranges.~~ **Deferred 2026-07-14**: `opc_cu_3147` remains unimplemented and unclaimed; non-empty IndexRange writes must return explicit unsupported behavior without mutation until follow-up `S071-D1` is implemented.
4. **Given** a Session has already been activated, **When** a client calls ActivateSession with a new valid user identity, **Then** the Session user changes and a new server nonce is returned according to OPC-10000-4 section 5.7.3.
5. **Given** a dedicated optional Write or Session CU symbol is disabled, **When** a client attempts that CU's behavior, **Then** the server returns an explicit unsupported StatusCode and preserves the previous value, identity, and nonce state.

---

### User Story 3 - Expose and Return Diagnostics Consistently (Priority: P3)

As a client or test harness requesting diagnostics, I need diagnostic counters and returnDiagnostics responses to be consistent with the server's address-space diagnostics nodes, so that diagnostics CUs are observable rather than label-only.

**Why this priority**: Existing diagnostics counters and ServerDiagnostics nodes are present, but the roadmap requires CU-level coverage for Base Info Diagnostics and Base Services Diagnostics.

**Independent Test**: Enable diagnostics CUs, drive successful and failed services, read ServerDiagnostics nodes, and request service diagnostics through `returnDiagnostics`; verify the same events are reflected in both observable surfaces.

**Acceptance Scenarios**:

1. **Given** diagnostics CUs are enabled, **When** sessions, subscriptions, or requests are created, closed, rejected, or timed out, **Then** ServerDiagnosticsSummary values exposed under the Server object reflect the live counters required by OPC-10000-5 sections 6.3.3 and 8.3.2.
2. **Given** a request includes supported `returnDiagnostics` flags, **When** a service returns a diagnostic-capable success or failure, **Then** available diagnostic information is returned according to OPC-10000-4 common response-header and diagnostic-info rules.
3. **Given** diagnostics are disabled by profile configuration, **When** the same services execute, **Then** the server does not claim diagnostics CUs and does not expose partial diagnostics as conformant.
4. **Given** one dedicated diagnostics CU symbol is disabled independently, **When** a client requests that CU's address-space or response-diagnostics behavior, **Then** only the disabled behavior is unavailable and no diagnostics claim is emitted for it.

### Edge Cases

- Service arrays with malformed negative lengths, excessive counts, truncated body data, or invalid NodeIds return cited service-level or operation-level StatusCodes.
- TranslateBrowsePathsToNodeIds handles missing target browse names, namespace-index mismatches, unmatched references, and overly deep browse paths without buffer overflow or partial success claims.
- Write handles unsupported attributes, read-only nodes, type mismatches, invalid StatusCode/Timestamp fields, unsupported IndexRange use, and partial array writes that exceed the target array.
- ActivateSession rejects invalid identity tokens, unsupported user-token policies, and change-user attempts on invalid or inactive sessions with cited StatusCodes.
- Diagnostics requests with unsupported diagnostic masks fail safely or return an explicitly empty diagnostics section; they do not fabricate diagnostic data.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST audit each in-scope CU against existing code and tests and classify it as already satisfied, needs a named symbol, needs additional tests, or needs missing service behaviour.
- **FR-002**: The system MUST promote each satisfied in-scope CU from `implementation_state: unimplemented` to an honest implemented state only when it has a dedicated `MUC_OPCUA_CU_*` symbol, profile defaults, and backing tests in the manifest.
- **FR-003**: The system MUST keep nano-default claims limited to CUs whose manifest `profile_defaults.nano` value is true; optional write, session change-user, and diagnostics CUs MUST remain off in nano unless their manifest default requires otherwise.
- **FR-004**: The system MUST provide CU-level coverage for `opc_cu_2317` View TranslateBrowsePath, `opc_cu_2328` Discovery Get Endpoints, `opc_cu_2352` Discovery Find Servers Self, and `opc_cu_3530` View Basic 2 for nano and higher profile defaults.
- **FR-005**: The system MUST provide gated CU-level coverage for `opc_cu_2389` Attribute Write Values and `opc_cu_2936` Attribute Write StatusCode & Timestamp for profiles where the manifest defaults these CUs on or where a custom profile enables them. ~~This feature claims `opc_cu_3147` Attribute Write Index.~~ **Deferred 2026-07-14**: `opc_cu_3147` remains unimplemented and unclaimed under FR-010 and follow-up `S071-D1`.
- **FR-006**: The system MUST provide gated CU-level coverage for `opc_cu_2400` Session Change User for profiles where the manifest defaults this CU on or where a custom profile enables it.
- **FR-007**: The system MUST provide gated CU-level coverage for `opc_cu_3192` Base Info Diagnostics and `opc_cu_3983` Base Services Diagnostics for profiles where the manifest defaults these CUs on or where a custom profile enables them.
- **FR-008**: The system MUST regenerate and validate generated Kconfig, defconfigs, capacity headers, claim maps, conformance docs, and build documentation after manifest changes.
- **FR-009**: The system MUST add or update unit, integration, or fixture tests so every in-scope CU has an executable proof tied to its service behaviour or address-space diagnostics surface.
- **FR-010**: The system MUST document any in-scope CU that remains intentionally unclaimed, including the exact missing behaviour and the tracked follow-up required before it can be claimed.
- **FR-011**: Every dedicated CU symbol used by an implemented manifest claim MUST control the corresponding compiled or dispatched behavior independently; disabling the symbol MUST make that behavior unavailable without relying on a broader aggregate alias or disabling unrelated CUs.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Conformance-unit claims are governed by OPC-10000-7 section 4.2, which defines ConformanceUnits as testable feature sets.
- **OPC-002**: Discovery behaviour for `FindServers` and `GetEndpoints` is governed by OPC-10000-4 sections 5.5.1, 5.5.2, and 5.5.4.
- **OPC-003**: Session change-user behaviour is governed by OPC-10000-4 sections 5.7.2 and 5.7.3, including ActivateSession association of user identity with a Session and returning a new server nonce on activation.
- **OPC-004**: View and browse-path behaviour is governed by OPC-10000-4 sections 5.9.2, 5.9.3, and 5.9.4.
- **OPC-005**: Write behaviour, including whole-attribute and indexed constructed values, is governed by OPC-10000-4 section 5.11.4 and its service and operation-level StatusCode tables.
- **OPC-006**: Common response diagnostics and StatusCodes are governed by OPC-10000-4 sections 7.32 and 7.38.
- **OPC-007**: ServerDiagnostics address-space exposure is governed by OPC-10000-5 sections 6.3.1, 6.3.3, and 8.3.2; ServerDiagnosticsSummaryDataType encoding is governed by OPC-10000-5 section 12.9.
- **OPC-008**: Binary wire encoding for new or updated request/response fixtures remains governed by OPC-10000-6 section 5.2.1.
- **OPC-009**: Unsupported or not-enabled service behaviour MUST return an explicit OPC UA StatusCode rather than silently succeeding or claiming conformance.
- **OPC-010**: Conformance status after this feature remains profile-targeting, not CTT-verified, until external CTT evidence exists.

### Scope Boundaries *(mandatory)*

- **In Scope**: Ten roadmap item 006 CUs: `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, `opc_cu_2389`, `opc_cu_2400`, `opc_cu_2936`, `opc_cu_3147`, `opc_cu_3192`, `opc_cu_3530`, and `opc_cu_3983`.
- **In Scope**: Manifest, Kconfig, generated conformance docs, and profile tests required to make these CU claims honest.
- **In Scope**: Service tests for discovery, view, write, session change-user, and diagnostics behaviours required by the in-scope CUs.
- **In Scope**: CMake/source/dispatch wiring and profile tests required to prove each dedicated CU symbol independently gates its claimed behavior.
- **Out of Scope**: Security administration, security policy selection, role authorization, time synchronization, subscription service CUs, NodeManagement, Query, Historical Access, and CTT certification.
- **Compatibility Claim**: After completion, the server may claim only the in-scope CUs that have implemented manifest state, generated Kconfig symbols, and backing tests; profile-level conformance remains profile-targeting.
- **Application Headroom Goal**: Added default nano flash impact SHOULD stay within the roadmap estimate of approximately 0.5-2 KB for mandatory service-behaviour additions; optional/micro+ CU code MUST compile out when not enabled.

### Key Entities *(include if feature involves data)*

- **Conformance Unit Claim**: A manifest record with an OPC CU id, display name, profile defaults, implementation state, Kconfig symbol, backing tests, and normative reference evidence.
- **Service Request/Response Behaviour**: Observable OPC UA service result, operation result, and encoded response body for Discovery, Session, View, Write, and Diagnostics-related requests.
- **Session User Identity**: The user identity associated with an activated Session and changed through a later ActivateSession call when `opc_cu_2400` is enabled.
- **Diagnostics Surface**: The combination of live diagnostic counters, ServerDiagnostics address-space nodes, and optional service-level diagnostic information returned through response headers.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All ten in-scope CU manifest entries have a final classification and no entry remains silently unimplemented without a documented reason and follow-up.
- **SC-002**: Every claimed in-scope CU has at least one backing test listed in the manifest and executable in the relevant host/profile test path.
- **SC-003**: Nano profile builds expose only nano-default in-scope CUs while micro, embedded, standard, full, and custom profiles can include their corresponding optional/default-on CUs.
- **SC-004**: Generated profile artifacts validate with the repository manifest validator, and generated files have no uncommitted drift after regeneration.
- **SC-005**: Discovery, view, write, session, and diagnostics service tests cover valid requests, malformed requests, unsupported/not-enabled behaviour, and cited StatusCode outcomes for each changed behaviour.
- **SC-006**: The default nano build remains within the documented size budget and any measured growth is recorded in the feature size ledger.
- **SC-007**: The implementation plan and task list cite the exact OPC-10000 sections for every task that changes protocol-visible behaviour.

## Assumptions

- Roadmap item 006 is a claim-and-gap-closing slice: existing service implementations may satisfy some CUs once symbols, manifest state, and tests are added.
- `profiles/opcua-profile-manifest.yaml` is the source of truth for profile defaults, so roadmap wording that refers broadly to nano service behaviour does not override CU-level profile defaults.
- The merged type-system/base-info work in `specs/069-nano-type-system/` satisfies roadmap dependency 005 for the purposes of planning item 006.
- Existing generated artifact workflow remains the mechanism for updating Kconfig, defconfigs, claim maps, and conformance documentation.
- No CTT verification is available in this feature; all conformance statements are internal profile-targeting claims until external CTT evidence is produced.
