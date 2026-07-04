# Tasks: Five-Lens Audit Findings Cleanup

**Input**: Design documents from `/specs/029-fix-audit-findings/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/fix-contracts.md, quickstart.md
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md` (42 findings)

**Tests**: Protocol, security, StatusCode, session, and wire-visible changes are test-first per Constitution IV. Documentation-only tasks may omit tests.

**Organization**: Tasks grouped by user story. Tests appear before the implementation they validate. Each task is atomic (single file, single finding) and cites OPC UA spec sections.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Maps to user story from spec.md (US1–US5)
- Include exact file paths and OPC UA part/section references
- Each task maps to exactly one audit finding (T1–T42)

## Path Conventions

fixes: `src/core/service_dispatch.c`, `src/services/{browse,subscription,node_management,read,history,secure_channel,session}.c`, `src/address_space/{value_source,base_nodes,address_space}.c`, `src/security/{certificate,security_policy,asym_chunk}.c`, `src/platform/{host,mbedtls,wolfssl}_crypto_adapter.c`, `platform/pico/mu_pico_adapter.c`
tests: `tests/unit/test_*.c`, `tests/integration/test_*.c`
docs: `docs/review/`, `docs/adr/`, `docs/traceability/`

---

## Phase 1: Setup (Baseline Capture)

**Purpose**: Record pre-change baselines so every later task can compare against them

- [ ] T001 [P] Capture pre-change size baseline: run `scripts/measure_size.sh all` and save the per-profile `.text`/`.data`/`.bss` figures in `specs/029-fix-audit-findings/quickstart.md` (baseline block)
- [ ] T002 [P] Capture pre-change test baseline: run a green `ctest` across the `full` build and record the passing test count in `specs/029-fix-audit-findings/quickstart.md`
- [ ] T003 [P] Verify the audit report source document `docs/review/five-lens-audit-2026-07-04.md` contains all 42 findings T1–T42 and the 9 test coverage gaps

---

## Phase 2: User Story 1 — Critical Security Fixes (Priority: P1) 🎯 MVP

**Goal**: ActivateSession nonce generation must fail-closed on entropy failure; decrypted password buffer must be zeroized before function exit.

**Independent Test**: Drive ActivateSession with a mock entropy adapter that returns failure; assert `MU_STATUS_BAD_SECURITYCHECKSFAILED`. Run valgrind on the password-decrypt path; assert no plaintext password bytes survive past function return.

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [ ] T004 [US1] Add RED test `tests/unit/test_activate_session_nonce_fail.c`: nonce generation failure through mock entropy adapter → ActivateSession returns `MU_STATUS_BAD_SECURITYCHECKSFAILED`, per OPC-10000-4 §5.7.3 (ActivateSession) and §7.38.2 (StatusCode set). Audit finding T1.

- [ ] T005 [US1] Add RED test `tests/unit/test_password_decrypt_zeroize.c`: encrypted username-token password decrypted on stack → after `goto activate_done`, the `decrypt_buf[256]` stack region contains no plaintext password bytes (verify via canary pattern or valgrind), per OPC-10000-4 §5.7.3 (ActivateSession). Audit finding T2.

### Implementation for User Story 1

- [ ] T006 [US1] Change `fill_server_nonce()` signature in `src/core/service_dispatch.c` from `void` to `opcua_statuscode_t`; on entropy failure return the error code; update `handle_activate_session` caller to reject with `MU_STATUS_BAD_SECURITYCHECKSFAILED` when `fill_server_nonce` fails, consistent with `handle_create_session` and `handle_open_secure_channel` fail-closed behavior. OPC-10000-4 §5.7.3, §7.38.2. Audit finding T1.

- [ ] T007 [US1] Add `mu_secure_zero(decrypt_buf, sizeof(decrypt_buf))` at both exit points (before `goto activate_done` label target and before the closing `}` of the `#ifdef MUC_OPCUA_SECURITY` block) in `src/core/service_dispatch.c` `handle_activate_session`, per OPC-10000-4 §5.7.3 (ActivateSession). Audit finding T2.

**Checkpoint**: US1 independently testable — T004 and T005 green; no regressions in existing ActivateSession tests. Run `scripts/measure_size.sh nano micro embedded full` and confirm no increase in `.text`/`.data`/`.bss` from US1-only changes.

---

## Phase 3: User Story 2 — OPC UA Conformance Correctness Fixes (Priority: P1)

**Goal**: Browse returns references that fit the per-node limit; DeleteNodes respects the `deleteTargetReferences` flag; session creation avoids dangling sessions; write validation doesn't proceed on read failure; value_source_read handles all built-in scalar types; deadband None doesn't spam notifications; HistoryRead rejects invalid ExtensionObject encoding masks.

**Independent Test**: Each fix has a corresponding RED test that passes after the fix. Full ctest suite green across all four ARM profiles.

### Tests for User Story 2

> Write these tests first and confirm they fail before implementation.

- [ ] T008 [US2] Add RED test `tests/unit/test_browse_limit_preserves_refs.c`: browse a node with `requested_max_references_per_node` set lower than the node's actual reference count; assert `res->num_references == requested_max_references_per_node` and the references are non-null, per OPC-10000-4 §5.9.2 (Browse View Service Set). Audit finding T3.

- [ ] T009 [US2] Add RED test `tests/unit/test_delete_nodes_flag.c`: DeleteNodes with `delete_target_references = false`; assert the node is removed but references from surviving nodes pointing to the deleted node remain, per OPC-10000-4 §5.8.5.2 (DeleteNodes). Audit finding T9.

- [ ] T010 [US2] Add RED test `tests/unit/test_session_create_order.c`: force a response encoding failure after session ID allocation; assert no session slot is consumed (session remains CLOSED), per OPC-10000-4 §5.7.2 (CreateSession). Audit finding T8.

- [ ] T011 [US2] Add RED test `tests/unit/test_write_validation_read_failure.c`: write a value to a node whose `mu_value_source_read` callback returns an error; assert the write is aborted (StatusCode ≠ GOOD), per OPC-10000-4 §5.11.4 (Write). Audit finding T10.

- [ ] T012 [US2] Add RED test `tests/unit/test_value_source_scalar_types.c`: read static values of types Byte, Double, UInt16, Int64, DateTime, StatusCode; assert each returns the correct value (not `BAD_NOTREADABLE`), per OPC-10000-3 §5.6 (Variable NodeClass Value Attribute). Audit finding T11.

- [ ] T013 [US2] Add RED test `tests/unit/test_deadband_none_notification.c`: create monitored item with deadband type NONE; change nothing between two sample ticks; assert no data-change notification is generated on the second tick, per OPC-10000-4 §7.22.2 (DataChangeTrigger/deadband). Audit finding T27.

- [ ] T014 [US2] Add RED test `tests/unit/test_history_encoding_mask.c`: feed HistoryRead decoder an ExtensionObject with encoding mask 0x03; assert `MU_STATUS_BAD_NOTSUPPORTED`, per OPC-10000-6 §5.2.2.15 (ExtensionObject encoding). Audit finding T29.

### Implementation for User Story 2

- [ ] T015 [US2] In `src/services/browse.c`, at the `too_many_refs` label before `continue`, assign `res->references = &ref_pool[node_refs_start]; res->num_references = match_count;` per OPC-10000-4 §5.9.2. Audit finding T3.

- [ ] T016 [US2] In `src/services/node_management.c` DeleteNodes handler, guard the reference-cleanup loop with `if (items[i].delete_target_references)` per OPC-10000-4 §5.8.5.2. Audit finding T9.

- [ ] T017 [US2] In `src/core/service_dispatch.c` `handle_create_session`, move `write_response_prefix()` call before `mu_session_create_with_identifiers()` so encoding failure occurs before state mutation per OPC-10000-4 §5.7.2. Audit finding T8.

- [ ] T018 [US2] In `src/core/service_dispatch.c` Write processing path, after `mu_value_source_read` failure, set `result = MU_STATUS_BAD_INTERNALERROR` before the type-check guard per OPC-10000-4 §5.11.4. Audit finding T10.

- [ ] T019 [US2] In `src/address_space/value_source.c` `mu_value_source_read`, replace the narrow type switch with a broad default that returns `*value = source->data.static_value; return MU_STATUS_GOOD;` for all scalar built-in types per OPC-10000-3 §5.6. Audit finding T11.

- [ ] T020 [US2] In `src/services/subscription.c` `monitored_item_change_reportable`, when `deadband_type == MU_DEADBAND_TYPE_NONE`, return `false` (defer to caller's value-equality check) instead of unconditionally `true` per OPC-10000-4 §7.22.2. Audit finding T27.

- [ ] T021 [US2] In `src/services/history.c` HistoryRead ExtensionObject decoder, change `if ((encoding_mask & 0x01) == 0)` to `if (encoding_mask != 0x01)` to reject all non-ByteString encoding masks per OPC-10000-6 §5.2.2.15. Audit finding T29.

**Checkpoint**: US2 independently testable — T008–T014 green; existing Browse/Write/Session/Subscription/History tests still green. Run `scripts/measure_size.sh nano micro embedded full` and confirm per FR-023 gate.

---

## Phase 4: User Story 3 — Subscription Hot-Path Optimization (Priority: P1)

**Goal**: Publish cycle scans items once, not 3-4 times; deadband computation uses inline arithmetic instead of `fabs()` linking `<math.h>` (saving ~12 KB ROM); sample timer avoids 64-bit division in common case. Wire output identical.

**Independent Test**: Existing subscription deadband and integer-equivalence tests pass identically. `hotpath_benchmark` shows throughput improvement. `nm` confirms no `fabs` symbol. `measure_size.sh` confirms ~-12 KB ROM.

### Implementation for User Story 3

> No RED tests needed — existing tests validate wire-identical output. Implementation is refactoring with behavioral equivalence.

- [ ] T022 [US3] In `src/services/subscription.c` `mu_subscriptions_tick`, collect reportable item indices into `opcua_uint16_t reportable_indices[MU_MAX_MONITORED_ITEMS]` during the sampling pass, then iterate only over `reportable_count` items in the encoding and cleanup passes, replacing the triple full-scan per OPC-10000-4 §5.14 (Subscription). Verify wire output identical via existing `test_subscriptions` and `test_deadband_integer_equivalence`. Audit finding T4.

- [ ] T023 [US3] In `src/services/subscription.c` `monitored_item_change_reportable`, replace `fabs((double)numeric - (double)item->last_reported_numeric)` with inline `opcua_double_t diff = numeric - item->last_reported_numeric; if (diff < 0.0) diff = -diff;` per OPC-10000-4 §7.22.2. Verify identical deadband decisions via existing `test_deadband_integer_equivalence`. Audit findings T5 + T24.

- [ ] T024 [US3] Remove `#include <math.h>` from `src/services/subscription.c` (no remaining math library calls). Verify via `nm build/full/libmuc_opcua.a | grep fabs` returns empty. Audit finding T24.

- [ ] T025 [US3] In `src/services/subscription.c` `advance_sample_timer`, replace the 64-bit division `elapsed / interval` in the catch-up path with a repeated-subtraction loop capped at 100 iterations per OPC-10000-4 §5.14. The common `elapsed < interval` fast path already avoids division; this improves the rare catch-up path. Audit finding T6.

**Checkpoint**: US3 independently testable — existing subscription tests green; `measure_size.sh full` shows ~-12 KB ROM; no `<math.h>` dependency. Run `scripts/measure_size.sh all` and confirm net reduction across all profiles.

---

## Phase 5: User Story 4 — Tier 2 Hardening (Priority: P2)

**Goal**: Explicit OAEP hash/MGF parameters in OpenSSL host adapter; consolidated certificate token ifdef; nonce stack copies zeroized; Pico TCP documentation; trust model documentation; entropy-based channel IDs; mbedTLS signature_length validation.

**Independent Test**: Each fix tested in isolation per acceptance scenarios in spec.md.

### Implementation for User Story 4

> All tasks are [P] — independent files, no sequential dependencies.

- [ ] T026 [P] [US4] In `src/platform/host_crypto_adapter.c` `rsa_oaep()`, add explicit `EVP_PKEY_CTX_set_rsa_oaep_md(pc, EVP_sha1())` and `EVP_PKEY_CTX_set_rsa_mgf1_md(pc, EVP_sha1())` calls matching the pattern in `rsa_oaep_sha256()`, per OPC-10000-4 §5.5 (certificate/security validation). Verify handshake still passes via `test_server_handshake_secure`. Audit finding T7.

- [ ] T027 [P] [US4] In `src/core/service_dispatch.c` `handle_activate_session`, consolidate the certificate token (type 327) decode and verification paths under a single `#ifdef MUC_OPCUA_SECURITY` block per OPC-10000-4 §5.7.3. Audit finding T12.

- [ ] T028 [P] [US4] In `src/core/service_dispatch.c` `handle_create_session` and `handle_activate_session`, add `mu_secure_zero(nonce_buf, sizeof(nonce_buf))` after the `memcpy` of the nonce into the session slot per OPC-10000-4 §5.7.2. Audit finding T13.

- [ ] T029 [P] [US4] Add documentation to `platform/pico/README.md` stating the current TCP stub status: `pico_tcp_*` functions are compile-time stubs only; real TCP networking requires LWIP or CYW43 integration; the adapter serves as a compile-validation target. Audit finding T14.

- [ ] T030 [P] [US4] Add documentation to `docs/adr/0002-audit-remediation-security-decisions.md` (or new ADR) describing the trust model: DER-exact certificate comparison via `mu_trust_list_match`, no chain-of-trust or CRL/OCSP validation, intended for pre-verified certificate deployments per OPC-10000-4 §5.5. Audit finding T16.

- [ ] T031 [P] [US4] In `src/services/secure_channel.c` `mu_secure_channel_open`, use the entropy adapter (`generate_random`) for at least 4 bytes of the channel ID instead of hardcoding `1` per OPC-10000-6 §6.7.4. Fall back to current hardcoded `1` when no entropy adapter is configured. Audit finding T17.

- [ ] T032 [P] [US4] In `src/platform/mbedtls_crypto_adapter.c` `m_rsa_pss_sha256_verify`, validate `signature_length == mbedtls_rsa_get_len(rsa)` before calling `mbedtls_rsa_rsassa_pss_verify` per OPC-10000-4 §5.5 (certificate validation). Return `MU_STATUS_BAD_SECURITYCHECKSFAILED` on mismatch. Audit finding T18.

**Checkpoint**: US4 independently testable — all 7 acceptance scenarios pass. Run `scripts/measure_size.sh nano micro embedded full` and confirm per FR-023 gate.

---

## Phase 6: User Story 5 — Tier 3 Cleanup & Test Coverage (Priority: P3)

**Goal**: Close all remaining low-severity findings (T15, T19–T26, T28, T30–T42). Add tests for the 9 coverage gaps. Apply const-correctness, loop bounds, and documentation improvements.

**Independent Test**: Each item is independently verifiable. Const-correctness verified by compile check. Tests verified by ctest. Documentation verified by review.

### Tests for User Story 5 (coverage gaps)

> These 8 tests close the remaining coverage gaps from the audit report. The 9th gap (ActivateSession nonce fail-closed) is already satisfied by T004 in US1.

- [ ] T033 [P] [US5] Add test `tests/unit/test_read_browsename_namespace.c`: read BrowseName attribute of a non-ns=0 node; assert `namespace_index` reflects the BrowseName's actual namespace, not the NodeId's namespace per OPC-10000-3 §5.2.4. Audit finding T19.

- [ ] T034 [P] [US5] Add test `tests/unit/test_read_timestamps_to_return.c`: read a variable with `timestamps_to_return != 0`; assert the DataValue contains non-zero `source_timestamp` and/or `server_timestamp` per OPC-10000-4 §5.11.2.3. Audit finding T20.

### Implementation for User Story 5

> Most tasks are [P] — independent files, can run in any order after US1–US4.

- [ ] T035 [P] [US5] Add a `BAD_TOOMANYOPERATIONS` guard in `src/core/service_dispatch.c` Query-first handler before writing past `node_types[4]` / `filter_elements[4]` / `filter_operands[8]` array bounds per OPC-10000-4 §5.9 (Query). Audit finding T21.

- [ ] T037a [P] [US5] Add `opcua_uint32_t type_definition_numeric` field (ns=0 numeric NodeId of the HasTypeDefinition target, or 0 if none) to `mu_node_t` in `include/muc_opcua/address_space.h`. Populate it during node creation/init where HasTypeDefinition references are already resolved. Audit finding T22 (data-structure change only — no behavior change in this task).

- [ ] T037b [US5] In `src/services/browse.c` Browse reference loop, replace the O(R*T) `HasTypeDefinition` linear search over target node references with a direct read of `target->type_definition_numeric` per OPC-10000-4 §5.9.2. Depends on T037a. Audit finding T22.

- [ ] T038 [P] [US5] In `src/core/service_dispatch.c` `find_service_descriptor`, replace the linear scan through `g_supported_services[]` with a switch-case dispatch on the request NodeId's numeric identifier (OPC UA request IDs cluster in the 400-900 range; all are ns=0 numerics), falling through to the linear scan for unknown IDs. This avoids adding a lookup table while eliminating ~15 comparisons per request on average per OPC-10000-4 §5.5.1 (Discovery). Audit finding T23.

- [ ] T039 [P] [US5] In `src/core/service_dispatch.c` `handle_get_endpoints`, parse the profile URIs once into a small array, then compare each endpoint against the cached array instead of re-reading and discarding per endpoint per OPC-10000-4 §5.5.1. Audit finding T25.

- [ ] T040 [P] [US5] Add `const` to `g_supported_services[]` in `src/core/service_dispatch.c` and `POLICY_TABLE[]` in `src/security/security_policy.c` so they reside in `.rodata` (flash). No spec citation — defense-in-depth against RAM bit-flips on M0+. Audit finding T26.

- [ ] T041 [P] [US5] Consolidate the 35+ individual `#if MUC_OPCUA_BASE_TYPE_SYSTEM` / `#endif` blocks in `src/address_space/base_nodes.c` into a single contiguous `#ifdef` block for the type-system node set. Audit finding T28.

- [ ] T042 [P] [US5] In `src/core/service_dispatch.c` `handle_translate_browse_paths` response encoding, compute `remainingPathIndex = (opcua_uint32_t)(element_total - element_index - 1)` instead of hardcoding `0xFFFFFFFFu` per OPC-10000-4 §5.9.3 (TranslateBrowsePaths). Audit finding T30.

- [ ] T043 [P] [US5] In `src/services/subscription.c` `advance_publish_timer`, add a maximum iteration guard of 100 to the `do...while` loop on zero-interval subscriptions per OPC-10000-4 §5.14. Audit finding T31.

- [ ] T044 [P] [US5] In `src/security/certificate.c` `mu_server_config_validate` (or init path), add a check that the server's own certificate is within its validity window, logging a diagnostic if expired, per OPC-10000-4 §5.5 (certificate validity). Audit finding T32.

- [ ] T045 [P] [US5] Add documentation comment in `src/services/session.c` `mu_session_create` noting the XOR-with-constant session ID generation is bounded by `MU_MAX_SESSIONS` (default 2) and is not cryptographically random — acceptable for the single-connection profile. Audit finding T33.

- [ ] T046 [P] [US5] Add documentation comment in `src/security/trustlist.c` noting that `mu_trust_list_match` uses DER-exact byte comparison (not SPKI), meaning re-encoded certificates must match the exact DER bytes in the trust list. Audit finding T34.

- [ ] T047 [P] [US5] Add documentation comment in `src/core/server.c` noting that `opn_pending_security_policy` / `opn_pending_client_cert` are transient pointers into the receive buffer, valid only within a single `mu_server_poll` cycle (safe for the single-threaded poll model). Audit finding T36.

- [ ] T048 [P] [US5] Add documentation to `platform/pico/README.md` recommending that production code seed a software DRBG (e.g., CTR_DRBG from mbedTLS) from the RP2040 ROSC entropy source rather than using the raw ROSC bits directly. Audit finding T39.

- [ ] T049a [P] [US5] In `src/encoding/binary_string.c`, make `mu_binary_read_bytestring` and `mu_binary_write_bytestring` `static inline` (they are pure casts to `mu_string_t*` with delegate). Move their declarations to `include/muc_opcua/encoding.h`. Audit finding T40.

- [ ] T049b [P] [US5] In `src/encoding/binary_writer.c`, make the int→uint cast wrappers (`mu_binary_write_int16`, `mu_binary_write_int32`, `mu_binary_write_int64`) `static inline` in `include/muc_opcua/encoding.h`. Audit finding T13 (Performance audit).

- [ ] T050 [P] [US5] In `src/core/message_chunk.c`, pack the 3-byte message type + 1-byte chunk_type into a single `uint32_t` for comparison instead of six sequential character comparisons, per OPC-10000-6 §6.7.2 (MessageChunk structure). Audit finding T41.

**Checkpoint**: US5 independently testable — all Tier 3 findings addressed; 9 coverage gaps have green tests. Run `scripts/measure_size.sh all` and confirm per FR-023 gate.

---

## Phase 7: Polish & Cross-Cutting

**Purpose**: Global gate verification, size measurement, and traceability updates

- [ ] T051 Run global gate sweep: `make test-nano && make test-micro && make test-embedded && make test-full` — all four profile suites green. Run `hotpath_benchmark` and compare against T001 baseline; verify publish-cycle throughput improvement.
- [ ] T052 Run ASAN/UBSan builds: `ctest --test-dir build/asan` green.
- [ ] T053 Run `scripts/measure_size.sh all` and compare against T001 baseline: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap, ~-12 KB ROM from `<math.h>` removal.
- [ ] T054 Run `scripts/check_build_matrix.sh` — all embedded cross-compiles green.
- [ ] T055 Verify `<math.h>` no longer linked: `nm build/full/libmuc_opcua.a | grep -c fabs` returns 0.
- [ ] T056 [P] Update `specs/029-fix-audit-findings/quickstart.md` with the final post-change numbers.
- [ ] T057 [P] Update `docs/traceability/conformance-claims.md` if any normative scope changed (expected: no changes — all fixes preserve or correct existing behavior).
- [ ] T058 [P] Document audit findings T35 (MessageSize bounded by `MU_ASYM_MAX_PLAINTEXT` — no overflow reachable), T38 (stack protector recommendation for Cortex-M0 — deferred to platform toolchain config, not a library change), and T42 (O(N²) address-space validation is init-time only with small N — no fix needed) as "no fix needed" with rationale in `specs/029-fix-audit-findings/quickstart.md`. Satisfies SC-001 completeness.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — run immediately.
- **US1 (Phase 2)**: No dependencies on other stories. First P1 story to land (security-critical).
- **US2 (Phase 3)**: No dependencies on US1. Can run in parallel with US1 (different files).
- **US3 (Phase 4)**: No dependencies on US1/US2. Can run in parallel (different functions in `subscription.c` from US2's T020/T021).
- **US4 (Phase 5)**: No dependencies on prior stories. All [P] tasks.
- **US5 (Phase 6)**: Depends on US1–US4 completion (builds on their fixes). The 9th coverage gap (nonce fail-closed) is already satisfied by T004 in US1.
- **Polish (Phase 7)**: Depends on all stories complete.

### Within Each User Story

- US1: T004→T006 (test then implement nonce fail-closed), T005→T007 (test then implement password zeroize)
- US2: T008→T015 (browse test then fix), T009→T016 (deleteNodes test then fix), T010→T017 (session test then fix), T011→T018 (write test then fix), T012→T019 (value_source test then fix), T013→T020 (deadband test then fix), T014→T021 (encoding mask test then fix)
- US3: T022→T023→T024→T025 sequential (subscription.c is a single file; avoid merge conflicts)
- US4: All [P] — run in any order
- US5: T033–T034 [P] first (tests), then T035–T050 [P] (implementations and docs, independent files). T037a must precede T037b. T049a and T049b are independent of each other.

### Parallel Opportunities

- **Setup**: T001, T002, T003 can run in parallel
- **US1 and US2**: Entire phases can run in parallel (different files)
- **US3**: Can run in parallel with US1/US2 after T022→T025 order within US3
- **US4**: All 7 tasks are [P] — fully parallelizable
- **US5**: Most tasks are [P] — T035–T050 all touch different files; T033–T034 are test-only. T037a must precede T037b.
- **Polish**: T056, T057 are [P]

---

## Implementation Strategy

1. **Land US1 first** (T001–T007): the two critical security fixes. Ships the nonce fail-closed and password zeroization. This is the MVP.
2. **Land US2 next** (T008–T021): the seven correctness fixes. Each has test-before-code.
3. **Land US3** (T022–T025): the subscription hot-path optimization. Largest ROM win.
4. **Land US4** (T026–T032): Tier 2 hardening in one batch (all independent).
5. **Land US5** (T033–T050): Tier 3 cleanup, new test coverage, and documentation.
6. **Gate sweep** (T051–T058): final validation, size verification, benchmark comparison, and no-fix documentation.

Each story is independently shippable and testable. US1 and US2 can proceed in parallel by different implementers.

---

## Notes

- Each task maps to exactly one audit finding T1–T42 for traceability. Findings T35, T38, and T42 are explicitly documented as "no fix needed" in T058.
- OPC UA spec citations are from the audit report and verified in contracts/fix-contracts.md.
- All protocol/security/StatusCode tasks have RED tests before implementation (Constitution IV).
- The `<math.h>` removal (T023–T024) is the only change that affects the link; all others are local source edits.
- Documentation-only findings (T14, T16, T33, T34, T36, T39) have no RED tests per the "Non-protocol documentation-only tasks may omit tests" rule.
- Net ROM impact: approximately -12 KB (T023–T024). Net RAM impact: 0 bytes.
- Task IDs T037a/b and T048a/b use letter suffixes to indicate atomic sub-tasks of the same audit finding (split per analysis finding A1/A2); T037b depends on T037a (header field must exist before usage).
