<!-- markdownlint-disable MD013 -->

# CU Reconciliation Log (spec 073 long tail)

Per-facet grounding of the fine-grained OPC UA Server conformance units against
the muc-opcua implementation, reconciled via `satisfied_by` links to our coarse
feature aliases (or left honestly unimplemented). Each claim requires **code +
a backing test**; behavioural mismatches and untested/partial paths are **not**
claimed. Numbers refer to `docs/conformance/completion.md`.

## 2026-07-15 — Standard DataChange Subscription 2022 Server Facet (opc_id 1324)

Aliases used: `opc_cu_subscription_basic` (`MUC_OPCUA_CU_SUBSCRIPTION_BASIC`),
`opc_cu_subscription_standard` (`MUC_OPCUA_CU_SUBSCRIPTION_STANDARD`).

Facet: 16 required + 1 optional. Reconciled **11/16 required** (was 1/16).
Overall reconciled required 21 → 31.

### Claimed (satisfied_by)

| CU | Name | Alias | Evidence |
| --- | --- | --- | --- |
| 2928 | Monitored Items Deadband Filter | basic | `subscription_monitor.c` abs-deadband compare; `test_subscriptions::test_monitored_item_absolute_deadband`, `test_subscription_deadband` |
| 2940 | GetMonitoredItems Method | standard | `standard/dispatch_method.c` (method 11492); `test_method_call::test_get_monitored_items_returns_server_and_client_handles` |
| 2963 | Monitor Basic | basic | `basic/monitored_items.c` + `subscription_monitor.c`; `test_subscriptions::test_create/modify/delete_monitored_items`, `test_set_monitoring_mode` |
| 3146 | Monitor Triggering | standard | `standard/triggering.c` + link storage; `test_subscriptions::test_set_triggering`, capacity/error tests |
| 3532 | Monitor Queueing | standard | `basic/monitored_items.c` clamp + `subscription_monitor.c` overflow bit; `test_subscriptions::test_monitored_item_queue_overflow`, capacity tests |
| 3534 | Subscription Multiple | basic | `subscription.h` `subscriptions[]` `_Static_assert(>=5)`; `test_subscriptions_capacity`, `test_subscription_session_isolation` |
| 3535 | Subscription Retransmission Queue | basic | `basic/retransmit.c` + `handle_republish`; `test_subscriptions::test_republish_and_acknowledge`. **Caveat**: single-slot store (most-recent message), profile-targeting minimal capacity — may not meet CTT multi-message republish depth. |
| 3544 | ResendData Method | standard | `standard/dispatch_method.c` (method 12873) + `mu_subscription_request_resend_data`; `test_method_call::test_resend_data_reissues_current_values_on_next_publish` |
| 3913 | Subscription Publish Basic | basic | `basic/publish_due.c` + `tick.c`; `test_subscriptions::test_publish_delivers_data_change`, `test_publish_keep_alive`, `test_subscription_publish` |
| 5207 | Monitor Items 2 | basic | `basic/monitored_items.c` alloc; `subscription.h` `monitored_items[]` `_Static_assert(>=500)`; `test_subscriptions_capacity` |
| 3727 | Subscription Basic | basic | (already reconciled before this increment) |

### NOT claimed (grounded reasons — honest under-claim)

| CU | Name | Reason |
| --- | --- | --- |
| 3143 | PublishRequest Queue Overflow | Behavioural mismatch: the CU requires discarding the **oldest** queued PublishRequest with the error; our code returns `Bad_TooManyPublishRequests` on the **incoming** request (`tick.c`). Different observable behaviour → not conformant. |
| 3196 | Base Info Fixed SamplingInterval (opt) | The `SamplingIntervalDiagnosticsArray` diagnostic node does not exist in the address space (only a disabled stub). |
| 3911 | ServerCapabilities Subscriptions | Only `MaxMonitoredItemsPerCall` is exposed; `MaxSubscriptions`, `MaxMonitoredItems`, `MaxSubscriptionsPerSession`, `MaxMonitoredItemsPerSubscription`, `AggregateFunctions` nodes are absent. |
| 3922 | Base Info SemanticChange Bit | The SemanticsChanged status bit (0x4000) is never set (only a disabled stub exists; the overflow InfoBit is a different bit). |
| 4055 | ServerCapabilities MaxMonitoredItemsQueueSize | No `MaxMonitoredItemsQueueSize` node in the address space (disabled stub only). |
| 5208 | Monitor Value Change V2 | Value-change monitoring works, but the CU's required IndexRange element selection on a monitored item is decoded-and-skipped, never applied to slice array values, and untested. |

### Follow-ups surfaced

- 3143: implement oldest-request-discard semantics (OPC-10000-4 §5.13.5) to claim it.
- 3535: grow the retransmission store to a multi-message queue for CTT-level republish.
- 3911/4055/3922/3196: add the missing ServerCapabilities / diagnostics / status-bit surface.
- 5208: apply monitored-item IndexRange slicing + test.

## 2026-07-15 — Auditing 2022 Server Facet (opc_id 1328)

30 CUs (18 required + 12 optional). This facet reuses the subscription CUs
already reconciled under 1324 (2963, 3534, 3535, 3727, 3913, 5207). New work:
the auditing / event / base-info CUs. Reconciled **5 more** (facet 11/18
required; overall reconciled required 31 → 36).

### Claimed (satisfied_by) — added as granular manifest entries

| CU | Name | Alias | Evidence |
| --- | --- | --- | --- |
| 2318 | Monitor QueueSize_ServerMax | subscription_standard | `subscription_monitor.c` clamps requested QueueSize to compiled max; `test_subscriptions_capacity` |
| 2515 | Address Space Events 2 | events | `notification.c` `mu_server_trigger_event` queues + delivers events; `test_event_notifications::test_alarm_event_generation_and_publishing` |
| 2536 | Base Info ContentFilter | event_filter_where | `event_filter.c` `mu_where_clause_eval` full operator set; `test_event_filter_where` |
| 3150 | Monitor Events | events | `notification.c` MonitoredItem on EventNotifier attr delivers EventFieldLists E2E; `test_event_notifications` |
| 4030 | Monitor Complex Event Filter | event_filter_where | `filter_reader.c` SELECT + `event_filter.c` WHERE, unsupported-operator rejection; `test_event_notifications` (E2E) |

### NOT claimed — grounded reasons

- **Auditing CUs (2422, 3968, 5213, 3228, 3224, 3226, 3230)**: the audit subsystem
  (`auditing/audit_events.c` `mu_raise_audit_event`) is a **tested dispatch stub with
  zero service integration** — no session/secure-channel/write/method/history/node-
  management handler ever raises an audit event (only 4 audit types defined, none
  emitted). Infra + isolated unit tests exist (PARTIAL), but the server does not
  actually emit AuditEvents, so none of the granular auditing CUs are claimed.
- **3194 Base Info Events Capabilities**: the EventNotifier attribute is never
  exposed — `read_attribute.c` has no `MU_ATTRIBUTEID_EVENTNOTIFIER` case and no node
  sets `.event_notifier`; a client cannot discover the Server as an event source.
- **3206 EventQueueOverflow EventType**: overflow silently discards the oldest event;
  no `EventQueueOverflowEventType` is inserted (behavioural mismatch).
- **3546 LocalTime Events**: the LocalTime event field is name-resolvable but always
  emitted Null (behavioural mismatch).
- **3199 System Status**: ServerStatus/State nodes exist and are read, but no
  system-status *event* generation; CU intent (node vs event) ambiguous — deferred.
- **Absent**: 2747, 2822, 2978, 3224, 3226, 3230, 4427 (N/A client), 5578 — no code.

### Surfaced concern (follow-up)

`opc_cu_auditing` (`MUC_OPCUA_CU_AUDITING`) is currently **claimed**, but the audit
subsystem has no service integration (no service call raises an audit event). Its
claim appears **over-stated** and should be reviewed — either wire audit-event
emission into the service handlers, or downgrade the alias. Out of scope for this
reconciliation increment; flagged here so it is not lost.

## 2026-07-15 — Node Management 2022 Server Facet (opc_id 1329)

54 CUs (15 required + 39 optional). Reconciled the 4 core NodeManagement
**service** CUs (facet 0/15 → 5/15 required incl. 2536 from 1328; overall
reconciled required 36 → 40). Added as granular manifest entries under
`service_nodemanagement` (`MUC_OPCUA_CU_NODEMANAGEMENT`, full only):

| CU | Name | Evidence |
| --- | --- | --- |
| 2380 | Node Management Add Node | `dispatch_node_mgmt.c` handle_add_nodes → mu_add_nodes_process; `test_node_management` (decode/encode, duplicate-NodeId) |
| 2394 | Node Management Delete Node | handle_delete_nodes → mu_delete_nodes_process; `test_node_management` |
| 2939 | Node Management Add Ref | handle_add_references → mu_add_references_process; `test_node_management` |
| 3153 | Node Management Delete Ref | handle_delete_references → mu_delete_references_process; `test_node_management` (incl. Bad_NotFound) |

### NOT claimed / deferred

- **2489 Node Management Capabilities**: no NodeManagement ServerCapabilities node
  in the address space.
- **~44 "Base Info X DataType" companion CUs** (2423/2481/2482/2500/2513/2514/…):
  these require exposing specific companion-spec DataType/ReferenceType nodes
  (RationalNumber, EUInformation, Audio, Spatial, ISA-95 relations, etc.) in the
  address space. As a minimal profile-targeting server we do not expose most of
  these; deferred as a batch — each needs per-node address-space grounding and most
  are genuinely absent. Core-type CUs (3185/3188/3189 base type folders/ServerType)
  are also deferred pending per-node grounding.

## 2026-07-16 — Auditing made real (spec 074), reconciles 4 CUs

Spec 074 (server-emitted, client-observable AuditEvents) turned the audit
subsystem from a host-callback stub into genuine OPC UA auditing (emit + observe
via the Server EventNotifier). That makes previously-unclaimable CUs honest:

| CU | Name | Alias | Now backed by |
| --- | --- | --- | --- |
| 3194 | Base Info Events Capabilities | opc_cu_events | Server EventNotifier readable; test_read_service::test_read_service_eventnotifier |
| 2422 | Auditing Secure Communication | opc_cu_auditing | AuditOpenSecureChannelEvent emitted+observable; test_event_notifications |
| 3968 | Auditing Services | opc_cu_auditing | Create/Activate/Write AuditEvents emitted+observable; test_event_notifications |
| 3228 | Auditing Write | opc_cu_auditing | AuditWriteUpdateEvent emitted+observable; test_event_notifications |

`opc_cu_auditing` re-annotated + test_event_notifications added as backing (the
emit->observe proof), resolving the over-claim flagged in the 1328 increment.
Overall reconciled required 40 -> 43. Still deferred: 5213 (no connection-close
audit type), 3224/3226/3230 (no NodeManagement/History/Method audit emit sites);
documented 074 follow-ups: failure-path (Status=false) auditing, OldValue capture,
SessionId/SecureChannelId string population.

## 2026-07-16 — Publish-queue overflow discards the oldest request (CU 3143)

Closes the behavioural mismatch flagged in the 1324 increment. OPC-10000-4
**§5.14.5.1** (verified against the reference, not §5.13.5 as the earlier follow-up
mis-cited): "When a Server receives a new Publish request that exceeds its limit it
shall **de-queue the oldest** Publish request and return a response with the result
set to Bad_TooManyPublishRequests." We previously rejected the **incoming** request
(`Bad_TooManyPublishRequests` on the new one) — a different observable behaviour.

Fix: on overflow, `handle_publish` (`subscription_crud.c`) now calls the new
`publish_request_evict_oldest` (`publish_due.c`), which de-queues the session's
oldest parked request (`publish_request_dequeue`, already FIFO-by-`enqueued_ms`) and
answers **that** request with a `Bad_TooManyPublishRequests` ServiceFault via the same
`mu_write_service_fault` + `mu_server_emit_message` path the timeout-eviction already
uses; the incoming request then parks in the freed slot. If the session has no parked
request to evict (queue full of another session), we fall back to returning the error
on the incoming request. The low-level `mu_publish_request_enqueue` primitive is
unchanged (still returns the error when full) — the discard policy lives at the
service layer.

| CU | Name | Alias | Now backed by |
| --- | --- | --- | --- |
| 3143 | Subscription PublishRequest Queue Overflow | opc_cu_subscription_basic | `publish_request_evict_oldest` (publish_due.c) + handle_publish wiring; test_subscriptions_capacity::test_publish_queue_overflow_evicts_oldest_and_parks_newest |

Facet 1324 (Standard DataChange Subscription) required 11/16 -> 12/16. Overall
reconciled required 44 -> 45 (one unique CU). The same shared CU also advances every
facet that lists 3143 — 1328 Auditing 14/18 -> 15/18, the A&C alarm facets, and the
Embedded/Standard 2022 profiles — but overall counts it once. Remaining 1324
required: 3911/4055 (ServerCapabilities nodes), 3922 (SemanticChange status bit),
5208 (monitored-item IndexRange slicing).

## 2026-07-16 — DataChange Subscription facet completed: 16/16 required (CUs 5208, 3911, 4055, 3922)

The remaining four required CUs of facet 1324, each with real code + a backing test.
All are gated on `MUC_OPCUA_CU_SUBSCRIPTION_BASIC` and aliased to
`opc_cu_subscription_basic` (off for nano, on for micro/embedded/standard/full).

| CU | Name | Now backed by |
| --- | --- | --- |
| 5208 | Monitor Value Change V2 | IndexRange parsed at MonitoredItem create (`subscription_helpers.c`), malformed → Bad_IndexRangeInvalid, applied to array samples via `apply_numeric_index_range` in `read_monitored_item_value`; `test_monitored_index_range` |
| 3911 | Base Info ServerCapabilities Subscriptions | `AggregateFunctions`(2997), `MaxSubscriptions`(24096), `MaxMonitoredItems`(24097), `MaxSubscriptionsPerSession`(24098), `MaxMonitoredItemsPerSubscription`(24104) added to both `base_nodes.c` tables; `test_operation_limits::test_subscription_capability_nodes_resolve` |
| 4055 | Base Info MaxMonitoredItemsQueueSize | `MaxMonitoredItemsQueueSize`(31916) node; same resolve test |
| 3922 | Base Info SemanticChange Bit | `mu_server_signal_semantic_change` latches the item; next DataChange Notification sets StatusCode bit 14 (0x4000) then the one-shot latch clears; `test_subscriptions::test_publish_semantics_changed_bit` |

Implementation notes:
- **5208**: the read-path `apply_index_range` helpers (`variant_elem_size`,
  `parse_numeric_range`) were previously gated on `MUC_OPCUA_CU_MULTI_CHUNK` (off on
  standard); the gate was widened to `|| MUC_OPCUA_CU_SUBSCRIPTION_BASIC` and a numeric
  core `apply_numeric_index_range` was extracted so read and sampling share one slicer.
  Per-item cost: two `opcua_int32_t` (parsed start/end, `-1` = no range).
- **3911/4055**: the base address space is binary-searched with **no linear fallback**,
  so it must stay strictly ascending by NodeId. The existing
  `test_base_address_space_is_sorted` guard (runs on every profile) confirms the new
  nodes preserve the ordering; `AggregateFunctions`(2997) needs two guarded copies in
  the type-system table (DataAccess-on vs -off) so exactly one is emitted in sorted
  position. nano excludes all of these via the compile gate.
- **3922**: the SemanticsChanged trigger is integrator-driven (the library owns no
  runtime EU/EURange changes in its static address space); providing the ABI +
  emitting the bit + clearing it one-shot is the honest, spec-conformant mechanism.

Facet 1324 required 12/16 -> **16/16** (complete). Overall reconciled required
45 -> 49 (four unique CUs). The shared subscription/base-info CUs also lift the
Auditing (1328), A&C alarm, and Embedded/Standard 2022 facet tallies that list them.

## 2026-07-16 — Auditing 074 follow-ups: OldValue, failure-path, SessionId/SecureChannelId

Deepens the already-claimed AuditEvents (2422 SecureChannel, 3968 Services, 3228
Write) so they are genuinely conformant, resolving the three follow-ups flagged when
spec 074 first made auditing real. No new CU numbers — this raises the fidelity of
existing claims. All full-profile-only (auditing is full-only).

- **OldValue capture** (attribute_handler.c): the pre-write value is read BEFORE the
  write and carried in AuditWriteUpdateEvent.OldValue (was always Null).
- **Failure-path auditing** (Status=false): rejected writes now emit an
  AuditWriteUpdateEvent with Status=false (was success-only); ActivateSession emits on
  failed activation (bad user identity); CreateSession emits on rejection
  (capacity/validation). An auditor now sees failed/rejected operations, not just
  successes — the security-relevant half.
- **SecureChannelId** (osc_handler.c): AuditOpenSecureChannelEvent carries the numeric
  channel id formatted as a String (was Null).
- **SessionId** (activate_session.c): AuditActivateSessionEvent carries the SessionId
  NodeId (was unset; CreateSession already set it).

Backing test: `tests/integration/test_write_service.c` gains an audit callback that
captures every raised event while the harness drives OPN + CreateSession +
ActivateSession + a successful write (1001, 10→42) + rejected writes; it asserts the
successful write's OldValue=10/NewValue=42, a rejected write with Status=false, a
non-empty SecureChannelId on the channel event, and a populated SessionId on the
activate event. Still deferred (new emit sites, not this increment): 3224/3226/3230
(NodeManagement/History/Method audits) and 5213 (connection-close).
