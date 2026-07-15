<!-- markdownlint-disable MD013 -->

# Research: Nano SecurityPolicy-None Crypto Gating

**Feature**: 072-nano-securitypolicy-none-gating | **Date**: 2026-07-15
**Status**: All planning unknowns resolved. Coupling map verified against the current `main` tree (not the removed prototype worktree).

## Decision 1 — Gate name and default policy (FR-005)

**Decision**: Add one Kconfig symbol `MUC_OPCUA_SECURE_CHANNEL_CRYPTO`. Per-profile default: **off for nano only**, **on for micro/embedded/standard/full**. Driven by profile intent, not backend presence.

**Rationale**: Nano's profile definition (OPC-10000-7 §4.3 / `profiles/opcua-profile-snapshot.json`) is SecurityPolicy None + Anonymous only, so its crypto is provably dead. micro/embedded/standard/full are security-capable profiles that claim SecurityPolicy CUs; keeping the gate on preserves their conformance surface even when a particular build omits a crypto backend (the crypto then compiles as harmless dead code rather than silently dropping claimed security). Conformance surface stays a function of the profile, not the toolchain.

**Alternatives rejected**: (a) *Backend-aware auto-off* — off whenever no crypto backend; rejected because it couples a build's advertised conformance to whether the integrator supplied mbedTLS/wolfSSL, which the spec explicitly forbids ("not solely by backend presence"). (b) *Runtime-only reliance on `crypto_adapter == NULL`* — leaves ~6 KB of dead flash linked on nano; violates Principle VII.

## Decision 2 — Crypto translation units to exclude when the gate is off

**Verified** at `src/CMakeLists.txt:450-457`: the crypto TUs are added by `file(GLOB_RECURSE _sources cu/core_2022_server/security/*.c)` under `if(MUC_OPCUA_FACET_CORE_2022_SERVER)`, with `ecc.c` removed from the list (it is added separately under `MUC_OPCUA_CU_SECURITY_ECC` at `:462-466`).

TUs to exclude when `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` is off:

- `src/cu/core_2022_server/security/sym_chunk.c`
- `src/cu/core_2022_server/security/asym_chunk/wrap.c`
- `src/cu/core_2022_server/security/asym_chunk/unwrap.c`
- `src/cu/core_2022_server/security/certificate.c`
- `src/cu/core_2022_server/security/trustlist.c`
- `src/cu/core_2022_server/security/key_derivation.c` — **except** the relocated `mu_secure_zero`/`mu_secure_memeq` utilities (Decision 4)

**Stays always-compiled**: `src/cu/.../security/security_policy.c` is NOT under the glob — it lives in the core source list (`src/CMakeLists.txt:39`) and provides the policy/mode enums and `mu_security_policy_from_uri`, which the None path and the non-None rejection both need. `ecc.c` remains under its own `MUC_OPCUA_CU_SECURITY_ECC` gate (which itself must depend on the crypto gate — see Decision 5).

**Decision**: Change the glob condition (and/or a nested list) so these TUs are selected on `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` rather than `MUC_OPCUA_FACET_CORE_2022_SERVER`. The facet's `MUC_OPCUA_SECURITY=1` definition and node-table content stay under the facet.

## Decision 3 — Crypto call-site guard migration

**Verified**: every crypto block in these six files is guarded by `#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER` today, each with a SecurityPolicy-None fallback. Migrate each to `MUC_OPCUA_SECURE_CHANNEL_CRYPTO`:

| File | Guard sites (verified) | None fallback |
| --- | --- | --- |
| `src/core/server/data_chunk.c` | `:6, :15, :63, :262…:466` | `handle_data_chunk_plaintext` (`:140`) is outside the guard; `:343` routes None→plaintext |
| `src/core/server/process_message.c` | `:98…:103` | `:104` unconditional `handle_data_chunk_plaintext` |
| `src/core/service_dispatch/osc_handler.c` | `:63…:83, :183…:188, :238…:294` (nested `MUC_OPCUA_CU_SECURITY_ECC`) | `:292 #else` no-facet path; runtime `policy!=NONE && adapter!=NULL` |
| `src/core/service_dispatch/create_session.c` | `:131…:150, :263…:295, :328…:333, :365…:367` | runtime `policy!=NONE && policy!=INVALID && adapter!=NULL` |
| `src/core/service_dispatch/activate_session.c` | `:8…:90, :154…:156, :161…:203, +many` (interleaved `MUC_OPCUA_CU_USER_AUTH`, ECC) | `:12` early-return for None; explicit `#else` at `:320/:387` |
| `src/services/secure_channel.c` | `:3…:5, :57…:61, :92…:116` (nested ECC `:103`), `:163…:169` | None path `:84-91` is outside the guard (see Decision 6) |

**Note on `activate_session.c`**: guards are interleaved with `MUC_OPCUA_CU_USER_AUTH` and `MUC_OPCUA_CU_SECURITY_ECC`. Only the blocks that touch **message/asym crypto, certificate, or key material** move to the crypto gate; blocks that are purely user-token plumbing stay on their existing CU gate. Each block is classified during implementation and cited to its OPC-10000-4 §5.6/§5.7 line.

## Decision 4 — `mu_secure_zero` / `mu_secure_memeq` relocation

**Verified**: declared unconditionally in `src/security/key_derivation.h:11-12`; defined in `src/cu/core_2022_server/security/key_derivation.c:16` (a crypto-glob TU that will be excluded). Every current caller is behind `MUC_OPCUA_FACET_CORE_2022_SERVER`, so **no no-crypto caller exists today** — relocation is not strictly forced, but the header exposes the symbol unconditionally and the defining TU disappears when the gate is off.

**Decision**: Relocate `mu_secure_zero` (and `mu_secure_memeq`) into an always-compiled utility TU (e.g. a small `src/security/secure_util.c` or an existing always-compiled core unit), keeping the `key_derivation.h` declaration or moving it to a util header. Rationale: robustness against future/overlooked no-crypto callers and clean layering (a defensive-zeroing primitive is not inherently crypto). This also recovers a few hundred extra bytes on nano (the utility no longer drags `key_derivation.c`'s KDF code — though the KDF is excluded anyway; the win is keeping the util without the TU). Verify with a nano gate-off link.

**Alternative rejected**: leave the definition in `key_derivation.c` and only compile that one function when the gate is off — fragile (partial-TU compilation via `#ifdef` around everything else) and keeps a crypto-named TU in a no-crypto build.

## Decision 5 — ECC sub-gate dependency

**Verified**: `ecc.c` and its call-sites are under `MUC_OPCUA_CU_SECURITY_ECC`, added separately at `src/CMakeLists.txt:462-466`. ECC is asymmetric message crypto and cannot function without the crypto stack.

**Decision**: `MUC_OPCUA_CU_SECURITY_ECC` (and the OpenSSL/mbedTLS/wolfSSL backend selection blocks at `:475-513`) MUST depend on `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` in Kconfig/CMake, so ECC cannot be enabled when the crypto gate is off. Nano has ECC off already; this makes the dependency explicit.

## Decision 6 — Non-None rejection is crypto-independent (no change)

**Verified**: `src/services/secure_channel.c::mu_secure_channel_open` rejects non-None with `MU_STATUS_BAD_SECURITYPOLICYREJECTED` at `:77-78` (invalid URI), `:84-87` (server None-only, client non-None), and `:117-119` (catch-all). These are **outside** the `#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER` block (`:92-116`, the Basic256/AES/ECC classification arms). URI classification uses `mu_security_policy_from_uri` from the always-compiled `security_policy.c`.

**Decision**: No change required for FR-004 — the explicit non-None rejection already survives with the crypto gate off. A gate-off test MUST assert it still returns `Bad_SecurityPolicyRejected` (regression guard).

## Decision 7 — Manifest / conformance reconciliation (FR-009)

**Decision**: Represent the new gate in `profiles/opcua-profile-manifest.yaml` with its per-profile defaults, and ensure nano's security posture (SecurityPolicy None only; message-crypto CUs unclaimed) is explicit and consistent with the gated build. Regenerate Kconfig/defconfigs/docs so the advertised posture matches. The gating test (`scripts/test_profile_gating.sh`) asserts the gate resolves off for nano and on for secured profiles (FR-010).

## Open questions

None. FR-005 resolved (Decision 1). Coupling map verified. Ready for Phase 1 design and tasks.
