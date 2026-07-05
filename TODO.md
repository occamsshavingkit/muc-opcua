# TODO — muc-opcua

**Updated**: 2026-07-05 (specs 039, 040, 041 implementation complete)
**Source**: code review findings, complexity audit, binary size analysis

## Remaining Active Backlog

Items identified by CodeRabbit review of spec 039 (PR #251) — deferred for future cleanup:

### Bugs (functional)

| ID | File | Severity | Issue |
|----|------|----------|-------|
| CR1 | `tests/unit/test_write_response.c` | MEDIUM | Tests silently become no-ops when `MUC_OPCUA_SERVICE_WRITE` undefined — needs explicit skip placeholder |
| CR2 | `src/security/asym_chunk.c` | LOW | Duplicate safe conversion logic; should reuse `mu_safe_int32_from_size_t` for cert/thumb length clamping |

### Code Quality

| ID | File | Severity | Issue |
|----|------|----------|-------|
| CR3 | `src/core/ctz.h` | LOW | `mu_ctz_u32` is a macro on GCC/Clang but `static inline` on fallback — unify to `static inline` wrapper for type safety |
| CR4 | `src/core/safe_mem.h` | LOW | `#include <stdint.h>` between declarations instead of at top — reorder for MISRA compliance |
| CR5 | `tests/unit/test_dispatch_subscription.c` | LOW | Duplicate helpers (`test_entropy`, `test_tick_ms`, `prepare_server`, etc.) from `test_transfer_subscriptions.c` — extract to shared test utility |

### Documentation

| ID | File | Severity | Issue |
|----|------|----------|-------|
| CR6 | `specs/039-clear-remaining-backlog/spec.md` | LOW | Summary says 43 items but listed categories sum to 40 — reconcile count |
| CR7 | `specs/039-clear-remaining-backlog/plan.md` | LOW | Size budget missing CTZ de Bruijn lookup table (128 bytes static) |
| CR8 | `specs/039-clear-remaining-backlog/quickstart.md` | LOW | Placeholder verification grep misses `#warning "STUB"` marker |
| CR9 | `specs/039-clear-remaining-backlog/quickstart.md` | LOW | Full verification block starts with `cd build` but previous section already left reader there |

### Features with Stub Tests (test infrastructure exists, implementation deferred)

| ID | Test File | Feature | OPC Ref |
|----|-----------|---------|---------|
| STUB1 | `tests/unit/test_aggregate_full.c` | Full aggregate function set (42 functions) | OPC-10000-13 |
| STUB2 | `tests/unit/test_audit_events.c` | Audit event dispatch (mu_raise_audit_event call path) | OPC-10000-5 §6.5 |
| STUB3 | `tests/unit/test_reverse_connect.c` | Server-initiated TCP connections | OPC-10000-6 §7.5 |
| STUB4 | `tests/unit/test_time_sync.c` | Security time synchronization | OPC-10000-4 §A.2 |
| STUB5 | `tests/unit/test_complex_types.c` | Complex type round-trip encode/decode | OPC-10000-3 §5.6.4

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

### Complexity Audit (2026-07-05) — Too Many Parameters

| ID | File | Function | Params |
|----|------|----------|--------|
| CP1 | `src/security/sym_chunk.c:99` | `mu_sym_chunk_wrap` | 13 |
| CP2 | `src/security/asym_chunk.c:115` | `mu_asym_chunk_wrap` | 12 |
| CP3 | `src/security/asym_chunk.c:281` | `mu_asym_chunk_unwrap` | 9 |
| CP4 | `src/core/uasc.c:13` | `mu_uasc_finalize_symmetric` | 8 |
| CP5 | `src/core/service_dispatch.c:1252` | `drive_subscription_id_status_array` | 8 |
| CP6 | `src/services/read.c:428` | `mu_read_process_with_user_index` | 8 |

### Complexity Audit (2026-07-05) — Duplicate Code

| ID | Area | Issue |
|----|------|-------|
| CD1 | `service_dispatch.c` + others | ns0-numeric NodeId check duplicated 13× — needs shared `mu_nodeid_is_ns0_numeric()` |
| CD2 | `mbedtls_crypto_adapter.c` | 8 near-identical AES/OAEP functions differing only in key size or hash |
| CD3 | `host_crypto_adapter.c` | 6 near-identical cipher/OAEP functions |
| CD4 | `sym_chunk.c`, `asym_chunk.c` | Manual LE byte writes instead of `mu_binary_le_put_u32()` | ✅ sym_chunk.c `get_u32` replaced; asym_chunk.c still uses manual byte writes |
| CD5 | `dispatch_session.c` | Duplicate signature verification logic in `verify_activate_client_signature` and `handle_activate_certificate` |
| CD6 | `server.c` | Poll helper duplication across `#ifdef` branches |

### Complexity Audit (2026-07-05) — Deep Nesting

| ID | File | Lines | Issue |
|----|------|-------|-------|
| CN1 | `src/core/service_dispatch.c` | 1122-1186 | Filter-type dispatch chain reaches 5 nesting levels |
| CN2 | `src/core/service_dispatch.c` | 1621-1700 | Per-item filter-update logic nests 4-5 levels |
| CN3 | `src/core/service_dispatch.c` | 961-1069 | `read_event_filter_body` triple-nested with string-comparison chain |

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
