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
