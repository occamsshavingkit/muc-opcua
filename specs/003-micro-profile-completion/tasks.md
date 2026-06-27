# Tasks: Micro Embedded Device 2017 Server Profile Completion

**Input**: [spec.md](spec.md)
**Target Profile**: Micro Embedded Device 2017 Server Profile (Nano + the Embedded
Data Change Subscription Server Facet + ≥2 sessions).

**Ownership tags**: `[Claude-test]` = Claude writes the failing test first;
`[Codex-impl]` = Codex writes production code to pass it (no test/CMake/spec/doc edits,
no commit/push); `[Claude]` = Claude-only (design contract, docs, validation).

**OPC Reference Rule**: every protocol task cites the exact OPC UA section.
**Task Size Rule**: each `[Codex-impl]` task is one reviewable PR-sized diff.

Conventions: Subscription Service Set is OPC 10000-4 §5.14; MonitoredItem Service Set
is §5.13; MonitoringParameters §7.21; DataChangeFilter §7.17; NotificationMessage /
DataChangeNotification §7.20. Service handlers live in `src/core/service_dispatch.c`
with the gated `g_supported_services[]` table; the engine lives in new
`src/services/subscription.{c,h}`; service IDs in `include/micro_opcua/opcua_ids.h`.
All subscription code is gated by `MICRO_OPCUA_SUBSCRIPTIONS`.

---

## US1 — Subscription lifecycle core (P1)

- [X] T001 [Claude] Design the engine contract: author `src/services/subscription.h`
  (types `mu_subscription_t`, `mu_monitored_item_t`, the `MU_MAX_*` constants, the
  `mu_subscriptions_t` container, and the `mu_subscriptions_tick(server, now_ms)` +
  create/delete entry points), plus the `MICRO_OPCUA_SUBSCRIPTIONS` storage bump in
  `include/micro_opcua/config.h`. Header + sizing only; no logic.
  (OPC refs: OPC 10000-4 §5.14.1.3)
- [X] T002 [Claude-test] Integration test in `tests/integration/test_subscriptions.c`
  (new harness, modeled on `test_view_services.c`): after connect/session/activate,
  a CreateSubscription request returns a CreateSubscriptionResponse with a non-zero
  `subscriptionId`, revised PublishingInterval/LifetimeCount/MaxKeepAliveCount, and
  Good. A second CreateSubscription gets a distinct id; the (default 2)+1‑th returns
  `Bad_TooManySubscriptions`. (OPC refs: OPC 10000-4 §5.14.2)
- [X] T003 [Codex-impl] Implement `CreateSubscription`: the `mu_subscriptions`
  allocate/init in `src/services/subscription.c`, the handler + `g_supported_services[]`
  row (requires_session=true) + dispatch `switch` in `src/core/service_dispatch.c`,
  and the request/response IDs in `opcua_ids.h` (CreateSubscription 787/790 exist).
  Revise PublishingInterval/LifetimeCount/MaxKeepAliveCount to server bounds; assign a
  unique IntegerId; `Bad_TooManySubscriptions` when full. (OPC refs: OPC 10000-4 §5.14.2)

- [X] T004 [Claude-test] Integration test: DeleteSubscriptions removes a created
  subscription (Good per op); deleting an unknown id yields per-op
  `Bad_SubscriptionIdInvalid`; the response service result is Good when at least one op
  succeeds and `Bad_NothingToDo` for an empty list. (OPC refs: OPC 10000-4 §5.14.8)
- [X] T005 [Codex-impl] Implement `DeleteSubscriptions` (free the subscription and its
  MonitoredItems) + handler/table/switch wiring + `DeleteSubscriptionsRequest/Response`
  IDs in `opcua_ids.h`. (OPC refs: OPC 10000-4 §5.14.8)

## US2 — MonitoredItems + data-change sampling (P1)

- [X] T006 [Claude-test] Integration test: CreateMonitoredItems on a subscription for
  `ServerStatus.CurrentTime`(i=2258, Value attribute) returns a MonitoredItemCreateResult
  with Good, a non-zero `monitoredItemId`, and a revised sampling interval; an unknown
  NodeId → `Bad_NodeIdUnknown`; exceeding `MU_MAX_MONITORED_ITEMS` →
  `Bad_TooManyMonitoredItems`. (OPC refs: OPC 10000-4 §5.13.2, §7.21)
- [X] T007 [Codex-impl] Implement `CreateMonitoredItems` (data-change monitoring mode
  only): decode `itemsToCreate[]` (ReadValueId + MonitoringParameters §7.21 + optional
  DataChangeFilter §7.17), bind each to a node/attribute via the address space, take the
  initial sample into `last_value`, assign `monitoredItemId`; operation results per
  §5.13.2.4. Handler/table/switch + `CreateMonitoredItemsRequest/Response` IDs.
  (OPC refs: OPC 10000-4 §5.13.2, §7.17, §7.21)

- [ ] T008 [Claude-test] Unit test for change detection
  (`tests/unit/test_subscription_sampling.c`): driving `mu_subscriptions_tick` with a
  mock value source whose value changes between ticks sets the item's pending-notification
  flag exactly once per change (StatusValue trigger); no change → no pending flag.
  (OPC refs: OPC 10000-4 §5.12.1.6, §7.17.2)
- [ ] T009 [Codex-impl] Implement poll-driven sampling in `mu_subscriptions_tick`:
  sample due REPORTING/SAMPLING items via `mu_resolve_node`+value source, compare to
  `last_value` per the DataChangeFilter trigger, set `has_pending_notification` and
  update `last_value` on change; advance the per-item sampling timer. Call the tick from
  `mu_server_poll`. (OPC refs: OPC 10000-4 §5.12.1.6, §7.17.2)

- [X] T010 [Claude-test] Integration test: DeleteMonitoredItems removes an item (Good);
  unknown id → `Bad_MonitoredItemIdInvalid`; unknown subscription →
  `Bad_SubscriptionIdInvalid`. (OPC refs: OPC 10000-4 §5.13.6)
- [X] T011 [Codex-impl] Implement `DeleteMonitoredItems` + wiring +
  `DeleteMonitoredItemsRequest/Response` IDs. (OPC refs: OPC 10000-4 §5.13.6)

## US3 — Publish / keep-alive / Republish (P1)

- [ ] T012 [Claude-test] Integration test: with a subscription + a monitored item, a
  PublishRequest with no pending data is parked (no immediate response); after a value
  change and enough `mu_server_poll` iterations to cross the publishing interval, a
  PublishResponse arrives carrying a DataChangeNotification with the item's
  `clientHandle` and new value, `moreNotifications=false`, an incrementing
  `sequenceNumber`, and `availableSequenceNumbers`. (OPC refs: OPC 10000-4 §5.14.5, §7.20)
- [ ] T013 [Codex-impl] Implement the Publish request queue + async delivery: park
  requests in `mu_subscriptions`, build/encode DataChangeNotification NotificationMessages
  in `mu_subscriptions_tick` and emit PublishResponses over the owning connection's
  secure channel, retain the message for Republish; `Bad_TooManyPublishRequests` when the
  queue is full. Handler + table/switch (Publish 826/829 exist). This is the one
  asynchronous service — the send/framing path must be reachable from the tick.
  (OPC refs: OPC 10000-4 §5.14.5, §5.14.1.1, §7.20)
- [ ] T014 [Claude-test] Integration test: with no data changes, after
  `max_keep_alive_count` publishing intervals a **keep-alive** PublishResponse (empty
  NotificationMessage, sequence number unchanged) is sent for a parked request.
  (OPC refs: OPC 10000-4 §5.14.1.1, §5.14.1.2)
- [ ] T015 [Codex-impl] Implement keep-alive emission and the keep-alive/lifetime
  counter state machine in `mu_subscriptions_tick`. (OPC refs: OPC 10000-4 §5.14.1.2)

- [ ] T016 [Claude-test] Integration test: SubscriptionAcknowledgements in a PublishRequest
  purge the acknowledged retained message; a Republish for a retained sequence number
  re-sends it, and Republish for a purged/unknown one → `Bad_MessageNotAvailable`.
  (OPC refs: OPC 10000-4 §5.14.5.2, §5.14.6)
- [ ] T017 [Codex-impl] Implement acknowledgement processing + `Republish` handler/wiring
  + `RepublishRequest/Response` IDs. (OPC refs: OPC 10000-4 §5.14.6)

## US4 — Subscription / MonitoredItem management (P2)

- [ ] T018 [Claude-test] Integration tests for ModifySubscription (revised values echoed)
  and SetPublishingMode (publishing disabled → only keep-alives, no data notifications).
  (OPC refs: OPC 10000-4 §5.14.3, §5.14.4)
- [ ] T019 [Codex-impl] Implement `ModifySubscription` + `SetPublishingMode` + wiring +
  IDs. (OPC refs: OPC 10000-4 §5.14.3, §5.14.4)
- [ ] T020 [Claude-test] Integration tests for ModifyMonitoredItems (revised sampling
  interval) and SetMonitoringMode (DISABLED stops sampling/notifications; REPORTING
  resumes). (OPC refs: OPC 10000-4 §5.13.3, §5.13.4)
- [ ] T021 [Codex-impl] Implement `ModifyMonitoredItems` + `SetMonitoringMode` + wiring +
  IDs. (OPC refs: OPC 10000-4 §5.13.3, §5.13.4)

## US5 — At least two sessions (P1, profile-mandatory)

- [ ] T022 [Claude] Design the multi-connection/multi-session refactor: a
  `mu_connection_t` (tcp_connection + secure_channel + rx reassembly + idle clock) and
  `mu_session_t sessions[MU_MAX_SESSIONS]` in `struct mu_server`; document the
  session→connection binding used to route async Publish responses, in
  `src/core/server_internal.h`. (OPC refs: OPC 10000-4 §5.6.2)
- [ ] T023 [Claude-test] Integration test: two independent connections each complete
  Hello/OpenSecureChannel/CreateSession/ActivateSession and hold **simultaneously**
  active sessions with distinct `authenticationToken`s; a Read on each succeeds; the
  `MU_MAX_SESSIONS+1`-th CreateSession → `Bad_TooManySessions`. (OPC refs: OPC 10000-4 §5.6.2)
- [ ] T024 [Codex-impl] Refactor the connection/session state in
  `src/core/server.c` + `service_dispatch.c` to fixed-size connection and session
  arrays; accept/poll over all connection slots; CreateSession picks a free slot
  (`Bad_TooManySessions` when full); resolve sessions by `auth_token`; free a
  connection's sessions/subscriptions on close. (OPC refs: OPC 10000-4 §5.6.2, §6.x)
- [ ] T025 [Claude-test] Integration test: a subscription created on session A is not
  visible to / operable from session B (`Bad_SubscriptionIdInvalid`), and an async
  Publish response routes back to the correct connection. (OPC refs: OPC 10000-4 §5.14.1.3)

## US6 — Profile wiring, conformance docs, validation (P1)

- [ ] T026 [Codex-impl] Add the `MICRO_OPCUA_SUBSCRIPTIONS` CMake option (default ON)
  and gate `src/services/subscription.c` + the definition in `src/CMakeLists.txt`;
  guard all engine code under `#if MICRO_OPCUA_SUBSCRIPTIONS`. (OPC refs: n/a — build)
- [ ] T027 [Claude] Update `Makefile` so `make micro` enables
  `-DMICRO_OPCUA_SUBSCRIPTIONS=ON` (distinct from `make nano`), and `make nano`
  explicitly OFF; update target docs. (OPC refs: n/a — build)
- [ ] T028 [Claude] Update `docs/conformance/{status,services}.md` and add
  `docs/conformance/profile-micro.md` (subscription/MonitoredItem CUs → Implemented,
  profile-targeting wording per the conformance-doc test rules); add the new sources to
  `docs/traceability/files-to-sections.md`. (OPC refs: OPC 10000-7 Micro profile)
- [ ] T029 [Claude] Full-surface validation: `-Werror` build, full ctest, ASan+UBSan,
  the dotnet-interop job, and a `sizeof(struct mu_server) <= MU_SERVER_STORAGE_BYTES`
  static assert check. (OPC refs: n/a — validation)
