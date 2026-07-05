# TODO — muc-opcua

**Updated**: 2026-07-05 (spec 039 implementation)
**Source**: stale audit findings, interop gaps, hot-path audit, comp review

## Remaining Active Backlog

(All 43 items cleared — see Completed in Spec 039 below.)

## ✅ Completed in Spec 039

- Bugs: protocol version constant (T002), sampling interval fix (T003), timestampsToReturn validation (T004)
- Hot-Path: variant array free (T005), read cache enabled (T006), tick batching (T007), auth token reuse (T008), reader/writer bounds batching (T009-T010), DataValue zero-init removal (T011), memmove deferral (T012), session timeout scan optimization (T013), write type-check fix (T014), duplicate node check O(N log N) (T015), MessageSize once (T016), listen() gate (T017)
- Code Quality: portable ctz (T018), LE uint32 consolidation (T019), dispatch dedup (T020), subscription include guard (T021), history error propagation (T022), strict aliasing fix (T023), C11 doc (T024), int32 cast guards (T025), expanded nodeid flags (T026), calloc gate (T027)
- Security: OPN nonce 128B (T028), RSA decrypt buffer 512B (T029), constant-time memcmp (T030), event filter select_count cap (T031)
- Tests: transfer_subscriptions behavioral (T032), dispatch_subscription unit tests (T032a), 5 placeholder files → stub markers (T033-T034), service_message tests (T035), circular verification → byte fixtures (T036), encode gap catalog + fill (T037-T037b), WriteResponse fixture + round-trip (T038), reader->position assertions (T039), interop Subscribe/Publish + Call (T040)
- Docs: 4 ADRs (T041), spec grounding in 3 core files (T042), spec grounding in 2 headers (T043)
- CI: 124 tests pass (123/124, pre-existing test_traceability_docs failure unchanged)

## ✅ Completed in Spec 038

- CI/CD: removed `|| true` from 5 silent-failure steps, wired sanitizers, added coverage+fuzz jobs, added `standard` to matrix, set Debug build type
- Clang-tidy: added `--warnings-as-errors=*`
- Session timeout: gated on `MUC_OPCUA_SESSION_TIMEOUT` instead of `MUC_OPCUA_MULTI_CHUNK`
- `mu_session_create` deprecated in favor of `mu_session_create_with_identifiers`
- `mu_checked_memcpy_off` integer underflow guard (UB1)
- `mu_binary_read_expanded_nodeid` null-buffer guard (UB2)
- `handle_activate_session` decomposed into per-token helpers
- `mu_server_poll` common logic extracted from duplicated branches
- Version tracking: CHANGELOG.md + version.h.in
