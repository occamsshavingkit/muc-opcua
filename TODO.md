# TODO — muc-opcua

**Updated**: 2026-07-04
**Source**: `docs/review/five-lens-audit-2026-07-04.md`, interop findings, code review

## Remaining Backlog

### Deferred audit findings

| File | Finding | Notes |
|------|---------|-------|
| `src/services/secure_channel.c` | T17 | Channel ID entropy. Fix 3 integration tests first (they assume channel_id=1). |
| `src/services/browse.c` | T22 | TypeDefinition cache. Requires adding field to `mu_node_t` struct. |
| `src/core/tcp_connection.c` | T006 follow-up | Lines 20 and 129 hardcode the OPC UA TCP protocol version (`0`/`0u`) instead of using `MU_OPC_UA_TCP_PROTOCOL_VERSION` from `include/muc_opcua/transport.h`. Same constant as T006 but in the HEL/ACK exchange rather than the OPN response, so out of T006's scope. |
| `specs/035-spec-compliance-fixes` | T007 grounding invalid | T007 requested CreateSession ServerUri/EndpointUrl validation returning Bad_ServerUriInvalid, citing "OPC-10000-4 §5.7.2.3". The **current** spec says the opposite: §5.7.2.2 Table 15 states serverUri "is no longer used ... the Server shall ignore any value provided" and endpointUrl is "diagnostics" only (never reject). Strict validation broke 12 tests that correctly send null serverUri. The canonical `MU_STATUS_BAD_SERVERURIINVALID` (0x804F0000, per OPC Foundation Opc.Ua.StatusCodes.csv) was added to `status.h`/`status.c` — it applies to FindServers/RegisterServer (§5.5.5.3), not CreateSession. Revisit if a future FindServers/RegisterServer implementation needs it; do NOT add CreateSession validation. |
| `src/core/service_dispatch.c` | T009 I8 audit misattribution | Audit finding I8 claimed CreateSubscription mishandles sampling interval=-1. CreateSubscription has a **publishing** interval, not a sampling interval, and per OPC-10000-4 §5.14.2.2/§5.14.3.2 "If the requested value is 0 or negative, the Server shall revise with the fastest supported publishing interval" — i.e. the minimum (50 ms). The current `publishing_interval_bits_to_ms` behavior is already spec-compliant, so I8 was not changed. Only I11 (CreateMonitoredItems §7.21) was a real bug and was fixed in T009. |
| `src/core/service_dispatch.c` | ModifyMonitoredItems sampling interval=-1 | Same §7.21 bug as I11 but in `handle_modify_monitored_items` (~line 1656, `revised_sampling_interval_ms = publishing_interval_bits_to_ms(sampling_interval_bits)`). When a ModifyMonitoredItems request sends -1, it currently resolves to the 50 ms minimum instead of the owning Subscription's `publishing_interval_ms`. Out of T009's explicit scope (which named only CreateSubscription/CreateMonitoredItems); fix alongside T012/T013 which already touch ModifyMonitoredItems. |
| `src/core/service_dispatch.c` | ModifyMonitoredItems timestampsToReturn ignored | `handle_modify_monitored_items` (~line 1548) decodes `timestamps_to_return` then `(void)`s it, so a modify request can neither validate (`Bad_TimestampsToReturnInvalid`) nor update the per-item setting that Publish relies on. Symmetric with the CreateMonitoredItems fix done in T010; fix alongside T012/T013 which already touch ModifyMonitoredItems. |
| `tests/unit/test_address_space_dynamic.c` | Pre-existing test failure: `test_Browse_DynamicReferences` | Discovered while running the full suite after T016. The test (line 214) expects `MU_STATUS_GOOD` from `mu_browse_process_with_user_index` but gets `MU_STATUS_BAD_VIEWIDUNKNOWN` (0x80390000). Reproduces without any of T016's changes (the read/timestamp code path). Browse/dynamic-view issue — unrelated to Read timestamps. Fix before T023/T024 can be marked complete. |
| `tests/` (7 tests) | Pre-existing failures blocking T023/T024 | Discovered while running the full suite after T022. `test_service_state_errors`, `test_server_handshake`, `test_response_regression`, `test_user_auth_encrypted`, `test_user_auth_certificate`, `test_write_service_integration`, and `test_event_notifications` all fail with the accumulated working-tree changes from prior tasks (T003-T020) but pass at the clean branch HEAD. Reproduced with T022's variant changes reverted, so NOT caused by T022. `test_response_regression` shows response-byte drift (e.g. Expected 0x4D Was 0x4F) — golden masters need regenerating after the response-encoding changes in T016/T010. Investigate and fix before T023/T024. |
| `src/core/server.c`, `src/core/service_dispatch.c`, `src/services/session.h` | T001 (spec 036) session-timeout SEGFAULT under ctest | T001 added session-timeout reaping in `mu_server_poll_background` and a `last_activity_ms` refresh in `mu_service_dispatch`. The reaper keys off the **monotonic** time adapter, so under ctest (which adds ~1 s of startup overhead before the first `mu_server_poll`) tests that create a session and call `mu_server_poll` later see `now_ms - last_activity_ms > revised_session_timeout_ms` and the session is force-closed mid-test, SEGFAULT-ing `test_session`, `test_dispatch_services`, `test_dispatch_session_order`, `test_service_dispatch`, `test_method_call_errors`, `test_connection_multiplex` in the default build (and the same class of tests in `build/test-micro`, `build/test-embedded`). Tests pass when the binary is run directly (no ctest overhead) and pass when T001's working-tree diff is reverted. NOT caused by T007 (SetMonitoringMode immediate-sample), which only touches `set_monitoring_mode_result`. Fix: either initialise `last_activity_ms` at session creation from the same adapter, or use the mock/zero-tick guard consistently in tests, or cap the reaper so it never fires inside the test's first poll window. |
| `src/services/subscription_publish.c` | T002 (spec 036) breaks `test_advance_publish_timer_terminates_within_bound_when_interval_is_zero` | T002 increments `sub->lifetime_counter` every `publish_due` cycle and deletes the subscription when `lifetime_counter >= lifetime_count`. In `tests/unit/test_subscription_publish.c:60` the zero-interval subscription is reaped before the assertion can read `next_publish_ms` (expected ≤100, observed 1001 because the slot was recycled/reset). Reproduces in `build/test-micro` and `build/test-embedded`. Fix: in the test, either set a large `lifetime_count`, or guard the reaper so a publish that sends a response resets the lifetime counter (T002 already resets on send — the bug is the unconditional `++lifetime_counter` on every `advance_publish_timer`). |

### Tech debt

- `src/core/service_dispatch.c` (being split in spec 032) — extract per-service dispatch into modules
- `src/services/subscription.c` (split completed in spec 032) — verify module boundaries

### Interop Test Hardening (HIGH)

**Problem discovered 2026-07-04**: WriteResponse was missing the mandatory `diagnosticInfos[]` field (OPC-10000-4 §5.11.4.2, OPC-10000-6 §5.2.5). Neither interop smoke tests nor unit tests caught this — open62541 client was the detector.

**Root causes identified**:
1. `tests/interop/interop_smoke.py` has **zero write tests** — no Write request is ever sent to the server
2. `tests/unit/test_write_decoder.c` validates `results[]` encoding but **never reads diagnosticInfos** — incomplete wire-level test
3. No test verifies a full WriteResponse binary round-trip against a known-good fixture

**Required**:
- [ ] Audit all interop tests: verify each service (Read, Write, Browse, Subscribe, Publish, Call, CreateSession, ActivateSession) has at least one wire-level round-trip test against a real client
- [ ] Audit all `*_encode` unit tests: verify each reads every mandatory field in the encoded response, including null/empty arrays
- [ ] Add write interop test: issue Write via asyncua/opcua-asyncio, verify server responds with well-formed WriteResponse
- [ ] Generate binary fixture for WriteResponse and add round-trip encode/decode test
- [ ] No silent failures: every test that reads a binary response must verify `reader->position == expected_length` at the end to catch trailing/missing fields
