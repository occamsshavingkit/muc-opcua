<!-- markdownlint-disable MD013 -->

# Feature Specification: Server-Emitted, Client-Observable AuditEvents

**Feature Branch**: `074-server-auditing-events`
**Created**: 2026-07-15
**Status**: Draft
**Input**: Spec-073 reconciliation surfaced that `opc_cu_auditing` is over-claimed — the audit subsystem is a host-callback dispatch stub with (a) zero service call sites raising audit events and (b) no path for a client to observe AuditEvents over OPC UA. This feature makes auditing genuinely conformant.

## Context and Grounding

OPC UA Auditing (OPC-10000-3 §6.4 AuditEvents; OPC-10000-5 §6.4.3 `AuditEventType` = `i=2052`) requires the Server to generate **AuditEvents** — Events reported through the Server's event mechanism — whenever an auditable action occurs. A client observes them by creating a **MonitoredItem on the `EventNotifier` attribute** (attribute id 12) of an Object that is an event notifier (at minimum the Server Object `i=2253`), with an EventFilter selecting the AuditEvent fields.

The muc-opcua codebase already has the two halves that must be joined:

- **Audit infrastructure** (`src/cu/core_2022_server/auditing/audit_events.c`, `include/muc_opcua/services/audit.h`): a `mu_audit_event_t` struct with four typed variants — `MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL`, `_CREATE_SESSION`, `_ACTIVATE_SESSION`, `_WRITE_UPDATE` — and `mu_raise_audit_event()`, which currently dispatches **only to host-registered callbacks**. It is never called by any service handler.
- **Event delivery pipeline** (`src/cu/core_2022_server/subscription/basic/notification.c`): `mu_server_trigger_event()` (`:330`) queues an event notification to every MonitoredItem watching `attribute_id == 12` and delivers `EventFieldList`s in Publish. This is proven end-to-end for alarm/condition events (`test_event_notifications`).

The gaps this feature closes (grounded in spec-073 reconciliation-log.md):

1. **Emission**: no service handler calls `mu_raise_audit_event`.
2. **Observability**: audit events reach a host callback, never the OPC UA event pipeline; a client cannot subscribe to them.
3. **EventNotifier attribute** is never exposed — `read_attribute.c` has no `MU_ATTRIBUTEID_EVENTNOTIFIER` case, and no node (incl. the Server Object) sets `.event_notifier` with the `SubscribeToEvents` bit. (This is conformance unit `opc_cu_3194` / catalog CU 3194, currently unclaimed.)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A client observes AuditEvents over OPC UA (Priority: P1)

As an operator auditing a server, I create a Subscription and a MonitoredItem on the Server Object's `EventNotifier` attribute, and I receive an AuditEvent NotificationMessage whenever an auditable action (secure-channel open, session create/activate, attribute write) occurs — with the standard AuditEvent fields populated.

**Why this priority**: This is the entire point — auditing that no client can observe is not OPC UA auditing.

**Independent Test**: Build a profile with auditing, subscribe to the Server EventNotifier with an EventFilter, perform an auditable action, and assert an AuditEvent EventFieldList is published carrying the expected `EventType`, `SourceNode`, `Time`, `Severity`, and audit-specific fields.

**Acceptance Scenarios**:

1. **Given** a subscribed MonitoredItem on Server `EventNotifier` with an EventFilter selecting AuditEvent fields, **When** a client opens a SecureChannel / CreateSession / ActivateSession / writes an attribute, **Then** an AuditEvent of the corresponding subtype is delivered in a Publish response with `Status`, `ActionTimeStamp`, `ServerId`, `ClientAuditEntryId`, `ClientUserId`, and the subtype-specific fields populated per OPC-10000-5 §6.4.3.
2. **Given** the Server Object, **When** a client reads its `EventNotifier` attribute (id 12), **Then** the server returns a Byte with the `SubscribeToEvents` (bit 0) set — advertising the Server as an event source (closes CU 3194).
3. **Given** an auditable action that **fails** (e.g. a rejected ActivateSession), **When** the AuditEvent is generated, **Then** its `Status` field is `false` and the failure is still reported.

---

### User Story 2 - Auditing stays honest and gated (Priority: P1)

As a maintainer, I need the auditing claim to match reality (emit + observe, not a stub) and to compile out cleanly on profiles that do not enable it, so `opc_cu_auditing` is a truthful claim and the size cost is opt-in.

**Why this priority**: The feature exists to remove an over-claim; it must not introduce a new one, and must respect Size Discipline.

**Independent Test**: With auditing disabled the AuditEvent emission and EventNotifier-on-Server code compile out; with it enabled the emission sites, event routing, and reconciled CUs are all present and tested. Manifest/docs reflect the true auditing posture.

**Acceptance Scenarios**:

1. **Given** `MUC_OPCUA_CU_AUDITING` off, **When** the library is built, **Then** no audit emission or audit-event-routing code is linked and the Server EventNotifier need not advertise AuditEvents.
2. **Given** the manifest, **When** `opc_cu_auditing` and the granular Auditing CUs are read, **Then** they reflect emit+observe reality (claimed only where backed by an emit→observe test), and `opc_cu_3194` is reconciled.

## Requirements *(mandatory)*

- **FR-001**: The Server MUST generate an OPC UA AuditEvent (subtype of `AuditEventType` `i=2052`) for each of the four already-modelled auditable actions — OpenSecureChannel, CreateSession, ActivateSession, attribute Write — at the point the action completes (success or failure), grounded to OPC-10000-3 §6.4 and the specific AuditEventType subtypes in OPC-10000-5 §6.4.x.
- **FR-002**: Generated AuditEvents MUST be routed into the existing event-notification pipeline (`mu_server_trigger_event`) so they are delivered to MonitoredItems watching the Server Object's `EventNotifier`, carrying the base Event fields (EventId, EventType, SourceNode, SourceName, Time, ReceiveTime, Message, Severity) and the AuditEvent fields (ActionTimeStamp, Status, ServerId, ClientAuditEntryId, ClientUserId) plus subtype-specific fields.
- **FR-003**: The Server Object (`i=2253`) MUST expose a readable `EventNotifier` attribute (id 12) with the `SubscribeToEvents` bit set; `read_attribute.c` MUST handle `MU_ATTRIBUTEID_EVENTNOTIFIER`. This reconciles `opc_cu_3194`.
- **FR-004**: A client MUST be able to create a MonitoredItem on the Server `EventNotifier`, supply an EventFilter (SELECT + optional WHERE), and receive the AuditEvents; the existing EventFilter SELECT/WHERE machinery MUST apply to AuditEvents.
- **FR-005**: Audit generation MUST be gated by `MUC_OPCUA_CU_AUDITING` and MUST compile out with no residual `.text` when disabled; the retained host-callback `mu_audit_callback_t` path MAY remain for integrators but is NOT the conformance mechanism.
- **FR-006**: Failed auditable actions MUST still emit an AuditEvent with `Status = false` (OPC-10000-3 §6.4.3).
- **FR-007**: The AuditEvent field values MUST be grounded (NodeIds, BrowseNames, field names) against the opc-ua-reference, never memory; tests MUST assert the wire-visible EventFieldList, not just internal state (non-circular).
- **FR-008**: The manifest and generated conformance docs MUST be updated so `opc_cu_auditing` reflects emit+observe reality, the granular Auditing CUs that become genuinely backed are reconciled (`satisfied_by` with emit→observe tests), and `opc_cu_3194` is reconciled. [NEEDS CLARIFICATION during planning: which granular Auditing CUs (2422/3968/5213/3228/3230/3224/3226) become claimable once emit+observe exists — depends on the exact per-CU CTT expectations.]
- **FR-009**: The feature MUST state and measure its flash/RAM impact; the AuditEvent construction MUST reuse the existing event-field buffers and stay within the no-heap hot-path model.

### Key Entities

- **AuditEvent** — an Event instance (subtype of `AuditEventType` i=2052) with base + audit + subtype fields, built from `mu_audit_event_t` and emitted via the event pipeline.
- **Server EventNotifier** — the `EventNotifier` attribute (id 12) on the Server Object advertising it as an event source (`SubscribeToEvents`).
- **Emission sites** — the four service completion points (`secure_channel.c` OPN, `create_session.c`, `activate_session.c`, `attribute_handler.c`/`attribute_write` write) that call the audit-emit entry point.

## Success Criteria *(mandatory)*

- **SC-001**: A client subscribed to the Server EventNotifier receives an AuditEvent for each of the four auditable actions, verified end-to-end against the published EventFieldList.
- **SC-002**: Reading the Server `EventNotifier` attribute returns `SubscribeToEvents`; `opc_cu_3194` reconciles.
- **SC-003**: With auditing disabled the build is unchanged in `.text` (feature compiles out); with it enabled the size cost is measured and recorded.
- **SC-004**: `opc_cu_auditing` and the newly-backed granular Auditing CUs are reconciled with emit→observe tests; `validate.py --all`, `test_profile_gating.sh`, and the per-profile test matrix pass.

## Scope

- **In scope**: emitting AuditEvents for the four existing typed actions; routing them into the event pipeline; exposing the Server EventNotifier attribute; EventFilter over AuditEvents; manifest/doc reconciliation; size + gating evidence.
- **Out of scope**: new AuditEvent subtypes beyond the four modelled (NodeManagement/History/Method audit — remain unclaimed); the full AuditEventType node hierarchy in the address space beyond what a client needs to filter/select; companion-spec audit types (GDS, Onboarding, etc.).

## Dependencies and Assumptions

- Builds on the existing event pipeline (`mu_server_trigger_event`) and EventFilter machinery (proven by `test_event_notifications`).
- Assumes the four `mu_audit_event_t` variants and their fields are the correct starting set (grounded against OPC-10000-5 §6.4.x during planning; subtype NodeIds/field lists to be confirmed via opc-ua-reference).
- Independent of, but complementary to, the spec-073 reconciliation (this feature moves auditing CUs from "not claimed" to genuinely claimable).
