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
