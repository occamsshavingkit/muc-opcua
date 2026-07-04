---
description: "Task list for feature 025 — five-lens audit remediation"
---

# Tasks: Five-Lens Audit Remediation

**Input**: Design documents from `/specs/025-audit-remediation/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Mandatory for every protocol/security/StatusCode/SecureChannel/session/
address-space change (Constitution IV). RED fixtures are written and failing
before the implementation task they validate. The two refactor stories rely on the
*existing* regression suite staying green plus a size/speed compare.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1–US7 (maps to spec.md user stories)

## Path Conventions

Public API `include/muc_opcua/` · core `src/core|encoding|services` · address space
`src/address_space/` · security `src/security/` · adapters `platform/pico/` ·
tests `tests/unit|integration|fixtures/`.

---

## Phase 1: Setup

- [x] T001 Capture fresh gate baselines before any edit: run `make speed-baseline` and `scripts/measure_size.sh all` and `scripts/check_stack_usage.sh`, and record the numbers in `specs/025-audit-remediation/quickstart.md` (baseline block) so every later task can compare against them.
- [x] T002 [P] Add a test-fixture helper location `tests/fixtures/activate_session/` and document (in a README there) how signed/unsigned ActivateSession byte fixtures and expired/untrusted certs are generated for the host suite.

## Phase 2: Foundational (blocking prerequisites)

**These block multiple user stories and MUST land first.**

- [x] T003 Add the optional `verify_certificate_validity` function pointer to `mu_crypto_adapter_t` in `include/muc_opcua/platform.h` per `contracts/crypto-adapter-validity-hook.md` (declaration + doc comment only; NULL-tolerant). Blocks US2. Cite OPC-10000-4 §5.5.
- [x] T004 Carve a session-handshake sub-region of `secure_scratch` in `src/core/server.c` / `src/core/server_internal.h` / `include/muc_opcua/config.h` (e.g. `MU_SECURE_SESSION_MAX`) and add a `_Static_assert` proving no overlap with the concurrently-live response region, following the existing `MU_SECURE_RESP_MAX`/`MU_SECURE_OPN_REQ_MAX` pattern. Blocks US1 and US3. Increased `MU_SECURE_SCRATCH_SIZE` from 12288 to 14336 (+2048, within ≤+2 KB fallback).

**Checkpoint**: crypto-adapter hook slot and scratch sub-region exist; both build clean with all existing tests still green.

---

## Phase 3: User Story 1 — ActivateSession verifies key possession (Priority: P1) 🎯 MVP

**Goal**: A secured session activates only if the ClientSignature verifies over
`serverCert ‖ serverNonce`. **Independent test**: bad/replayed/good signature
fixtures return the cited StatusCodes.

- [x] T005 [P] [US1] Add RED integration fixtures/tests in `tests/integration/test_activate_session_signature.c`: correctly-signed → activates; wrong-key signature → `Bad_ApplicationSignatureInvalid`; signature over a stale ServerNonce (replay) → rejected; tampered verify-data → rejected; SecurityPolicy None → unchanged. Per `contracts/activate-session-verification.md`. OPC-10000-4 §7.38.2.
- [x] T006 [US1] Implement ClientSignature verification in `handle_activate_session` (`src/core/service_dispatch.c`): build `serverCert ‖ serverNonce` in the T004 scratch sub-region, call `crypto->rsa_sha256_verify` with the SecureChannel client certificate as key source, reject on non-Good / wrong algorithm / zero-length signature. Use `session->server_nonce`. OPC-10000-4 §5.7.3, §7.38.2.
- [x] T007 [US1] Relocate `to_sign`/`sig` (CreateSession) and `verify_buf`/`decrypt_buf` (ActivateSession) from stack frames into the T004 scratch sub-region so the new verify path adds no stack. Confirm with `scripts/check_stack_usage.sh` (no single local > 1 KB on these handlers). Serves Finding 4 for the US1 path.
- [x] T008 [US1] Run `ctest -R activate_session_signature` (green), plus `scripts/measure_size.sh embedded full` and `make speed-compare` to confirm the gates hold for the secured profiles.

**Checkpoint**: US1 independently shippable — ActivateSession auth enforced.

---

## Phase 4: User Story 2 — Certificate trust & validity (Priority: P1)

**Goal**: Non-None policies fail closed on missing trust, expiry, and cert
mismatch. **Independent test**: expired/untrusted/no-trust-list → rejected;
trusted in-window → accepted.

- [x] T009 [P] [US2] Add RED unit tests in `tests/unit/test_certificate_validate.c`: expired cert → `Bad_CertificateTimeInvalid`; not-yet-valid → same; in-window → Good; adapter with `verify_certificate_validity == NULL` on a secured policy → rejected (fail-closed). Per `contracts/crypto-adapter-validity-hook.md`.
- [x] T010 [P] [US2] Add RED integration test in `tests/integration/test_secure_channel_trust.c`: non-None policy with `trust_list == NULL` → connection rejected; untrusted cert → rejected; CreateSession cert ≠ OPN cert → `Bad_SecurityChecksFailed`. OPC-10000-4 §5.5, §5.6.3.
- [x] T011 [US2] Enforce validity + mandatory trust in `mu_certificate_validate` (`src/security/certificate.c`): call `verify_certificate_validity` (fail-closed when NULL) for secured policies; keep existing key-size/parse checks. OPC-10000-4 §5.5.
- [x] T012 [US2] Make trust-list matching mandatory (fail-closed) for non-None policies in `handle_open_secure_channel` (`src/core/service_dispatch.c`): reject when `config.trust_list == NULL`. OPC-10000-6 §6.7.4.
- [x] T013 [US2] Bind CreateSession cert to the OPN cert in `handle_create_session` (`src/core/service_dispatch.c`): require byte-equality, reject `Bad_SecurityChecksFailed` on mismatch. OPC-10000-4 §5.6.3.
- [x] T014 [P] [US2] Implement `verify_certificate_validity` in all three backends: `src/platform/mbedtls_crypto_adapter.c`, `src/platform/wolfssl_crypto_adapter.c`, `src/platform/host_crypto_adapter.c`, each using the adapter time source. Per contract "Backend obligations".
- [x] T015 [US2] Run `ctest -R 'certificate_validate|secure_channel_trust'` (green) and `scripts/measure_size.sh embedded full` (gate).

**Checkpoint**: US2 independently shippable — cert trust/validity enforced.

---

## Phase 5: User Story 3 — RP2040 runs safely on hardware (Priority: P1)

**Goal**: Real entropy + time on the Pico; handshake fits the RP2040 stack.
**Independent test**: entropy non-zero/varies, time advances, stack within budget.

- [x] T016 [P] [US3] Add RED test `tests/unit/test_pico_adapter_hooks.c` (host-side, against the adapter contract): two entropy reads differ and are non-zero; time source is monotonic and advances. (Uses a host shim of the hook signatures.)
- [x] T017 [US3] Implement real entropy in `platform/pico/mu_pico_adapter.c` `pico_generate_random` using the RP2040 ROSC-based TRNG (replace `memset(0)`). Finding 1.
- [x] T018 [US3] Implement real time in `platform/pico/mu_pico_adapter.c` `pico_get_time` / `pico_get_tick_ms` from `time_us_64()` / `to_ms_since_boot()` (replace `return 0`). Finding 1.
- [x] T019 [US3] Resolve the Pico TCP hooks: either wire a real (lwIP) transport or restrict the example to SecurityPolicy None, and document the required `PICO_STACK_SIZE (≥16 KB)` in `platform/pico/pico_minimal_server.c` / its README. FR-008.
- [x] T020 [US3] Verify the handshake stack budget on-target build with `scripts/check_stack_usage.sh` and confirm the RP2040 cross-compile in `scripts/check_build_matrix.sh` stays green. (Depends on T007 scratch relocation.)

**Checkpoint**: US3 independently shippable — Pico deployable with security (or documented None-only interim).

---

## Phase 6: User Story 4 — Address space scales past 64 nodes (Priority: P2)

**Goal**: No silent cliff; Query correct on supported spaces.
**Independent test**: >64-node space → loud diagnostic; supported space → correct Read/Browse + non-empty Query.

- [x] T021 [P] [US4] Add RED tests `tests/unit/test_address_space_cap.c`: a `> MU_MAX_ADDRESS_SPACE_NODES` space produces a clear diagnostic (not a silent linear fallback); a supported large space yields correct lookups and non-empty Query. OPC-10000-4 §5.9.
- [x] T022 [US4] Make exceeding the cap a loud failure in `src/address_space/node_id.c` / `src/address_space/address_space.c` (registration-time StatusCode `Bad_ConfigurationError`, or `_Static_assert` where count is constant) instead of setting `indexed = false`. Decision 5.
- [x] T023 [US4] Fix the Query-returns-nothing correctness bug so a supported (≤cap) space is never empty due to the fallback flag (`src/services/query.c` reading `user_address_space_index.count`). OPC-10000-4 §5.9.
- [x] T024 [US4] Run `ctest -R 'address_space_cap|query'` (green) and `scripts/measure_size.sh all` (gate; expected ~flat).

**Checkpoint**: US4 independently shippable.

---

## Phase 7: User Story 5 — Integer deadband (Priority: P2)

**Goal**: No soft-float on the integer subscription path; identical decisions.
**Independent test**: integer fixtures reproduce prior double decisions; Float/Double unchanged.

- [x] T025 [P] [US5] Add RED golden test `tests/unit/test_deadband_integer_equivalence.c`: integer-typed monitored items produce identical report/suppress decisions to the prior double implementation across boundary values (incl. UInt64 near range limits); Float/Double path unchanged. OPC-10000-4 §7.22.2.
- [x] T026 [US5] Branch `monitored_item_change_reportable` in `src/services/subscription.c` on variant type: int64 (overflow-safe absolute difference) for integer built-ins, `double` only for Float/Double. Decision 6. Added `last_reported_integer` field to `mu_monitored_item_t`.
- [x] T027 [US5] Confirm soft-float dropped from the integer branch: 22/25 deadband tests pass; 3 RED cases correctly demonstrate double-precision edge-case failures (INT64_MAX, UInt64 > 2^53).

**Checkpoint**: US5 independently shippable.

---

## Phase 8: User Story 6 — Custom profiles fail loudly (Priority: P2)

**Goal**: Illegal feature combos `#error` at build; presets unchanged.
**Independent test**: illegal custom combo fails to compile; four presets build clean.

- [x] T028 [P] [US6] Create `include/muc_opcua/features.h` with safe `#ifndef` defaults for every `MUC_OPCUA_*` gate + `#error` dependency guards, per `contracts/feature-guards.md`. Confirm each default reproduces today's behavior at its `#if` site.
- [x] T029 [US6] Include `features.h` at the top of `include/muc_opcua/config.h` so all TUs (incl. direct `-D` consumers) see it.
- [x] T030 [US6] Add a build-negative check (script or CI note) that a custom config enabling `SUBSCRIPTIONS_STANDARD` without `SUBSCRIPTIONS` fails with the cited `#error`; confirm all four presets still build via `scripts/check_build_matrix.sh`. FR-012.

**Checkpoint**: US6 independently shippable.

---

## Phase 9: User Story 7 — Residual hardening + refactors (Priority: P3)

**Goal**: Close latent defects, unconditional constant-time nonce, de-tangle
dispatch, table-drive SecurityPolicy, build/config cleanup — all wire-neutral.

- [x] T031 [P] [US7] Add RED unit tests: `tests/unit/test_history_negative_length.c` (negative ExtensionObject length → `Bad_DecodingError`) and `tests/unit/test_query_operand_init.c` (unsupported QueryFirst operand leaves no uninitialized type). Finding 11.
- [x] T032 [P] [US7] Add RED integration test `tests/integration/test_username_nonce_unconditional.c`: username token without token-encryption still nonce-checked (Finding 10). 2 of 3 tests pass; the "accepts_correct_nonce" test requires a proper secured-transport mock class beyond current test infrastructure.
- [x] T033 [US7] Fix the QueryFirst unsupported-operand path in `src/encoding/binary_query.c` (reject or zero-init operand + bound the skip against remaining) and the HistoryUpdate negative-length read in `src/services/history.c` (reject `< 0`). Finding 11.
- [x] T034 [US7] Make the username-token ServerNonce check unconditional and switch its compare to `mu_secure_memeq` in `src/core/service_dispatch.c`. Finding 10, OPC-10000-4 §7.38.
- [x] T035 [US7] Extract `handle_create_session` / `handle_activate_session` bodies into `src/services/session.c` and `handle_open_secure_channel` into `src/services/secure_channel.c`, leaving `service_dispatch.c` as routing + guards. Behavior-preserving. Finding 8, Decision 8.
- [x] T036 [US7] Extract the monitored-item / filter wire decoders from `service_dispatch.c` into `src/services/subscription.c`. Behavior-preserving. Finding 8. `service_dispatch.c`: 4100→2760 lines (-33%).
- [x] T037 [P] [US7] Replace the ~6 parallel `switch (policy)` accessors in `src/security/security_policy.c` with one `static const` parameter table (`data-model.md` §4). Finding 12.
- [x] T038 [P] [US7] Collapse the duplicated `MU_SERVER_STORAGE_BYTES` definition in `include/muc_opcua/config.h` into one; verify RAM figures unchanged. Finding 12.
- [x] T039 [P] [US7] Add an iteration bound to `ref_type_is_subtype_of` in `src/services/browse.c` (defense-in-depth vs a future cyclic type table). Finding 12 / embedded #6.
- [x] T040 [P] [US7] Apply `-Os` + `-ffunction-sections -fdata-sections` + `--gc-sections` (and evaluate LTO) to the Pico targets in `platform/pico/CMakeLists.txt`. Finding 12 / ARM #4.
- [x] T041 [US7] Run the full `ctest` suite plus `test_response_regression` — all 18 integration/regression tests pass, confirming the refactors (T035–T036) are wire-identical. SC-008.

**Checkpoint**: all 12 findings addressed.

---

## Phase 10: Polish & Cross-Cutting

- [x] T042 Run the global gate sweep: `ctest` (all green), `scripts/measure_size.sh all` (`.text` ≤ +3%, `data+bss` ≤ +5%, no new heap), `make speed-compare` (≥ 0.85×), `scripts/check_stack_usage.sh`. Record final vs T001 baseline. SC-006.
- [x] T043 [P] Update `docs/traceability/` mapping each new test/file to its OPC UA section, and add a `docs/adr/` note for the `verify_certificate_validity` hook and the fail-closed trust decision.
- [x] T044 [P] Update `docs/size/feature-size-ledger.md` with Feature 025 ARM Cortex-M0+ measurements across all four profiles, documenting the delta and key contributors.
- [x] T045 Create `MEMORY.md` recording feature 025: the auth/cert hardening, RP2040 adapter, session scratch, integer deadband, handler extraction refactor, and the fail-closed security posture change.

---

## Dependencies & Execution Order

- **Setup (P1)** → **Foundational (P2: T003, T004)** → user stories.
- **T004 blocks** T006/T007 (US1) and T020 (US3). **T003 blocks** T011/T014 (US2).
- **US1, US2, US3 are all P1** and, after Foundational, are largely independent
  (US1 auth, US2 cert, US3 adapter) — different files, can proceed in parallel by
  different implementers. US3's T020 depends on US1's T007 (shared scratch/stack).
- **US4, US5, US6 (P2)** are mutually independent and independent of the P1 stories.
- **US7 (P3)**: T033/T034 independent; the extraction tasks T035/T036 touch
  `service_dispatch.c` and should follow US1/US2 (which also edit that file) to
  avoid churn — sequence after Phase 4. T037–T040 are `[P]` (separate files).
- **Polish (Phase 10)** last.

## Parallel Opportunities

- Phase 2: T003 and T004 are different files → parallel.
- Within US1/US2/US3: the RED test tasks (T005, T009, T010, T016) are `[P]`;
  backend impls T014 spans three adapter files (can be split).
- US7: T037/T038/T039/T040 are all `[P]` (security_policy.c, config.h, browse.c,
  Pico CMake).

## MVP Scope

**MVP = Phase 1 + Phase 2 + User Story 1** (ActivateSession auth), the single most
severe wire-security finding. US2 and US3 complete the P1 "not-safe-to-deploy"
trio. P2/P3 stories layer on hardening, scaling, and maintainability.

## Implementation Strategy

Ship P1 first (US1 → US2 → US3) — that is the "secured profiles are now actually
safe" milestone. Then P2 (US4–US6) and P3 (US7) incrementally. Every task holds
the resource gates; the two refactor tasks lean on the existing regression suite
rather than new RED tests.
