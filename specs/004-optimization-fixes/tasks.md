---
description: "Task list for Optimization Audit Remediation"
---

# Tasks: Optimization Audit Remediation

**Input**: Design documents from `/specs/004-optimization-fixes/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: MANDATORY for this feature — every slice touches protocol parsing,
serialization, service dispatch, StatusCodes, SecureChannel/session state, or
security behavior (Constitution IV). Protocol tests/fixtures precede the
implementation they validate. Behaviour-preserving work is guarded by
byte-for-byte regression vectors (SC-003).

**Organization**: Grouped by the four prioritized user stories (US1 P1 → US4 P3).
Each story is an independently shippable, independently testable slice.

## Path Conventions

- Public API: `include/micro_opcua/` · Core: `src/core/`, `src/encoding/`, `src/services/`
- Address space: `src/address_space/` · Security: `src/security/`
- Tests: `tests/unit/`, `tests/integration/`, `tests/fixtures/`, `tests/fuzz/`
- Docs: `docs/traceability/`, `docs/size/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Add the configuration knobs and traceability scaffold the slices depend on.

- [X] T001 Add new compile-time knobs with documented defaults to `include/micro_opcua/config.h`: `MU_MAX_ADDRESS_SPACE_NODES`, `MU_CIPHER_CTX_SIZE`, `MU_SECURE_SCRATCH_SIZE`, `MICRO_OPCUA_STATUS_STRINGS` (default OFF) — all `-D`-overridable (data-model.md "Configuration knobs").
- [X] T002 [P] Create traceability doc `docs/traceability/004-optimization-fixes.md` mapping each behavioural change to its OPC section — OPC-10000-4 §5.6.2.2, §5.7.3.2, §5.9.2, §7.38.2 (StatusCodes) and OPC-10000-6 §5.2, §5.2.2.9 (NodeId) — with a row per FR (skeleton; filled per task).
- [X] T003 [P] Create the feature fixture directory `tests/fixtures/activate_session/` and a README noting fixtures cite OPC-10000-4 §5.7.3.2.
- [X] T004 [P] Register a `MICRO_OPCUA_LTO` CMake option stub (default OFF) in `cmake/MicroOpcUaOptions.cmake` (implementation wired in US4/T058).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared verification harnesses that multiple stories depend on.

**CRITICAL**: Complete before the user-story phases begin.

- [X] T005a Add a worst-case stack-usage measurement + CI check target to the build (e.g. `-fstack-usage` aggregation or `-Wstack-usage=<N>`) for SC-001.
- [X] T005b Record the pre-change secured-OPN stack baseline (~12.8 KiB) from T005a into `docs/size/feature-size-ledger.md` for SC-001 comparison. (Not [P]: shares `feature-size-ledger.md` with T007.)
- [X] T006 [P] Add a byte-identical response-capture harness in `tests/integration/test_response_regression.c` that records encoded Read/Browse/subscription responses for fixed inputs (golden vectors) — the SC-003 backstop reused by US3 and US4.
- [ ] T007 Snapshot current per-profile `size` figures into `docs/size/feature-size-ledger.md` as the "pre-remediation" baseline row for SC-005. (Not [P]: shares `feature-size-ledger.md` with T005b.)
- [X] T008 Confirm the existing no-heap check (`tests/unit/test_no_heap_lifecycle.c`) runs for the micro/embedded profiles so FR-019 is enforced throughout.

**Checkpoint**: Foundation ready — user-story work can begin.

---

## Phase 3: User Story 1 — Crash-safety & parser correctness (Priority: P1) [MVP]

**Goal**: Malformed or edge-case-but-legal input cannot crash, overflow the stack, or desync the decoder. Corrects ActivateSession decoding and OPN policy handling toward the spec; relocates secure-path stack scratch.

**Independent Test**: New malformed/edge-case unit + fuzz tests return cited StatusCodes or process correctly; secured-OPN worst-case stack ≤ 10 KiB.

### Tests for User Story 1 (write first, confirm they fail)

- [X] T009 [P] [US1] Add binary fixture: ActivateSession with a **non-empty** `clientSoftwareCertificates` array and a populated UserIdentityToken, citing OPC-10000-4 §5.7.3.2, in `tests/fixtures/activate_session/nonempty_certs.bin`.
- [X] T010 [P] [US1] Add ActivateSession decode test (correct consumption of cert array + token body + signature, session activates) in `tests/unit/test_service_dispatch.c` (FR-002).
- [X] T011 [P] [US1] Add NodeId identifier-type-confusion test (non-numeric/non-ns0 token & request type ids return `Bad_DecodingError`, and an unsupported ActivateSession identity-token type returns `Bad_IdentityTokenInvalid`, no union mis-read), citing OPC-10000-6 §5.2.2.9 + OPC-10000-4 §7.38.2, in `tests/unit/test_service_state_errors.c` (FR-004).
- [X] T011b [P] [US1] Add subscription op-cap test: a request exceeding the per-op cap returns `Bad_TooManyOperations` (OPC-10000-4 §7.38.2) before any response array length is emitted, in `tests/unit/test_dispatch_services.c` (FR-006). Confirm it fails before T024.
- [X] T012 [P] [US1] Add OpenSecureChannel policy/mode mismatch test (inconsistent `securityPolicyUri`/`securityMode` handled per OPC-10000-4 §5.6.2.2) in `tests/unit/test_secure_channel.c` (FR-005).
- [X] T013 [P] [US1] Add overflow-safe bound test (length near `SIZE_MAX` rejected with `Bad_DecodingError`, no wrap), citing OPC-10000-6 §5.2 + OPC-10000-4 §7.38.2, in `tests/unit/test_binary_primitives.c` (FR-003).
- [X] T014 [P] [US1] Add fuzz target `tests/fuzz/fuzz_activate_session.c` covering cert-array/identity-token-bearing ActivateSession frames (OPC-10000-4 §5.7.3.2).
- [X] T015 [US1] Add a secured-OPN stack-budget assertion wired to the T005a measurement (fails if > 10 KiB) (FR-001/SC-001).

### Implementation for User Story 1

- [X] T016a [P] [US1] Confirm via the OPC UA reference the exact StatusCode + section for **rejected SecurityPolicy/mode** (candidates `Bad_SecurityPolicyRejected`/`Bad_SecurityModeRejected`, OPC-10000-4 §7.38.2 / §5.6.2.2 — research D5); record in `docs/traceability/004-optimization-fixes.md`.
- [X] T016b [US1] Confirm via the OPC UA reference the exact StatusCode + section for **NodeId type confusion** (`Bad_DecodingError` / `Bad_IdentityTokenInvalid`, OPC-10000-6 §5.2.2.9 + OPC-10000-4 §7.38.2 — FR-004); record in `docs/traceability/004-optimization-fixes.md`. (Not [P]: shares the traceability doc with T016a/T016c.)
- [X] T016c [US1] Confirm via the OPC UA reference the exact StatusCode + section for **subscription op-cap exceeded** (`Bad_TooManyOperations`, OPC-10000-4 §7.38.2 — FR-006); record in `docs/traceability/004-optimization-fixes.md`. (Not [P]: shares the traceability doc with T016a/T016b.)
- [X] T017a [P] [US1] Make `ensure_bytes` bound checks overflow-safe (`count > length - position`) per OPC-10000-6 §5.2, returning `Bad_DecodingError` (OPC-10000-4 §7.38.2), in `src/encoding/binary_reader.c` (FR-003, contract: binary-codec-sticky-status.md).
- [X] T017b [P] [US1] Make `ensure_space` bound checks overflow-safe (`count > length - position`) per OPC-10000-6 §5.2 in `src/encoding/binary_writer.c` (FR-003, contract: binary-codec-sticky-status.md).
- [X] T018 [US1] In `handle_activate_session` (`src/core/service_dispatch.c`): consume each `SignedSoftwareCertificate` entry, validate+skip the UserIdentityToken `token_body_len`, and consume `UserTokenSignature` so following fields decode from correct offsets; cite OPC-10000-4 §5.7.3.2 in comments (FR-002).
- [X] T018b [US1] In `handle_activate_session` (`src/core/service_dispatch.c`): guard `token_type` on `MU_NODEID_NUMERIC`, returning `Bad_IdentityTokenInvalid` on an unsupported type; cite OPC-10000-6 §5.2.2.9 (NodeId) + OPC-10000-4 §7.38.2 (StatusCode) in comments (FR-004).
- [X] T019a [US1] Guard the request type-id NodeId in `src/core/server.c` (where it is decoded before `mu_service_dispatch`): if not ns0-numeric, reject with `Bad_DecodingError` (OPC-10000-6 §5.2.2.9 + OPC-10000-4 §7.38.2) (FR-004).
- [X] T019b [US1] Guard the session authentication-token id NodeId in `src/core/service_dispatch.c`: if not ns0-numeric, reject without mis-reading the union (OPC-10000-6 §5.2.2.9 + OPC-10000-4 §7.38.2) (FR-004).
- [X] T020a [US1] Change `mu_secure_channel_open` to accept the decoded `securityPolicyUri`/`securityMode` (signature + body) in `src/services/secure_channel.h` and `src/services/secure_channel.c` (FR-005, OPC-10000-4 §5.6.2.2).
- [X] T020b [US1] Decode and pass `securityPolicyUri`/`securityMode` into `mu_secure_channel_open` (currently `NULL`) from the OPN handler in `src/core/service_dispatch.c` and `src/core/server.c` (FR-005, OPC-10000-4 §5.6.2.2).
- [X] T020c [US1] Reject an inconsistent policy/mode combination with the T016a StatusCode in `src/services/secure_channel.c`, preserving the None-only build path (FR-005, OPC-10000-4 §5.6.2.2).
- [X] T021 [US1] Add `secure_scratch[MU_SECURE_SCRATCH_SIZE]` to `struct mu_server` (guarded by `MICRO_OPCUA_SECURITY`) in `src/core/server_internal.h` (data-model.md E4).
- [X] T022 [US1] Relocate the OPN-path scratch (`respbody`/`opn_buf`) from the stack to `server->secure_scratch` in `src/core/server.c` (FR-001/D1).
- [X] T023a [US1] Right-size `MU_ASYM_SCRATCH` (4096 → real RSA block, 256 B for 2048-bit) in `src/security/asym_chunk.c` (FR-001/D1).
- [X] T023b [US1] Move `plain`/`sign_buf`/`verify_buf` off the deep stack (into server-owned/shared scratch) in `src/security/asym_chunk.c` (FR-001/D1).
- [X] T024 [US1] Add explicit op caps to the subscription per-op request loops before emitting array lengths in `src/core/service_dispatch.c`, consistent with `MU_DISPATCH_MAX_*`, returning `Bad_TooManyOperations` (OPC-10000-4 §7.38.2) when exceeded (FR-006).
- [X] T025a [US1] Update `docs/traceability/004-optimization-fixes.md` (FR-001/002/003/004/005/006 rows) mapping each change + test to its OPC UA section.
- [X] T025b [US1] Record the new secured-OPN stack figure in `docs/size/feature-size-ledger.md`. (Not [P]: shared ledger file.)

**Checkpoint**: US1 independently testable; stack ≤ 10 KiB; parser robust to the new vectors.

---

## Phase 4: User Story 2 — Secret zeroization (Priority: P2)

**Goal**: No derived keys, KDF intermediates, plaintext, MACs, signatures, or nonces remain in RAM after use or channel reuse.

**Independent Test**: Channel reuse test asserts key storage reads zero after close; crypto helpers scrub stack secrets.

### Tests for User Story 2

- [X] T026 [P] [US2] Add a channel-reuse zeroization test (OPN → MSG → close, assert `client_keys`/`server_keys` and cipher-ctx storage read all-zero) in `tests/unit/test_secure_channel.c` (FR-007, contract: secure-zero.md).

### Implementation for User Story 2

- [X] T027 [US2] Add a non-elidable `mu_secure_zero(void*, size_t)` (volatile byte-store loop) in new files `src/security/secure_zero.c` + `src/security/secure_zero.h` (FR-008, contract: secure-zero.md).
- [X] T028 [US2] Zeroize `client_keys`/`server_keys` (and cipher-ctx storage if present) in `mu_secure_channel_init` and `mu_secure_channel_close` in `src/services/secure_channel.c` (FR-007).
- [X] T029 [P] [US2] Scrub secret stack intermediates before every return in `src/security/sym_chunk.c` (FR-008).
- [X] T030 [P] [US2] Scrub secret stack intermediates before every return in `src/security/key_derivation.c` (FR-008).
- [X] T031 [P] [US2] Scrub decrypted plaintext / signature / verify buffers before every return in `src/security/asym_chunk.c` (FR-008).
- [X] T032 [US2] Update `docs/traceability/004-optimization-fixes.md` (FR-007/008 rows).

**Checkpoint**: US1 + US2 both work independently; secrets provably cleared.

---

## Phase 5: User Story 3 — Hot-path performance (Priority: P2)

**Goal**: Sub-linear node resolution, zero per-tick lookups, one AES key schedule per channel, single-pass Browse, divide/ memmove fixes — all byte-identical.

**Independent Test**: Index property test (indexed == linear); sampling does zero node lookups/tick; cipher schedule once per channel; SC-003 golden vectors unchanged.

### Tests for User Story 3

- [X] T033 [P] [US3] Add address-space index property test (indexed lookup === linear lookup for all member/non-member NodeIds; comparison count ~log N) in `tests/unit/test_address_space_values.c` (FR-009, contract: address-space-index.md).
- [X] T034 [P] [US3] Add a sampling-tick probe test asserting zero `mu_address_space_find_node` calls per tick (resolved-node cached) in `tests/integration/test_subscriptions.c` (FR-010).
- [X] T035 [P] [US3] Add a cipher-context probe test (stub adapter counter proves key schedule runs once per channel vs once per message in fallback) in `tests/unit/test_sym_chunk.c` (FR-012, contract: crypto-adapter-cipher-context.md).
- [X] T036 [P] [US3] Add Browse single-pass byte-identical test (output equals two-pass output, all-or-nothing-per-node preserved) in `tests/unit/test_browse_service.c` (FR-011/FR-014, OPC-10000-4 §5.9.2).

### Implementation for User Story 3

- [X] T037a [US3] Declare the `mu_address_space_index_t` type (sort-key based, `MU_MAX_ADDRESS_SPACE_NODES`-bounded) in `include/micro_opcua/address_space.h` (FR-009/D3, data-model.md E1).
- [X] T037b [US3] Implement binary-search `mu_address_space_find_node` over the index (signature + result identical to the linear version, `mu_nodeid_equal` confirm) in `src/address_space/node_id.c` (FR-009/D3, contract: address-space-index.md).
- [X] T037c [US3] Add the linear-scan fallback path when `node_count > MU_MAX_ADDRESS_SPACE_NODES` in `src/address_space/node_id.c` (FR-009/D3).
- [X] T038 [US3] Wire index build into `mu_address_space_validate`/server init without masking existing validation errors in `src/address_space/address_space.c` (contract: address-space-index.md).
- [X] T039 [US3] Cache the resolved `const mu_node_t *` in `mu_monitored_item_t` at CreateMonitoredItems time and use it in the sampling timer in `src/services/subscription.c` (FR-010, data-model.md E3).
- [X] T040 [US3] Rewrite `mu_browse_process` to a single reference-resolution pass with backpatched count, preserving all-or-nothing-per-node semantics per OPC-10000-4 §5.9.2, in `src/services/browse.c` (FR-011/D7).
- [X] T041 [US3] Add the optional cipher-context members to `mu_crypto_adapter_t` in `include/micro_opcua/platform.h` (additive; contract: crypto-adapter-cipher-context.md).
- [X] T042 [US3] Add per-channel cipher-ctx storage (`cipher_ctx_*`) to `mu_secure_channel_t` (guarded) in `src/services/secure_channel.h` with a `MU_CIPHER_CTX_SIZE` compile-time assert (data-model.md E2).
- [X] T043 [US3] Initialize the cipher context at OPN (where keys derive) and use the ctx AES path with graceful fallback to stateless `aes256_cbc_*` in `src/security/sym_chunk.c` (FR-012/D2).
- [X] T044 [US3] Implement the cipher-ctx functions in the host adapter `src/platform/host_crypto_adapter.c` (reuse the EVP context instead of per-op new/free) (FR-012/audit-T2).
- [X] T045 [P] [US3] Add the `advance_sample_timer` divide fast-path (`elapsed < interval` ⇒ single add) in `src/services/subscription.c` — behaviour-preserving, no wire change (FR-013/audit-T11).
- [X] T046 [US3] Replace the per-message whole-remainder `memmove` with a read cursor + single compaction in `src/core/server.c` — behaviour-preserving, no wire change (FR-013/audit-T12).
- [X] T047a [US3] Run the T006 golden vectors and confirm Read/Browse/subscription responses are byte-identical (FR-014/SC-003).
- [X] T047b [US3] Update `docs/traceability/004-optimization-fixes.md` (FR-009/010/011/012/013 rows) and the size/RAM record for US3. (Not [P]: shared traceability/ledger files.)

**Checkpoint**: US1–US3 independently functional; performance improved with identical wire output.

---

## Phase 6: User Story 4 — Footprint reduction (Priority: P3)

**Goal**: Smaller Micro core flash, clean disabled-service builds, no behaviour change.

**Independent Test**: Every profile + every single-service-disabled permutation builds under warnings-as-errors; Micro core ≥ 8% smaller; SC-003 vectors unchanged.

### Tests for User Story 4

- [X] T048 [P] [US4] Add a build-matrix check script `scripts/check_build_matrix.sh` that compiles every single optional-service-disabled permutation under warnings-as-errors and asserts the disabled service is absent (`nm`) (FR-018/audit-T17).
- [X] T049 [P] [US4] Re-run the T006 golden vectors after the size refactors to confirm byte-identical output (SC-003/FR-014).

### Implementation for User Story 4

- [X] T050 [US4] Add a sticky `status` field to `mu_binary_reader_t`/`mu_binary_writer_t` in `include/micro_opcua/encoding.h`; primitives no-op once tripped; `ensure_*` still run (FR-015/D8, contract: binary-codec-sticky-status.md).
- [X] T051 [US4] Convert handlers in `src/core/service_dispatch.c` to a single end-of-handler status check (remove the ~294 per-call checks) (FR-015/D8).
- [X] T052a [P] [US4] Consolidate hand-rolled little-endian pack/unpack into one inline helper used by `src/encoding/binary_writer.c` and `binary_reader.c` (FR-015/audit-T13).
- [X] T052b [US4] Collapse the signed/unsigned writer twins into one width-parameterized writer in `src/encoding/binary_writer.c` (FR-015/audit-T13).
- [X] T053 [P] [US4] Replace the `element_size` switch with a `static const` table in `src/encoding/binary_variant.c` (FR-015/audit-T14).
- [X] T054 [US4] Add a `handler` fn-pointer column to `g_supported_services[]` and delete the parallel dispatch `switch`, keeping per-service `#ifdef` rows, in `src/core/service_dispatch.c` (FR-015/D9).
- [X] T055 [US4] Factor the four per-id array handlers (set_publishing/set_monitoring/delete_monitored_items/delete_subscriptions) into one driver + per-item callback in `src/core/service_dispatch.c` (FR-015/D9).
- [X] T056 [US4] Wrap service-specific static helpers (Browse/Translate, etc.) in their service `#ifdef` so disabled-service builds pass warnings-as-errors in `src/core/service_dispatch.c` (FR-018/audit-T17).
- [X] T057a [US4] Gate `mu_status_name` behind `MICRO_OPCUA_STATUS_STRINGS` (default OFF) in `src/core/status.c` (FR-016/D9).
- [X] T057b [US4] Delete the dead, stale `g_unsupported_services[]`/`mu_is_unsupported_service` (table + function + declaration) in `src/core/status.c` (FR-016/D9). (Not [P]: shares `status.c` with T057a.)
- [X] T058 [US4] Wire `MICRO_OPCUA_LTO` to `INTERPROCEDURAL_OPTIMIZATION` (default OFF) in `cmake/MicroOpcUaCodegen.cmake` (FR-017/D10).
- [X] T059a [US4] Rebuild all profiles, measure `size`, and confirm Micro core ≥ 8% smaller than the T007 baseline (SC-005).
- [X] T059b [US4] Refresh measured figures in `docs/size/feature-size-ledger.md` and update `docs/traceability/004-optimization-fixes.md` (FR-015/016/017/018 rows). (Not [P]: shared traceability/ledger files.)

**Checkpoint**: All four stories complete; flash reduced; behaviour unchanged.

---

## Phase 7: Compliance, Size & Polish

- [ ] T060 Run host unit + integration tests for every build profile (`ctest`).
- [ ] T061 [P] Run fuzz targets (incl. the US1 additions) for a fixed run budget.
- [ ] T062 [P] Run the ASan/UBSan build green (FR-020/SC-002).
- [ ] T063 [P] Run clang-format + cppcheck/clang-tidy with warnings-as-errors.
- [ ] T064 Run the Pico SDK cross-compile (embedded gate).
- [ ] T065 Confirm the no-heap check passes for all profiles (FR-019/SC-006).
- [ ] T066 Verify final secured-OPN stack ≤ 10 KiB and the size ledger reflects the ≥ 8% Micro reduction (SC-001/SC-005).
- [ ] T067 Verify `docs/traceability/004-optimization-fixes.md` maps every behavioural change + test to OPC UA sections (Constitution IV).
- [ ] T068 Validate `quickstart.md` end-to-end on host.

---

## Dependencies & Execution Order

### Phase dependencies
- Setup (P1) → Foundational (P2) → User Stories (P3–P6) → Polish (P7).
- US1 (P1) is the MVP and should land first. US2/US3/US4 each depend only on Foundational.

### Cross-story coupling to watch
- **`service_dispatch.c`** is edited by US1 (T018, T018b, T019, T020b, T024) and US4 (T051, T054–T056) — sequence US4's dispatch refactor **after** US1 to avoid churn/conflict.
- **`asym_chunk.c`/`sym_chunk.c`/`secure_channel.*`** are touched by US1 (stack), US2 (zeroize), and US3 (cipher ctx) — land in priority order (US1 → US2 → US3) to serialize edits.
- The T006 golden-vector harness (Foundational) gates US3 (T047a) and US4 (T049).

### Within each story
- Tests/fixtures before implementation (Constitution IV: must fail first).
- T016a (SecurityPolicy StatusCode) before T020c (OPN rejection); T016b before T018b/T019; T016c before T011b (op-cap test) before T024.
- T020a (signature change) before T020b (pass policy) before T020c (reject).
- T037a (index type) before T037b (lookup) before T037c (fallback); T037b before T038 (build wiring).
- T021 (scratch storage) before T022 (relocation) and before T023b (asym relocate).
- T041/T042 (interface + storage) before T043/T044 (use + adapter impl).

### Parallel opportunities
- Setup T002/T003/T004 [P]; Foundational T006/T007 [P].
- US1 tests T009–T014 + T011b [P] (distinct files); StatusCode confirmation T016a [P] (T016b/T016c are sequential — shared traceability doc); bound-check splits T017a/T017b [P]; US2 helpers T029/T030/T031 [P]; US3 tests T033–T036 [P]; US4 T052a/T053 [P] (T057a/T057b sequential — shared `status.c`).
- After Foundational, US2 and US3's non-`service_dispatch.c` work can proceed in parallel with US1's later tasks if different files (watch the security-file coupling above).

---

## Notes
- [P] = different files, no incomplete dependency. [Story] maps task → user story.
- Every protocol/security change cites OPC UA part/section in code + traceability.
- Behaviour-preserving (US3/US4) work is gated by the SC-003 golden vectors.
- Commit after each task or logical group; stop at checkpoints to validate behaviour, conformance traceability, and size.
- No hidden heap (FR-019), no uncited wire-visible behaviour, no cross-story dependency that breaks independent testing.
