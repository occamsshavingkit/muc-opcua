# Implementation Plan: SecurityPolicy-aware RSA-PSS Signatures

**Branch**: `026-rsa-pss-signatures` | **Date**: 2026-07-02 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/026-rsa-pss-signatures/spec.md`

## Summary

Make the asymmetric signature scheme and its advertised `SignatureData.algorithm`
URI follow the negotiated SecurityPolicy, so `Aes256_Sha256_RsaPss` uses
RSA-PSS-SHA256 (its conformant scheme) while `Basic256Sha256` and
`Aes128_Sha256_RsaOaep` keep RSA-PKCS#1.5-SHA256 exactly as today. Centralize the
policy→{sign, verify, algorithm-URI} decision in one place and route all three
asymmetric-signature sites through it, and validate the incoming ClientSignature's
declared algorithm URI against the policy.

**Technical approach**: Add a single policy-keyed signature selector rather than a
ternary at each call site (per FR-003):
- `security_policy.c`: a policy→signature-algorithm-URI accessor and a
  "uses PSS?" predicate (pure policy metadata; extends the existing policy table).
- `certificate.c`: two thin wrappers `mu_asym_signature_sign(...)` and
  `mu_asym_signature_verify(...)` that take the crypto adapter + policy, dispatch
  to `rsa_sha256_*` or `rsa_pss_sha256_*`, and **fail closed** if the required
  primitive is NULL (FR-004). This is the single source of truth for scheme
  selection and is where the crypto adapter already lives.
- `service_dispatch.c`: replace the three direct `rsa_sha256_*` calls + hardcoded
  `SIG_URI` (ServerSignature emit ~L851, ClientSignature verify ~L402, x509
  user-token verify ~L1171) with the wrappers + the URI accessor, and add
  ClientSignature `algorithm`-URI validation against the policy (FR-005).
The PSS primitives are already implemented in all three backends (feature 013), so
no new crypto is written.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged)
**Primary Dependencies**: None new. Existing crypto adapters (host/mbedtls/wolfssl) already expose `rsa_pss_sha256_sign`/`rsa_pss_sha256_verify`
**Storage**: N/A
**Testing**: Existing `ctest` host suite + ASAN; the `Aes256_Sha256_RsaPss` handshake test is switched to PSS signing; a new algorithm-mismatch rejection test is added
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Project Type**: Portable C library with thin platform adapters (unchanged)
**Performance Goals**: Negligible; a small selection branch on the handshake path (not per-message)
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap; byte-identical handshake for None/Basic256Sha256/Aes128
**Scale/Scope**: 3 call sites in `src/core/service_dispatch.c`, 2 small helpers, 1 URI accessor, ~2 tests
**OPC UA Normative References**: OPC-10000-7 SecurityPolicy `Aes256_Sha256_RsaPss` (RSA-PSS-SHA256 signatures, URI `http://opcfoundation.org/UA/security/rsa-pss-sha2-256`); OPC-10000-4 §5.6.3/§5.7.3/§7.38.2; OPC-10000-6 §6.7
**Target OPC UA Profile/Conformance Units**: Unchanged (Nano/Micro/Embedded 2017 + full)
**Conformance Status Target**: profile-targeting (this makes the PSS policy's signature algorithm conformant; a prerequisite toward CTT)

## Embedded Size Budget

**Flash Impact**: Small positive (a policy→URI accessor + two dispatch wrappers + the PSS branch). The `rsa_pss_sha256_*` code is already linked in secured builds (used by the OPN asym chunk for the PSS policy), so no new crypto object code. Estimate < +0.5% `.text` on embedded/full; 0 on nano/micro (no security).
**RAM Impact**: 0. No new buffers; a PSS signature is the same length as PKCS#1.5 for the same key, so the existing `sig[512]`/`verify_buf[1536]` locals are unchanged.
**Heap Use**: None (unchanged).
**Static Tables Added**: One extra `const char *` URI column (or accessor) on the existing SecurityPolicy table — a few bytes rodata.
**Transport Buffers**: Unchanged.
**Crypto Dependency Impact**: None new — routes to existing adapter hooks.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS — makes the advertised `Aes256_Sha256_RsaPss` policy behave per OPC-10000-7; every changed byte cites its section. No StatusCode invented (reuses `Bad_SecurityChecksFailed`).
- **Embedded C Core**: PASS — freestanding C11; no platform leakage; routes to existing adapter vtable.
- **Memory Model**: PASS — no new heap, no new buffers, no struct growth.
- **Minimal OPC UA Surface**: PASS — no new service/policy/transport; corrects an existing policy's signature algorithm.
- **Profile Research**: PASS — profiles unchanged.
- **Correctness Gates**: PASS — test-first: switch the PSS handshake test to PSS signing (RED against the current PKCS#1.5 server), add an algorithm-mismatch rejection test; the PKCS#1.5 policies are guarded by byte-identical regression.
- **Security Honesty**: PASS — fail-closed when the PSS primitive is absent; no silent downgrade to PKCS#1.5.
- **Toolchain Discipline**: PASS — CMake, ctest, ASAN, four ARM profiles all exercised.
- **Size Discipline**: PASS — impact estimated above and re-measured in tasks.

**Result: PASS (no violations). Complexity Tracking not required.**

## Project Structure

### Documentation (this feature)

```text
specs/026-rsa-pss-signatures/
├── plan.md              # This file
├── research.md          # Phase 0 — the signature-scheme/URI decisions
├── data-model.md        # Phase 1 — the selector entity
├── quickstart.md        # Phase 1 — build/test/verify recipe
├── contracts/
│   └── asym-signature-selection.md   # policy→{sign,verify,URI} contract
├── checklists/requirements.md        # from /speckit-specify (all pass)
└── tasks.md             # Phase 2 — /speckit-tasks (NOT created here)
```

### Source Code (repository root)

```text
src/security/
├── security_policy.c/.h # add mu_security_policy_asym_signature_uri(policy) +
│                        #   mu_security_policy_uses_pss(policy) (policy metadata)
├── certificate.c/.h     # add mu_asym_signature_sign()/mu_asym_signature_verify()
│                        #   wrappers: dispatch by policy, fail-closed when primitive NULL
src/core/
├── service_dispatch.c   # route the 3 signature sites through the wrappers + URI
│                        #   accessor; validate ClientSignature.algorithm vs policy
tests/integration/
├── test_secure_handshake_modern.c   # PSS-sign the Aes256_Sha256_RsaPss client path;
│                                     #   add an algorithm-mismatch rejection case
```

**Structure Decision**: Existing layout kept. The selector lives in
`security_policy.c` (policy metadata) + `certificate.c` (adapter-aware wrappers) —
both already the homes for policy/crypto-adapter logic. No new translation unit is
required, though an `asym_signature.c` is an acceptable alternative if the wrappers
grow; the plan keeps them in `certificate.c` to minimize new files.

## Complexity Tracking

> No Constitution Check violations — intentionally empty.
