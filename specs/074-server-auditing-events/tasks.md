<!-- markdownlint-disable MD013 -->

# Tasks: Server-Emitted, Client-Observable AuditEvents

**Input**: Design docs in `specs/074-server-auditing-events/`
**Prerequisites**: spec.md, plan.md, research.md, data-model.md, quickstart.md

**OPC UA Normative References**: OPC-10000-3 §6.4 (AuditEvents), §5.4 (EventNotifier); OPC-10000-5 §6.4.3 (AuditEventType i=2052), §6.4.6/6.4.8/6.4.10/6.4.25 (subtypes 2060/2071/2075/2100), §5.4 (BaseEventType); OPC-10000-4 §5.6/§5.7/§5.11.4.

**Organization**: test-first per Constitution IV — every wire-visible change lands a failing test first. Grouped by user story (US1 = client observes AuditEvents; US2 = honest + gated).

## Phase 1: Setup and baseline

- [ ] T001 Record pre-change ARM `.text` baseline for the auditing-enabled profiles (full/standard) and confirm nano compiles auditing out, via `scripts/measure_size.sh` — for the compile-out (C4) and cost (SC-003) checks.
- [ ] T002 Confirm the coupling: re-verify `mu_server_trigger_event` signature + `mu_event_notification_t`/`mu_event_fields_t` shape (notification.c) and the EventFilter SELECT field paths a client uses, so the audit→event adapter emits fields the filter can select.

## Phase 2: EventNotifier attribute (reconciles CU 3194)

- [ ] T003 [US1] RED unit test in `tests/unit/test_read_attribute.c` (or test_discovery_endpoint): reading the Server Object (`i=2253`) `EventNotifier` attribute (id 12) returns Byte `0x01` (SubscribeToEvents); a non-notifier node returns `0x00` (not `Bad_AttributeIdInvalid`).
- [ ] T004 [US1] Add `MU_ATTRIBUTEID_EVENTNOTIFIER` case to `read_attribute.c` returning the node's `event_notifier` byte (default 0).
- [ ] T005 [US1] Set `.event_notifier = 0x01` on the Server Object in `base_nodes.c` (gated with the events/auditing surface). GREEN T003.

## Phase 2b: Event-field-model extension (full-field AuditEvents)

*Added after the implementation probe (research.md Decision 6-7): the pipeline
carries only 5 base fields; full-field AuditEvents require extending it.*

- [ ] T005a [US1] Extend `MU_EVENT_FIELD_*` (event_filter.h) with STATUS,
  ACTIONTIMESTAMP, SERVERID, CLIENTAUDITENTRYID, CLIENTUSERID, ATTRIBUTEID,
  OLDVALUE, NEWVALUE, SECURECHANNELID, SESSIONID.
- [ ] T005b [US1] Extend `mu_event_notification_t` (types.h) with an audit payload
  under `#if MUC_OPCUA_CU_AUDITING`: source_node, status, action_timestamp,
  bounded inline strings (`MU_AUDIT_STR_MAX`) for server_id/client_user_id/
  client_audit_entry_id/secure_channel_id, session_id, attribute_id, and a scalar
  old/new value pair. Add `MU_AUDIT_STR_MAX` capacity.
- [ ] T005c [US1] RED unit test: SELECT of the new audit fields resolves the
  carried value (not Null); non-audit events still resolve base fields only.
- [ ] T005d [US1] Extend the notification.c SELECT resolver with the new enum
  cases (from the audit payload) and `filter_reader.c` SELECT-path parsing to map
  AuditEvent BrowseName paths to the new enums. GREEN T005c.

## Phase 3: US1 — audit→event routing + emission (the core)

### RED (tests first)

- [ ] T006 [US1] E2E RED in `tests/integration/test_event_notifications.c`: subscribe a MonitoredItem to Server `EventNotifier` with an EventFilter selecting AuditEvent fields; assert (initially failing) that after an attribute Write an `AuditWriteUpdateEventType` (i=2100) EventFieldList is published with EventType/SourceNode/Time/Status + AttributeId/Old/New.
- [ ] T007 [P] [US1] Unit RED in `tests/unit/test_audit_events.c`: the audit→event adapter maps each `mu_audit_event_t` variant to a `mu_event_notification_t` with the correct EventType NodeId (2060/2071/2075/2100) and base+audit field values (incl. Status=false path).

### GREEN (implementation)

- [ ] T008 [US1] Add the audit→event adapter in `auditing/audit_events.c`: build `mu_event_notification_t` from `mu_audit_event_t` (EventType per variant, base+audit+subtype fields per data-model.md) and call `mu_server_trigger_event` from `mu_raise_audit_event` (in addition to callbacks). GREEN T007.
- [ ] T009 [US1] Emit AuditWriteUpdateEvent at Write completion (`attribute_handler.c` / `cu/.../attribute_write/write.c`) with old/new value + attribute id. GREEN T006.
- [ ] T010 [US1] Emit AuditOpenSecureChannelEvent on OPN (`secure_channel.c`) — success and failure (Status), SecureChannelId.
- [ ] T011 [US1] Emit AuditCreateSessionEvent (`create_session.c`) with SessionId.
- [ ] T012 [US1] Emit AuditActivateSessionEvent (`activate_session.c`) — success and failure, SessionId + user; add E2E assertions for channel/session AuditEvents mirroring T006.
- [ ] T013 [US1] Verify FR-006: a rejected ActivateSession still emits an AuditEvent with `Status=false` (E2E).

## Phase 4: US2 — gating, honesty, reconciliation

- [ ] T014 [US2] Ensure all emission + audit→event routing is under `MUC_OPCUA_CU_AUDITING`; add/confirm a gating assertion that a nano build defines it off and links no audit-emit/routing code; `scripts/test_profile_gating.sh`.
- [ ] T015 [US2] Reconcile the manifest: `satisfied_by` (with the new emit→observe tests as evidence) for `opc_cu_2422`, `opc_cu_3968`, `opc_cu_3228` → `opc_cu_auditing`; reconcile `opc_cu_3194` (EventNotifier) → an events/read alias; re-annotate `opc_cu_auditing` notes to emit+observe reality; leave 5213 (deferred) / 3224/3226/3230 (unclaimed). Update `specs/073-.../reconciliation-log.md`.
- [ ] T016 [US2] Regenerate artifacts + `validate.py --all`; confirm reconciled counts climb and drift-free.

## Phase 5: Verification + size

- [ ] T017 Full per-profile CTest (nano/micro/embedded/standard/full) + pr-check; pytest; `test_profile_gating.sh`; `git diff --check`; format-check.
- [ ] T018 Measure auditing `.text` cost where enabled and confirm 0 delta on nano (auditing off); record in `docs/size/feature-size-ledger.md` (SC-003).

## Dependencies

- Phase 2 (EventNotifier) before Phase 3 (a client needs a readable notifier to subscribe).
- Within US1: RED (T006/T007) before GREEN (T008–T013); T008 (adapter) before the emission sites can be observed.
- Phase 4 after US1 GREEN; Phase 5 last.

## FR-008 (resolved, see research.md Decision 5)

Claimable after this feature: **2422, 3968, 3228** (+ **3194** EventNotifier).
Deferred: **5213** (no connection-close audit type). Unclaimed: **3224/3226/3230**
(no NodeManagement/History/Method audit emit sites).
