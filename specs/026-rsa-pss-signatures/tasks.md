---
description: "Task list for feature 026 — SecurityPolicy-aware RSA-PSS signatures"
---

# Tasks: SecurityPolicy-aware RSA-PSS Signatures

**Input**: Design documents from `/specs/026-rsa-pss-signatures/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Mandatory for the signature/StatusCode/handshake changes (Constitution IV).
RED first where behavior changes; the PKCS#1.5 policies rely on the existing
byte-identical regression suite.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1 (PSS works) / US2 (PKCS#1.5 unchanged) / US3 (algorithm validation)

## Path Conventions

security `src/security/` · core `src/core/` · tests `tests/integration/`.

---

## Phase 1: Setup

- [x] T001 Capture baselines before edits: `scripts/measure_size.sh all` (record nano/micro/embedded/full `.text`) and confirm the current host suite is green, so the byte-identical / gate checks have a reference.

## Phase 2: Foundational (blocking prerequisites)

**The selector is the single source of truth all three sites depend on.**

- [x] T002 [P] Add `mu_security_policy_asym_signature_uri(policy)` and `mu_security_policy_uses_pss(policy)` to `src/security/security_policy.c` + declarations in `src/security/security_policy.h`, keyed off the existing POLICY_TABLE: xmldsig URI + PSS=false for Basic256Sha256/Aes128_Sha256_RsaOaep, `http://opcfoundation.org/UA/security/rsa-pss-sha2-256` + PSS=true for Aes256_Sha256_RsaPss, NULL/false for None/Invalid. OPC-10000-7.
- [x] T003 Add `mu_asym_signature_sign(...)` and `mu_asym_signature_verify(...)` to `src/security/certificate.c` + declarations in `src/security/certificate.h`, dispatching by `mu_security_policy_uses_pss` to `rsa_pss_sha256_*` vs `rsa_sha256_*` and failing closed (`Bad_SecurityChecksFailed`) when the required adapter fn is NULL. Per `contracts/asym-signature-selection.md`.

**Checkpoint**: selector + wrappers compile; existing suite still green (no call sites changed yet).

---

## Phase 3: User Story 1 — Aes256_Sha256_RsaPss handshake works (Priority: P1) 🎯 MVP

**Goal**: The PSS policy signs/verifies with PSS and advertises the PSS URI.
**Independent test**: a full PSS handshake with a PSS-signing client succeeds.

- [x] T004 [US1] Route ServerSignature emission in `handle_create_session` (`src/core/service_dispatch.c` ~L851) through `mu_asym_signature_sign(cr, channelPolicy, ...)` and write `algorithm = mu_security_policy_asym_signature_uri(channelPolicy)` instead of `rsa_sha256_sign` + hardcoded `SIG_URI`. OPC-10000-4 §5.6.3.
- [x] T005 [US1] Route the application ClientSignature verification in `verify_activate_client_signature` (`src/core/service_dispatch.c` ~L402) through `mu_asym_signature_verify(cr, channelPolicy, channel_client_cert, ...)`. OPC-10000-4 §7.38.2.
- [~] T006 [US1] DEFERRED (out of scope) — Route the x509 user-identity-token signature verification (i=327 path, `src/core/service_dispatch.c` ~L1171) through `mu_asym_signature_verify(cr, channelPolicy, cert_token cert, ...)`. Note the UserTokenPolicy boundary from research Decision 4 in a code comment. OPC-10000-4 §5.7.3.
- [x] T007 [US1] Update `tests/integration/test_secure_handshake_modern.c`: for the `Aes256_Sha256_RsaPss` run, sign the ClientSignature with `client_crypto.rsa_pss_sha256_sign` (not PKCS#1.5) and assert the CreateSession ServerSignature `algorithm` is the PSS URI. (This is the RED→GREEN for the PSS switch.)
- [x] T008 [US1] Run `ctest -R 'secure_handshake_modern|server_handshake_secure'` (green) + ASAN on those tests; confirm the PSS handshake completes end-to-end.

**Checkpoint**: US1 shippable — the Aes256_Sha256_RsaPss policy is conformant.

---

## Phase 4: User Story 2 — PKCS#1.5 policies unchanged (Priority: P1)

**Goal**: Basic256Sha256 / Aes128 / None are byte-identical.
**Independent test**: regression fixtures + those handshake tests unchanged.

- [x] T009 [US2] Run `ctest -R 'response_regression|server_handshake_secure|secure_handshake_modern'` and confirm Basic256Sha256 (server_handshake_secure) + Aes128 (secure_handshake_modern aes128 sub-test) produce identical ServerSignature algorithm/bytes as the pre-feature build; None path unchanged. (No code change — this is the regression guard for T004–T006.)

**Checkpoint**: US2 verified — no regression on the shipping policies.

---

## Phase 5: User Story 3 — ClientSignature algorithm validated (Priority: P2)

**Goal**: A ClientSignature declaring the wrong algorithm URI for the policy is rejected.
**Independent test**: PSS channel + PKCS#1.5-URI ClientSignature → rejected.

- [x] T010 [US3] In `verify_activate_client_signature` (`src/core/service_dispatch.c`), before verifying, compare the decoded `algorithm` URI (exact length + bytes) against `mu_security_policy_asym_signature_uri(channelPolicy)`; on mismatch return `Bad_SecurityChecksFailed`. Guard against short/oversized URIs (length checked first). FR-005.
- [x] T011 [US3] Add a negative test in `tests/integration/test_secure_handshake_modern.c`: on an `Aes256_Sha256_RsaPss` channel, an ActivateSession whose ClientSignature carries the PKCS#1.5 algorithm URI (or PKCS#1.5 bytes) is rejected (session not activated → follow-up Read faults, mirroring the existing tamper test).

**Checkpoint**: US3 shippable — algorithm confusion rejected.

---

## Phase 6: Polish & Cross-Cutting

- [x] T012 Global gate sweep: full `ctest`, ASAN/UBSan suite, `scripts/measure_size.sh all` (`.text` ≤ +3%, `data+bss` ≤ +5%, no new heap), `scripts/check_build_matrix.sh` (four ARM profiles). Compare against T001 baseline.
- [x] T013 [P] clang-format all touched files; add the new `certificate.c`/`security_policy.c` symbols to `docs/traceability/` if the traceability test requires it; note the PSS conformance fix in `docs/conformance/profile-embedded.md` (still profile-targeting).
- [x] T014 [P] Update project memory (feature 026): the policy-keyed signature selector, fail-closed dispatch, and the UserTokenPolicy boundary note.

---

## Dependencies & Execution Order

- **Setup (T001)** → **Foundational (T002, T003)** → user stories.
- **T002 blocks T003** (wrappers use the `uses_pss` predicate) and both block T004–T006/T010.
- **US1 (T004–T008)** is the MVP. **US2 (T009)** is a verification-only guard over US1's edits. **US3 (T010–T011)** depends on the selector (T002) and the ClientSignature site (T005) existing.
- **Polish (T012–T014)** last.

## Parallel Opportunities

- T002 and T003 are sequential (T003 uses T002). T004/T005/T006 touch the same file (`service_dispatch.c`) so run sequentially. T013/T014 are `[P]` (docs/memory, separate from code).

## MVP Scope

**MVP = Setup + Foundational + US1** — the PSS policy actually working end-to-end.
US2 is a regression gate; US3 is hardening.

## Implementation Strategy

Land the selector (T002/T003), switch the three sites (T004–T006) with the PSS test
(T007) proving GREEN, confirm the PKCS#1.5 regression (T009), then add the
algorithm-validation hardening (T010/T011). Every step holds the resource gates and
keeps the unchanged policies byte-identical.
