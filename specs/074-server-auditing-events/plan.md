<!-- markdownlint-disable MD013 -->

# Implementation Plan: Server-Emitted, Client-Observable AuditEvents

**Branch**: `074-server-auditing-events` | **Date**: 2026-07-16 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/074-server-auditing-events/spec.md`

## Summary

Make OPC UA Auditing genuinely conformant by joining the two halves that already exist independently: the audit infrastructure (`mu_raise_audit_event` + four typed `mu_audit_event_t` variants) and the event-notification pipeline (`mu_server_trigger_event`, proven E2E for alarm events). The four modelled actions (OpenSecureChannel, CreateSession, ActivateSession, attribute Write) emit their grounded AuditEvent subtype (i=2060/2071/2075/2100 under AuditEventType i=2052), which is routed into the event pipeline and delivered to any MonitoredItem watching the Server Object's `EventNotifier` attribute. The Server Object exposes a readable `EventNotifier` (id 12) with `SubscribeToEvents` set — closing CU 3194. Gated by `MUC_OPCUA_CU_AUDITING`; the host-callback path is retained but is no longer the conformance mechanism. Reconciles `opc_cu_auditing` (honestly) plus CUs 2422/3968/3228/3194.

## Technical Context

**Language/Version**: C11 core; Python 3 for manifest/validation.
**Primary Dependencies**: existing event pipeline (`notification.c`), EventFilter machinery (`filter_reader.c`/`event_filter.c`), audit infra (`audit_events.c`/`audit.h`), address space (`base_nodes.c`), CMake/Kconfig gating, Unity.
**Storage**: static/caller-owned; AuditEvent construction reuses existing bounded event-field buffers (no-heap).
**Testing**: unit (EventNotifier read, audit→event field mapping) + integration (E2E: subscribe to Server EventNotifier → perform auditable action → assert published AuditEvent EventFieldList), per-profile gating, size measurement.
**Target Platform**: host tests + embedded profiles that enable auditing.
**Project Type**: Embedded OPC UA C library.
**Performance Goals**: no hot-path heap; audit emission is off the protocol hot path (post-action); zero cost when `MUC_OPCUA_CU_AUDITING` off.
**Constraints**: emission gated by `MUC_OPCUA_CU_AUDITING`; compiles out with no residual `.text` when off; AuditEvent construction reuses bounded event buffers; wire-visible tests (non-circular); NodeIds grounded via opc-ua-reference (research.md).
**Scale/Scope**: 4 emission sites + 1 audit→event adapter + EventNotifier attribute (read case + Server node bit) + manifest/doc reconciliation. No new services.
**OPC UA Normative References**: OPC-10000-3 §6.4 (AuditEvents), §5.4 (EventNotifier attribute); OPC-10000-5 §6.4.3 (AuditEventType i=2052), §6.4.6/6.4.8/6.4.10/6.4.25 (the four subtypes), §5.4 (BaseEventType fields); OPC-10000-4 §5.6/§5.7/§5.11.4 (the audited services).
**Target OPC UA Profile/Conformance Units**: reconciles `opc_cu_auditing`, `opc_cu_2422`, `opc_cu_3968`, `opc_cu_3228`, `opc_cu_3194`; defers `opc_cu_5213`; leaves `3224/3226/3230` unclaimed.
**Conformance Status Target**: profile-targeting, not CTT-verified.

## Embedded Size Budget

*Revised after the implementation probe (research.md Decision 6-7): the event
pipeline carries only 5 base fields, so full-field AuditEvents require extending
the event-field model — the RAM cost below is now the dominant cost.*

**Flash Impact**: audit→event adapter + field construction, the new
`MU_EVENT_FIELD_*` SELECT-resolver cases + filter-parser mappings, 4 emission
sites, EventNotifier read case, Server node bit. Estimated ~1–2 KB, **only** when
`MUC_OPCUA_CU_AUDITING` on; **0** otherwise (compiles out).
**RAM Impact**: **the material cost.** The audit payload enlarges the queued
`mu_event_notification_t` (bounded inline strings `MU_AUDIT_STR_MAX` + scalar
old/new), multiplied by `MU_MAX_EVENT_QUEUE_SIZE (8) * MU_INTERN_MAX_SUBSCRIPTIONS`
(up to 50–100). The audit payload is under `#if MUC_OPCUA_CU_AUDITING` so
non-auditing profiles pay **0**; auditing profiles pay `delta * 8 * subs`, to be
**measured** (SC-003) — may motivate a smaller audit-profile event-queue size or
subscription cap.
**Heap Use**: none (bounded inline capture; scalar write values only, arrays→Null).
**Static Tables Added**: none.
**Transport Buffers**: unchanged.
**Crypto Dependency Impact**: none.

## Constitution Check

*GATE: pass before Phase 0; re-check after Phase 1.*

- **Spec Fidelity (I)**: PASS. Every AuditEvent subtype + EventNotifier semantics grounded to exact OPC-10000-3/5 sections and NodeIds (research.md). Failed actions still audit with `Status=false`.
- **Embedded C Core (II)**: PASS. C11 core; audit emission is post-action, off the hot path; no platform leakage.
- **Memory Model (III)**: PASS. No hot-path heap; reuses bounded event buffers.
- **Minimal Surface (III)**: PASS. Reuses the existing event pipeline + EventFilter; adds only the audit→event adapter, 4 call sites, and the EventNotifier attribute. Fully gated.
- **Profile Research (I)**: PASS. Auditing is an optional facet; gated by `MUC_OPCUA_CU_AUDITING`, off for nano/minimal.
- **Correctness Gates (IV)**: PASS. Test-first, wire-visible: E2E asserts the published AuditEvent EventFieldList (non-circular); EventNotifier read case unit-tested; per-profile gating asserts compile-out.
- **Security Honesty (V)**: **PASS — the point of the feature.** Removes the `opc_cu_auditing` over-claim by making auditing emit+observe real; reconciles claims only where backed by emit→observe tests; documents the deferred/unclaimed audit surfaces.
- **Toolchain Discipline (VI)**: PASS. CMake/Kconfig gating; generated-artifact drift checks; size ledger; per-profile CI.
- **Size Discipline (VII)**: PASS. Zero cost when off; measured + recorded when on; no removal of status handling/bounds/tests.

## Project Structure

### Documentation (this feature)

```text
specs/074-server-auditing-events/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
└── tasks.md
```

### Source Code (repository root)

```text
include/muc_opcua/services/audit.h                 # audit event struct (existing)
src/cu/core_2022_server/auditing/audit_events.c    # + audit->event adapter (build mu_event_notification_t, call trigger_event)
src/cu/core_2022_server/subscription/basic/notification.c   # mu_server_trigger_event (reused)
src/core/service_dispatch/attribute_read/read_attribute.c   # + MU_ATTRIBUTEID_EVENTNOTIFIER case
src/address_space/base_nodes.c                     # Server Object .event_notifier = SubscribeToEvents
src/services/secure_channel.c                      # emit AuditOpenSecureChannelEvent on OPN
src/core/service_dispatch/create_session.c         # emit AuditCreateSessionEvent
src/core/service_dispatch/activate_session.c       # emit AuditActivateSessionEvent (success + failure)
src/core/service_dispatch/attribute_handler.c / cu/.../attribute_write/write.c   # emit AuditWriteUpdateEvent
tests/unit/{test_audit_events,test_discovery_endpoint,test_read_attribute}.c
tests/integration/test_event_notifications.c       # + audit E2E scenarios
profiles/opcua-profile-manifest.yaml + docs/       # reconcile opc_cu_auditing/2422/3968/3228/3194
```

**Structure Decision**: reuse the event pipeline and EventFilter; do not build a parallel audit-delivery path. Add one audit→event adapter, the four emission calls at service completion points, and the EventNotifier attribute (read case + Server node bit). Keep the host-callback path for integrators. All emission gated by `MUC_OPCUA_CU_AUDITING`.

## Design Artifacts

- Research (grounded NodeIds/fields + FR-008 resolution): [research.md](./research.md)
- Data model (AuditEvent field mapping, EventNotifier): [data-model.md](./data-model.md)
- Validation path: [quickstart.md](./quickstart.md)
- Tasks: [tasks.md](./tasks.md)

## Phase 0: Research

See [research.md](./research.md). Subtype NodeIds/fields grounded; FR-008 resolved (claimable = 2422/3968/3228 + 3194; defer 5213; unclaimed 3224/3226/3230). No blocking unknowns.

## Phase 1: Design & Contracts

See [data-model.md](./data-model.md). Contracts: (C1) reading Server EventNotifier returns SubscribeToEvents; (C2) each of the 4 actions delivers its grounded AuditEvent subtype with populated base+audit+subtype fields to a subscribed client; (C3) a failed action audits with Status=false; (C4) with auditing off, emission + Server-notifier-bit compile out (0 `.text`).

## Post-Design Constitution Check

PASS (pending Phase 1). Reuses proven infrastructure; gated; test-first wire-visible; makes an over-claim honest. No waiver.

## Complexity Tracking

- **Audit→event field construction**: mapping `mu_audit_event_t` variants to EventFieldLists must match the EventFilter SELECT paths clients use. Mitigation: reuse the existing event-field emission (notification.c :257-311) and assert wire output in E2E tests. Simpler alternative (host-callback only) rejected — it is the over-claim this feature removes.
- **EventNotifier attribute**: minimal (one read case + one node bit); risk is forgetting non-notifier nodes must return 0 — covered by a read-attribute unit test.
