# TODO — muc-opcua

**Updated**: 2026-07-07 (spec 045 + F1 mDNS complete)
**Source**: code review findings, complexity audit, binary size analysis

## Remaining Active Backlog

### Proposed Features

| ID | Feature | Scope | Effort | OPC Ref |
|----|---------|-------|--------|---------|
| F1 | **mDNS discovery (server-side)** | ✅ Complete (spec 048) — `mu_mdns_adapter_t` interface, no-op adapter, POSIX host adapter via UDP multicast. Server wiring in `mu_server_init`/`mu_server_close`. Gate: `MUC_OPCUA_SERVICE_DISCOVERY`. OPC-10000-12 §6.3 (LDS-ME). |

### Features with Stub Tests — Feature Implementation Required First

### Bugs (functional) ✅

| ID | File | Severity | Issue | Status |
|----|------|----------|-------|--------|
| CR1 | `tests/unit/test_write_response.c` | MEDIUM | Tests silently become no-ops when `MUC_OPCUA_SERVICE_WRITE` undefined | ✅ Fixed: #else skip placeholder with TEST_PASS_MESSAGE |
| CR2 | `src/security/asym_chunk.c` | LOW | Duplicate safe conversion logic; should reuse `mu_safe_int32_from_size_t` | ✅ Fixed: replaced ternary clamping |

### Code Quality ✅

| ID | File | Severity | Issue | Status |
|----|------|----------|-------|--------|
| CR3 | `src/core/ctz.h` | LOW | `mu_ctz_u32` macro vs inline — unify to `static inline` | ✅ Fixed: unified `static inline` |
| CR4 | `src/core/safe_mem.h` | LOW | `#include <stdint.h>` between declarations | ✅ Fixed: moved to top |
| CR5 | `tests/unit/test_dispatch_subscription.c` | LOW | Duplicate test helpers | ✅ Fixed: shared test_subscription_helpers.h |

### Documentation ✅

| ID | File | Severity | Issue | Status |
|----|------|----------|-------|--------|
| CR6 | `specs/039-clear-remaining-backlog/spec.md` | LOW | Summary count mismatch (43 vs 40) | ✅ Fixed: reconciled to 40 |
| CR7 | `specs/039-clear-remaining-backlog/plan.md` | LOW | Missing CTZ lookup table in size budget | ✅ Fixed: added 128-byte note |
| CR8 | `specs/039-clear-remaining-backlog/quickstart.md` | LOW | Placeholder grep misses #warning | ✅ Fixed: updated grep pattern |
| CR9 | `specs/039-clear-remaining-backlog/quickstart.md` | LOW | Verification block not self-contained | ✅ Fixed: uses `mkdir -p build && cd build` |

### Features with Stub Tests (implementation complete — spec 043)

Feature code exists. Only test implementation is needed.

| ID | Test File | Feature | What the Test Must Cover | OPC Ref | Status |
|----|-----------|---------|--------------------------|---------|--------|
| STUB1 | `tests/unit/test_aggregate_full.c` | Full aggregate function behavioral tests | ≥10 aggregate functions: (a) numeric: Avg, Sum, Min, Max, Count — known inputs → expected outputs, (b) PercentDeadband: samples crossing/not crossing threshold, (c) DurationGood/DurationBad: status-coded samples, (d) AggregateStatus: worst/best status, (e) Range: max − min. Each function independently testable | OPC-10000-13 | ✅ Complete (spec 043) |
| STUB2 | `tests/unit/test_audit_events.c` | Audit event dispatch (mu_raise_audit_event path) | (a) Registered callback receives event with correct fields (ActionTimeStamp, ServerId, ClientAuditEntryId), (b) No callback → graceful no-op return, (c) NULL server/event pointer → graceful no-op return, (d) Multiple callbacks registered → all fire | OPC-10000-5 §6.5 | ✅ Complete (spec 043) |
| STUB5 | `tests/unit/test_complex_types.c` | Complex type round-trip encode/decode | ≥3 types: (a) Structure with required scalar fields → encode→decode→deep-equal, (b) Structure with optional fields (EncodingMask) → verify present/absent detection, (c) Structure with nested structures → all levels preserved, (d) Structure with arrays → length + elements preserved | OPC-10000-3 §5.6.4 | ✅ Complete (spec 043) |
| STUB6 | `tests/integration/test_minimal_server_flow.c` | Minimal server lifecycle integration test | Full lifecycle: TCP connect → HEL→ACK → OpenSecureChannel → CreateSession → ActivateSession → Read → CloseSession → disconnect. Verify all StatusCodes are Good, repeat 3× no memory leaks | OPC-10000-6 §7.1 / OPC-10000-4 §5.6 | ✅ Complete (spec 043) |
| STUB7 | `tests/integration/test_discovery_endpoint_no_session.c` | Discovery without session | (a) GetEndpoints without session → endpoints with correct SecurityPolicy + TransportProfile URIs, (b) FindServers without session → servers list with correct ApplicationName + ApplicationURI, (c) FindServers filtered by ApplicationURI → only matching servers returned, (d) MU_DISCOVERY_MAX_ENDPOINTS limit enforced | OPC-10000-4 §5.5.4 | ✅ Complete (spec 043) |

### Features with Stub Tests (implementation complete — spec 044)

| ID | Test File | Feature | OPC Ref | Status |
|----|-----------|---------|------------------------|---------|
| STUB3 | `tests/unit/test_reverse_connect.c` | Server-initiated TCP connections | OPC-10000-6 §7.5 | ✅ Complete (spec 044) |
| STUB4 | `tests/unit/test_time_sync.c` | Security time synchronization | OPC-10000-4 §A.2 | ✅ Complete (spec 044) |
| STUB8 | `tests/unit/test_async_opcua_inventory.c` | Async OPC-UA conformance inventory | N/A (conformance doc) | ✅ Complete (spec 044) |

### Deferred Features (code incomplete — tests exist)

| ID | Feature | Status |
|----|---------|--------|
| D1 | Complex type encode/decode | ✅ Complete (spec 045) — `mu_binary_encode_struct`/`mu_binary_decode_struct`/`mu_binary_encode_enum`/`mu_binary_decode_enum` implemented in `src/encoding/binary_complex.c`. Round-trip tests pass for scalar, optional fields, enumerations. OPC-10000-3 §5.6.4, OPC-10000-6 §5.2.2.12, §5.2.2.9. |
| D2 | Audit callback mechanism | ✅ Complete (spec 045) — `mu_audit_callback_t` typedef, `mu_server_set_audit_callback`/`mu_server_add_audit_callback` API, dispatch from `mu_raise_audit_event`. Callback ordering, auditing-disabled gate, NULL-safety. OPC-10000-5 §6.5. |
| D3 | 39 additional aggregate functions | ✅ Complete (spec 045) — All 21 new aggregate function IDs added to `opcua_ids.h`. Accumulate/publish branches for Count, Range, Duration*, Percent*, Start, End, Delta, DeltaBounds, TimeAverage, TimeAverage2, Total, Total2, Maximum2, Minimum2, WorstQuality, WorstQuality2, AnnotationCount, Interpolative. Filter reader whitelist expanded. Gated on `MUC_OPCUA_AGGREGATE_FULL`. OPC-10000-13. |
| D4 | Binary size measurement | ✅ Complete (spec 045) — `scripts/measure_size.sh` now produces linked-ELF measurements with `.text`/`.data`/`.bss` breakdown, JSON output (`--json`), LTO comparison (`--lto`), and graceful toolchain-absence handling. Supporting files: `scripts/size_measure.ld`, `scripts/size_measure_startup.c`, `scripts/size_measure_main.c`. |

### Complexity Audit (2026-07-05) — Oversized Functions ✅ Fixed in Spec 040

| ID | File | Function | Lines | Status |
|----|------|----------|-------|--------|
| CX1 | `src/core/dispatch_session.c` | `handle_create_session` | 263→92 | ✅ Decomposed |
| CX2 | `src/core/dispatch_session.c` | `handle_activate_session` | 246→76 | ✅ Decomposed |
| CX3 | `src/core/service_dispatch.c` | `handle_modify_monitored_items` | 210→84 | ✅ Decomposed |
| CX4 | `src/core/server.c` | `process_message` | 183→85 | ✅ Decomposed |
| CX5 | `src/security/asym_chunk.c` | `mu_asym_chunk_unwrap` | 182→94 | ✅ Decomposed |
| CX6 | `src/services/subscription_publish.c` | `write_data_change_notification` | 168→89 | ✅ Decomposed |
| CX7 | `src/security/asym_chunk.c` | `mu_asym_chunk_wrap` | 166→55 | ✅ Decomposed |
| CX8 | `src/core/service_dispatch.c` | `handle_open_secure_channel` | 149→89 | ✅ Decomposed |
| CX9 | `src/services/subscription_publish.c` | `publish_due` | 139→34 | ✅ Decomposed |
| CX10 | `src/core/server.c` | `handle_data_chunk_secure` | 134→89 | ✅ Decomposed |
| CX11 | `src/core/server.c` | `handle_data_chunk_plaintext` | 123→79 | ✅ Decomposed |
| CX12 | `src/services/subscription_publish.c` | `build_publish_response` | 117→76 | ✅ Decomposed |
| CX13 | `src/core/dispatch_session.c` | `read_create_session_request` | 109→23 | ✅ Decomposed |
| CX14 | `src/core/server.c` | `mu_server_poll` | 102→36 | ✅ Decomposed |
| CX15 | `src/services/read.c` | `read_attribute` | 102→52 | ✅ Decomposed |
| CX16 | `src/core/service_dispatch.c` | `handle_create_monitored_items` | 98→52 | ✅ Decomposed |

### Complexity Audit (2026-07-05) — Too Many Parameters ✅

| ID | File | Function | Params | Status |
|----|------|----------|--------|--------|
| CP1 | `src/security/sym_chunk.c` | `mu_sym_chunk_wrap` | 13→1 | ✅ mu_sym_wrap_params_t struct |
| CP2 | `src/security/asym_chunk.c` | `mu_asym_chunk_wrap` | 12→1 | ✅ mu_asym_wrap_params_t struct |
| CP3 | `src/security/asym_chunk.c` | `mu_asym_chunk_unwrap` | 9→1 | ✅ mu_asym_unwrap_params_t struct |
| CP4 | `src/core/uasc.c` | `mu_uasc_finalize_symmetric` | 8→1 | ✅ mu_uasc_sym_finalize_params_t struct |
| CP5 | `src/core/service_dispatch.c` | `drive_subscription_id_status_array` | 8→1 | ✅ mu_subscription_id_status_ctx_t struct |
| CP6 | `src/services/read.c` | `mu_read_process_with_user_index` | 8→1 | ✅ mu_read_process_params_t struct |

### Complexity Audit (2026-07-05) — Duplicate Code ✅

| ID | Area | Issue | Status |
|----|------|-------|--------|
| CD1 | `service_dispatch.c` + others | ns0-numeric NodeId check duplicated 13× | ✅ mu_nodeid_is_ns0_numeric() |
| CD2 | `mbedtls_crypto_adapter.c` | 8 near-identical AES/OAEP functions | ✅ parameterized helpers |
| CD3 | `host_crypto_adapter.c` | 6 near-identical cipher/OAEP functions | ✅ parameterized rsa_oaep_impl |
| CD4 | `sym_chunk.c`, `asym_chunk.c` | Manual LE byte writes | ✅ Already fixed pre-039 |
| CD5 | `dispatch_session.c` | Duplicate signature verification | ✅ build_server_nonce_verify_buf shared helper |
| CD6 | `server.c` | Poll helper duplication across #ifdef | ✅ unified poll_read_and_process |

### Complexity Audit (2026-07-05) — Deep Nesting ✅

| ID | File | Lines | Issue | Status |
|----|------|-------|-------|--------|
| CN1 | `service_dispatch.c` | 1182-1218 | 4-level if/else on filter type NodeId | ✅ extracted is_*_filter_binary_type helpers |
| CN2 | `service_dispatch.c` | 1621-1700 | 4-5 level if nesting in modify item fields | ✅ early returns + clamp_queue_size helper |
| CN3 | `service_dispatch.c` | 288-318 | 5-level if/else for event field name dispatch | ✅ static lookup table resolve_event_field_type_by_name |

### Future Work: Binary Size Measurement

Archive measurement (`measure_size.sh`) is conservative — measures `.a` without
`--gc-sections`. The linked ELF better reflects flash usage. See
`docs/research/binary-size-optimization.md` for full analysis.

## ✅ Completed in Spec 041 — Split Oversized Files

- 11 files >500 lines split into directories with ~55 sub-files
- service_dispatch → 17 files, subscription_publish → 5, server → 5, etc.
- All logic files now ≤488 lines (base_nodes.c excluded — generated tables)
- CMake build system updated, traceability docs updated with 69 new files

## ✅ Completed in Spec 040 — Decompose Oversized Functions

- 16 functions >100 lines decomposed into 34 file-static helpers
- Largest reductions: handle_create_session (263→92), handle_activate_session (246→76)
- All parent functions ≤100 lines, helpers ≤50 lines

## ✅ Completed in Spec 039 — Clear Remaining Backlog

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
