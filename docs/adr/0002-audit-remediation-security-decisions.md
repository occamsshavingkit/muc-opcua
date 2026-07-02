# ADR 0002: Audit-remediation security & configuration decisions

**Status:** Accepted (feature 025-audit-remediation)
**Date:** 2026-07-02

## Context

The five-lens audit surfaced twelve findings; the load-bearing ones concern
secured-channel authentication, RP2040 readiness, and profile configuration. This
record captures the non-obvious design decisions made while remediating them, so
future maintainers understand *why* the code looks as it does.

## Decisions

### 1. ActivateSession ClientSignature is verified against a persisted per-channel certificate

OPC-10000-4 §5.7.3/§7.38.2 requires the server to verify the client's
`ClientSignature` over `(serverCertificate ‖ serverNonce)`. The OPN sender
certificate is cleared immediately after OpenSecureChannel (it pointed into
transient receive-buffer memory), so it is unavailable at ActivateSession time.
We therefore persist one copy of the client application-instance certificate for
the channel lifetime in `server->channel_client_cert` (sized
`MU_MAX_CLIENT_CERT_SIZE`, **caller-provided storage**, only under
`MUC_OPCUA_SECURITY`). CreateSession binds its ClientCertificate to equal this
certificate, which is then the trusted verification key at ActivateSession.
Rationale: one per-channel buffer (not one per session) keeps RAM flat on the
session pool, and the `.bss` size gate is untouched because the buffer lives in
the caller storage arena.

### 2. Secured policies fail closed on trust and certificate validity

Trust-list matching is now mandatory for any non-None policy: a missing trust list
rejects the connection rather than accepting it. Certificate validity
(`notBefore`/`notAfter`) is enforced through a new optional crypto-adapter hook
`verify_certificate_validity`; when the hook is absent the secured path fails
closed (`Bad_CertificateInvalid`) rather than skipping the check. Rationale:
Constitution §V — security must never be silently weakened. The trust check is
placed *after* `mu_secure_channel_open` so an unsupported policy / missing crypto
adapter is still reported as `Bad_SecurityPolicyRejected`, not masked by a trust
failure.

### 3. Signature scheme follows the server's own ServerSignature (PKCS#1.5)

ClientSignature verification uses `rsa_sha256_verify` (PKCS#1.5), matching the
ServerSignature the server itself emits, which is PKCS#1.5 regardless of policy.
Making both policy-correct RSA-PSS for `Aes256_Sha256_RsaPss` is a separate,
symmetric change and is out of scope here.

### 4. Address-space over-cap is a loud failure, not a silent fallback

A user address space larger than `MU_MAX_ADDRESS_SPACE_NODES` is rejected at
`mu_server_init` instead of silently degrading to an O(n) linear scan (which also
made Query return nothing). Constitution §VII: predictable failure over ambiguous
degradation. Integrators needing a larger space raise the cap macro, which resizes
the index storage.

### 5. RP2040 entropy/time are real; TCP is explicitly deferred

`pico_generate_random` now draws from the ROSC hardware TRNG (was an all-zero
stub); time comes from `time_us_64()`/`to_ms_since_boot()` (monotonic, boot-
relative — no RTC). The TCP adapter remains a skeleton; the example documents that
a real transport (lwIP) is required, that bring-up should use SecurityPolicy None,
and that secured builds need `PICO_STACK_SIZE ≥ 16 KB`.

### 6. `features.h` has dependency guards but no feature defaults

Every `MUC_OPCUA_*` gate is `=1` when on and undefined when off, and both
`#ifdef X` and `#if X` already treat undefined as off. Adding `#define X 0`
defaults would wrongly make `#ifdef X` true and enable the feature. So
`features.h` contains only `#error` guards for illegal combinations, not defaults.

## Consequences

- Secured Embedded/full profiles now enforce OPC-10000-4 application
  authentication; a client can no longer activate a session it cannot prove key
  possession for, nor connect with an untrusted/expired certificate.
- Per-profile `.text` grew ≤ ~0.9%; `data + bss` unchanged (0); caller storage
  grows by `MU_MAX_CLIENT_CERT_SIZE` under security. Within all gates.
- The RP2040 example is honestly documented as needing a transport before it can
  serve clients.
