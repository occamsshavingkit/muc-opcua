<!-- markdownlint-disable MD013 -->

# Tasks: Nano SecurityPolicy-None Crypto Gating

**Input**: Design documents from `specs/072-nano-securitypolicy-none-gating/`
**Prerequisites**: `spec.md`, `plan.md`, `research.md`, `data-model.md`, `contracts/crypto-gate-contracts.md`, `quickstart.md`

**OPC UA Normative References**: OPC-10000-7 §4.3 (NanoEmbeddedDevice: SecurityPolicy None, Anonymous); OPC-10000-6 §6.7 (SecurityPolicy None plaintext UA Secure Conversation), §6.7.2 (MessageChunk framing), §7.2 (OPC UA TCP); OPC-10000-4 §5.6 (SecureChannel Service Set), §5.7 (Session Service Set).

**Organization**: Grouped by user story (US1 = nano ships only None; US2 = secured byte-identical; US3 = conformance honesty). Test-first per Constitution Principle IV: any change to SecureChannel/session/security behaviour lands a failing test first.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: May run in parallel (different files, no dependency on incomplete tasks).

## Phase 1: Setup and Baseline

- [ ] T001 Record pre-change baselines for the byte-identical contract (C4): capture nano ARM `.text` (archive + linked ELF) and micro/embedded/standard/full ARM `.text` into `specs/072-nano-securitypolicy-none-gating/research.md` (or a `baseline.md`) using `scripts/measure_size.sh`, so the −6 KB nano delta and 0-byte secured delta can be proven.
- [ ] T002 Confirm the verified coupling map in `research.md` still matches `HEAD` (re-grep the six call-site guard lines and `src/CMakeLists.txt:450-457`); note any drift before editing.

## Phase 2: Foundational — Kconfig symbol, manifest, generator/validator

- [ ] T003 Add `config MUC_OPCUA_SECURE_CHANNEL_CRYPTO` to `/Kconfig` (bool, independent of `MUC_OPCUA_FACET_CORE_2022_SERVER`), `default y` for the security-capable profile symbols (micro/embedded/standard/full) and default off for nano, with help text citing OPC-10000-7 §4.3. Generated from the manifest (T004), not hand-authored — this task is the manifest-driven emission.
- [ ] T004 Add the gate to `profiles/opcua-profile-manifest.yaml` with `kconfig_symbol: MUC_OPCUA_SECURE_CHANNEL_CRYPTO` and per-profile defaults (nano false; micro/embedded/standard/full true; custom false), grounded to OPC-10000-7 §4.3, and make `MUC_OPCUA_CU_SECURITY_ECC` (and any backend selection) depend on it (research Decision 5).
- [ ] T005 [P] Add manifest-validation coverage in `scripts/profile_manifest/validate.py` (or `test_validate.py`) asserting the gate's nano=off / secured=on defaults and the ECC→crypto-gate dependency.
- [ ] T006 Regenerate all artifacts (`kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs,completion`) and run `validate.py --all`; confirm the new symbol appears with correct defaults and no unrelated drift.

## Phase 3: US1 — Nano ships only what SecurityPolicy None needs (P1, MVP)

**Goal**: nano build excludes the crypto TUs, keeps the None path, and still rejects non-None. **Independent test**: contracts C1, C2, C3 against a gate-off nano build.

### RED (tests first)

- [ ] T007 [US1] Add/confirm a gate-off exclusion test for **C1** (`tests/unit/` or a build-surface check): assert `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` undefined and no crypto-TU symbols linked in a nano build (build-time `_Static_assert` in a crypto call-site file that the gate is required, and/or a profile-surface assertion). RED before the CMake change.
- [ ] T008 [P] [US1] Add a **C3** regression test in `tests/unit/test_secure_channel*.c`: with the crypto gate off, `mu_secure_channel_open` given a non-None policy URI returns `MU_STATUS_BAD_SECURITYPOLICYREJECTED` and mutates no channel state (OPC-10000-4 §5.6). Must pass both before and after (crypto-independent path) — locks FR-004.
- [ ] T009 [US1] Confirm the **C2** None-path suite is wired for the nano gate-off build (`secure_channel`, `session`, `session_auth`, `discovery_*`, `browse_service`, `read_service`, `uasc_framing`, `server_handshake`, `service_dispatch`, `minimal_server_flow`); add any missing nano-build registration.

### GREEN (implementation)

- [ ] T010 [US1] `src/CMakeLists.txt`: select the crypto glob TUs (`sym_chunk.c`, `asym_chunk/{wrap,unwrap}.c`, `certificate.c`, `trustlist.c`, `key_derivation.c` minus the relocated util) on `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` instead of `MUC_OPCUA_FACET_CORE_2022_SERVER` (`:450-457`); keep `MUC_OPCUA_SECURITY`/node content under the facet; gate the OpenSSL adapter (`:475`) and ECC (`:462-466`) on the crypto gate.
- [ ] T011 [P] [US1] Relocate `mu_secure_zero` + `mu_secure_memeq` from `src/cu/core_2022_server/security/key_derivation.c` to an always-compiled utility TU (e.g. `src/security/secure_util.c` + header), update the declaration site (`src/security/key_derivation.h:11-12`), so no-crypto builds keep them (research Decision 4).
- [ ] T012 [US1] Migrate crypto call-site guards `MUC_OPCUA_FACET_CORE_2022_SERVER` → `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` in `src/core/server/data_chunk.c` and `src/core/server/process_message.c`, preserving the None→plaintext fallback (OPC-10000-6 §6.7). Cite lines per research Decision 3.
- [ ] T013 [US1] Migrate crypto call-site guards in `src/core/service_dispatch/osc_handler.c`, `create_session.c`, and `activate_session.c` (classify interleaved `MUC_OPCUA_CU_USER_AUTH`/`MUC_OPCUA_CU_SECURITY_ECC` blocks — only message/asym-crypto/cert/key blocks move; user-token plumbing stays), OPC-10000-4 §5.6/§5.7.
- [ ] T014 [US1] Migrate crypto call-site guards in `src/services/secure_channel.c` (`:57-61`, `:92-116` classification arms, `:163-169`), keeping the None path (`:84-91`) and non-None rejection (`:84-87`, `:117-119`) untouched (OPC-10000-4 §5.6).
- [ ] T015 [US1] Build nano gate-off and link clean (no undefined crypto symbols); run T007/T008/T009 GREEN. Fix any missed guard.

## Phase 4: US2 — Secured profiles byte-identical (P1)

**Goal**: gate on for secured profiles; crypto suite unchanged; ARM `.text` byte-identical (C4).

- [ ] T016 [US2] Build micro/embedded/standard/full with mbedTLS+wolfSSL; assert `MUC_OPCUA_SECURE_CHANNEL_CRYPTO=1` and all crypto TUs compile; run the full crypto suite (`secure_handshake_modern`, `sym_chunk`, `asym_chunk`, `user_auth_encrypted`, `ecc_handshake_e2e`) GREEN.
- [ ] T017 [US2] Prove **C4 byte-identical**: compare each secured profile's ARM `.text` to the T001 baseline; 0-byte delta. Investigate (do not accept) any nonzero delta — the guard swap must be a pure rename for secured builds.

## Phase 5: US3 — Conformance honesty (P2)

- [ ] T018 [US3] Reconcile nano's security posture in `profiles/opcua-profile-manifest.yaml` (SecurityPolicy None only; message-crypto CUs `opc_cu_3080`/Sign-Encrypt SecurityPolicy CUs unclaimed for nano) and regenerate `docs/conformance/` so the advertised posture matches the gated build (FR-009).
- [ ] T019 [P] [US3] Update `docs/build-and-gating.md` gate table and `docs/conformance/completion.md` for the new symbol via regeneration; confirm drift-free.

## Phase 6: Gating, size ledger, final verification

- [ ] T020 Extend `scripts/test_profile_gating.sh` to assert `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` resolves **off for nano** and **on for micro/embedded/standard/full** (FR-010).
- [ ] T021 Record the nano `.text` recovery (~5.8–6.0 KB, no BSS change) in `docs/size/feature-size-ledger.md` (SC-001/SC-004).
- [ ] T022 Full verification (quickstart.md §5): `ctest` on pr-check + all five profiles, `pytest scripts/profile_manifest/ tests/conformance/`, `validate.py --all`, `test_profile_gating.sh`, `git diff --check`, format-check. All green.

## Dependencies

- Phase 2 (T003–T006) precedes Phase 3 (the gate symbol must exist before CMake/guards reference it).
- Within US1: RED (T007–T009) before GREEN (T010–T015); T010 (CMake) and T011 (relocation) before T012–T014 (guard swaps) can link.
- US2 (T016–T017) after US1 GREEN. US3 (T018–T019) after the gate exists; independent of US2. Phase 6 last.
