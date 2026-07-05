# Tasks: Missing OPC UA Features

---

## Phase 1: Subscription Lifecycle (US1)

- [x] T001 [US1] Implement session timeout monitoring. In mu_server_poll or similar, check each active session's last_activity time against session_timeout. Auto-close expired sessions. `src/services/session.c`, `src/core/server.c`. Grounding: OPC-10000-4 §5.7.2.1.

- [x] T002 [US1] Implement subscription lifetime expiration. Increment lifetime_counter each publish cycle; when >= max_lifetime_count, delete subscription. `src/services/subscription_publish.c`. Grounding: OPC-10000-4 §5.14.5.

- [x] T003 [US1] Add timeoutHint check to Publish handler. Before processing parked Publish, check if elapsed time > timeoutHint and return timeout. `src/core/dispatch_subscription.c`. Grounding: OPC-10000-4 §7.32.

- [x] T004 [US1] Return Bad_NoSubscription when session has 0 subscriptions and Publish arrives. `src/core/dispatch_subscription.c`. Grounding: OPC-10000-4 §5.14.5.

- [x] T005 [US1] Issue StatusChangeNotification on subscription expiry. Encode StatusChangeNotification in the Publish response when subscription is deleted. `src/services/subscription_publish.c`. Grounding: OPC-10000-4 §5.14.1.4.

- [x] T006 [US1] Implement DeleteClientPublReqQueue on SetPublishingMode/DeleteSubscriptions changes. Clear queued Publish requests when subscriptions change. `src/services/subscription_publish.c`. Grounding: OPC-10000-4 §5.14.1.4.

- [x] T007 [US1] Trigger immediate sample on SetMonitoringMode enable (DISABLED→SAMPLING/REPORTING). `src/core/service_dispatch.c`. Grounding: OPC-10000-4 §5.13.1.3.

---

## Phase 2: Read & Write & Browse (US2+US3)

- [x] T008 [US2] Implement index_range support in Read. Decode NumericRange, extract single element or range from array values. `src/services/read.c`. Grounding: OPC-10000-4 §7.28.

- [x] T009 [US2] Honor maxAge in Read. If maxAge=0, re-read from value source; if >0, use cached value. `src/services/read.c`. Grounding: OPC-10000-4 §5.11.2.

- [x] T010 [US2] Support additional readable attributes: DESCRIPTION, WRITEMASK, USERWRITEMASK, VALUERANK, DATATYPE, ACCESSLEVEL. Add to attribute switch. `src/services/read.c`. Grounding: OPC-10000-3.

- [x] T011 [US3] Preserve SourceTimestamp/ServerTimestamp from written DataValue. When client provides timestamps, pass them through to the write handler. `src/core/dispatch_attribute.c`. Grounding: OPC-10000-4 §5.11.4.

- [x] T012 [US3] Document Write attribute scope: update docs/conformance/services.md to note that only the Value attribute (id=13) is writable in micro profile. No code change — this is a conformance documentation update. `docs/conformance/services.md`. Grounding: OPC-10000-4 §5.11.4.

- [x] T013 [US3] Implement Browse resultMask. Filter ReferenceDescription fields based on mask bits. `src/services/browse.c`. Grounding: OPC-10000-4 §5.9.2 Table 34.

---

## Phase 3: Encoding (US4)

- [x] T014 [US4] Add GUID NodeId format (0x04) support. Add guid field to mu_nodeid_t, implement encode/decode for 16-byte UUID. `include/muc_opcua/types.h`, `src/encoding/binary_nodeid.c`. Grounding: OPC-10000-6 §5.2.2.9. **WARNING**: Shares `types.h` and `binary_nodeid.c` with T015 — must run sequentially after T015 or vice versa.

- [x] T015 [US4] Add Opaque/ByteString NodeId format (0x05) support. Add namespace+data fields to mu_nodeid_t, implement encode/decode. `include/muc_opcua/types.h`, `src/encoding/binary_nodeid.c`. Grounding: OPC-10000-6 §5.2.2.9.

- [x] T016 [US4] Implement multi-chunk message support. Accept 'C' continuation chunks, buffer partial messages, reassemble before dispatch. `src/core/message_chunk.c`, `src/core/message_chunk.h`, `src/core/server.c`, `src/core/server_internal.h`. Grounding: OPC-10000-6 §6.7.2.

---

## Phase 4: Validation

- [x] T017 Build and run all 108 tests: `cmake --build build -j$(nproc) && ctest`. Must pass.

- [x] T018 Run ASAN/UBSan: `cmake -B build-san && cmake --build build-san -j$(nproc) && ctest --test-dir build-san`.
