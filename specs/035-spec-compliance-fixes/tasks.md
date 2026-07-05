# Tasks: Spec Compliance Fixes

**Source**: `docs/audit/spec-compliance-2026-07-05.md`

---

## Phase 1: SecureChannel & Session (US1) — CRITICAL + HIGH

- [x] T001 [US1] Implement CloseSecureChannel handler in `src/core/service_dispatch.c`. Decode requestHeader, call `mu_secure_channel_close()`, encode CloseSecureChannelResponse. Grounding: OPC-10000-4 §5.6.3.2 Table 13. Fixes I4.

- [x] T002 [US1] Validate ClientNonce length (32-128 bytes) in OpenSecureChannel in `src/core/service_dispatch.c:463-465`. Return Bad_NonceInvalid if invalid. Grounding: OPC-10000-4 §5.6.2.3 Table 12. Fixes I1.

- [x] T003 [US1] Validate ClientNonce length (32-128 bytes) in CreateSession in `src/core/dispatch_session.c:169-171`. Return Bad_NonceInvalid. Grounding: OPC-10000-4 §5.7.2.3 Table 16. Fixes I2.

- [x] T004 [US1] Validate ApplicationUri against ClientCertificate in CreateSession in `src/core/dispatch_session.c:219-230`. Compare cert subject CN or SAN to ClientDescription.ApplicationUri. Return Bad_CertificateUriInvalid. Grounding: OPC-10000-4 §5.7.2.1. Fixes I3.

- [x] T005 [US1] Validate requestType in OpenSecureChannel in `src/core/service_dispatch.c:457`. Accept only 0 (ISSUE) and 1 (RENEW). Return Bad_RequestTypeInvalid. Grounding: OPC-10000-4 §5.6.2.3. Fixes I6.

- [x] T006 [US1] Report actual ServerProtocolVersion in OPN response in `src/core/service_dispatch.c:527`. Define MU_OPC_UA_PROTOCOL_VERSION = 0 (per spec §7.1.2.2 — version 0 for OPC UA TCP). Grounding: OPC-10000-4 §5.6.2.2 Table 11. Fixes I14.

- [x] T007 [US1] Validate ServerUri and EndpointUrl in CreateSession in `src/core/dispatch_session.c:157-158`. Compare against configured server endpoint values. Return Bad_ServerUriInvalid or Bad_EndpointUrlInvalid. Grounding: OPC-10000-4 §5.7.2.3. Fixes I21.

---

## Phase 2: Subscription & MonitoredItem (US2) — CRITICAL + HIGH

- [x] T008 [US2] Fix SetPublishingMode state table compliance in `src/core/dispatch_subscription.c:151-164`. Reset lifetime counter and set MoreNotifications=FALSE when publishing is enabled. Grounding: OPC-10000-4 §5.14.4 state table row 19. Fixes I5.

- [x] T009 [US2] Fix sampling interval=-1 handling in CreateMonitoredItems in `src/core/service_dispatch.c`. Use Subscription publishing interval instead of 50ms when -1 is specified. Grounding: OPC-10000-4 §7.21. Fixes I11. (I8/CreateSubscription not changed — see TODO.md: per §5.14.2.2/§5.14.3.2 a negative *publishing* interval must revise to the fastest supported interval, i.e. the 50ms minimum, which is already the current behavior.)

- [x] T010 [US2] Populate timestampsToReturn in CreateMonitoredItems DataValues in `src/core/service_dispatch.c:1299` and `src/services/subscription_publish.c:442-450`. Include sourceTimestamp/serverTimestamp based on requested TimestampsToReturn. Grounding: OPC-10000-4 §5.13.2.2 Table 63. Fixes I9.

- [x] T011 [US2] Report actual revised queue_size in CreateMonitoredItems response in `src/core/service_dispatch.c:1449-1450`. Use `item->queue_size` instead of hardcoded 1. Grounding: OPC-10000-4 §5.13.2.2 Table 63. Fixes I10.

- [x] T012 [US2] Apply queueSize and discardOldest in ModifyMonitoredItems in `src/core/service_dispatch.c:1593-1600`. Apply the decoded values to the monitored item. Grounding: OPC-10000-4 §5.13.3.2 Table 67. Fixes I12.

- [x] T013 [US2] Apply filter type changes in ModifyMonitoredItems in `src/core/service_dispatch.c:1550-1592`. Update the item's filter when a new DataChangeFilter or AggregateFilter is specified. Grounding: OPC-10000-4 §5.13.3.2. Fixes I13.

- [x] T014 [US2] Reject requests after CloseSession with Bad_SessionClosed in `src/core/dispatch_session.c:800-841`. Set session state to CLOSED and reject subsequent requests. Grounding: OPC-10000-4 §5.7.4.1. Fixes I22.

---

## Phase 3: Browse, Read, Write (US3)

- [x] T015 [US3] Implement ViewDescription filtering in Browse in `src/services/browse.c:82-93`. When a non-empty View is requested, filter references to only those defined for that View. Grounding: OPC-10000-4 §5.9.2.2 Table 34. Fixes I7.

- [x] T016 [US3] Populate timestamps in Read DataValue responses in `src/services/read.c:206`. Generate sourceTimestamp/serverTimestamp based on timestampsToReturn. Grounding: OPC-10000-4 §5.11.2.2 Table 47. Fixes I15.

- [x] T017 [US3] Match QualifiedName including namespace_index in TranslateBrowsePaths in `src/core/dispatch_view.c:131`. Compare both namespace_index and name when matching browse names. Grounding: OPC-10000-4 §7.30. Fixes I16.

- [x] T018 [US3] Reject empty targetName on last path element in TranslateBrowsePaths in `src/core/dispatch_view.c:109-166`. Return Bad_BrowseNameInvalid. Grounding: OPC-10000-4 §5.9.4.1. Fixes I17.

- [x] T019 [US3] Change Write attributeId decode from Int32 to UInt32 in `src/services/write.c:39`. Use mu_binary_read_uint32 to match Read behavior. Grounding: OPC-10000-4 §5.11.4.2 Table 53. Fixes I18.

- [x] T020 [US3] Accept DataType subtypes in Write type check in `src/core/dispatch_attribute.c:125`. Implement subtype-aware comparison instead of exact type match. Grounding: OPC-10000-4 §5.11.4.2 Table 53. Fixes I19.

---

## Phase 4: Encoding & Security (US4)

- [x] T021 [US4] Use entropy adapter for Channel ID in `src/services/secure_channel.c:88-91`. Replace incrementing counter with `generate_random()` when adapter is available. Grounding: OPC-10000-6 §6.7.4. Fixes I20.

- [x] T022 [US4] Implement Variant array read in `src/encoding/binary_variant.c:12-15`. Parse Int32 array length + elements when array bit (0x80) is set. Grounding: OPC-10000-6 §5.2.5, §5.2.2.16. Fixes I23.

---

## Phase 5: Validation

- [x] T023 Run full test suite: `cmake --build build -j$(nproc) && ctest -V`. All 108 must pass.

- [x] T024 Run ASAN/UBSan: `cmake -B build-san -DMUC_OPCUA_SANITIZE=ON -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build build-san -j$(nproc) && ctest --test-dir build-san`. Zero failures.
