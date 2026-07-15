<!-- markdownlint-disable MD013 -->

# Contracts: Nano SecurityPolicy-None Crypto Gating

**Feature**: 072-nano-securitypolicy-none-gating | **Date**: 2026-07-15

Four build/behaviour contracts. Each maps to spec acceptance scenarios and success criteria and is enforced test-first.

## C1 — Nano excludes crypto (SC-001, US1-AS1)

**Given** a nano-profile build, **when** the library archive and config are inspected, **then** `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` is undefined, and `sym_chunk.c`, `asym_chunk/wrap.c`, `asym_chunk/unwrap.c`, `certificate.c`, `trustlist.c` produce no object files in the archive, and no linked symbol references them.
**Evidence**: nano build links clean with the gate off (no undefined `mu_*` crypto symbols); `nm`/archive inspection or a build-time assert; `scripts/test_profile_gating.sh` asserts the gate is off for nano.

## C2 — SecurityPolicy-None path intact (SC-002, US1-AS2)

**Given** a nano-profile server (crypto gate off), **when** a client performs Hello → OpenSecureChannel(None) → CreateSession → ActivateSession(Anonymous) → Read → Browse, **then** every step succeeds exactly as before this change.
**Evidence**: the existing None-path test set passes against a gate-off build: `minimal_server_flow`, `service_dispatch`, `session`, `session_auth`, `discovery_services`, `discovery_endpoint`, `browse_service`, `read_service`, `secure_channel`, `uasc_framing`, `server_handshake`.

## C3 — Non-None rejected explicitly (FR-004, US1-AS3)

**Given** a nano-profile server (crypto gate off), **when** a client requests any non-None SecurityPolicy in OpenSecureChannel, **then** the server returns `Bad_SecurityPolicyRejected` and establishes no partial/insecure channel or session.
**Evidence**: a gate-off unit test drives `mu_secure_channel_open` with a non-None policy URI and asserts `MU_STATUS_BAD_SECURITYPOLICYREJECTED` and no channel state mutation (regression guard on the crypto-independent rejection path).

## C4 — Secured profiles byte-identical (SC-003, US2)

**Given** any profile built with a crypto backend (`MUC_OPCUA_HAVE_MBEDTLS`/`MUC_OPCUA_HAVE_WOLFSSL`), **when** configured, **then** `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` is defined, all crypto TUs compile, the full crypto suite passes unchanged (`secure_handshake_modern`, `sym_chunk`, `asym_chunk`, `user_auth_encrypted`, `ecc_handshake_e2e`), **and** the profile's ARM `.text` is byte-identical to its pre-change baseline.
**Evidence**: per-profile CTest on micro/embedded/standard/full stays green; `scripts/measure_size.sh` (or the CI size job) shows zero `.text` delta for secured profiles; gating test asserts the gate on for secured profiles.

## Size contract (SC-001)

Nano ARM `.text` drops ~5.8–6.0 KB with no BSS change; the size ledger records the recovery. Any deviation beyond the validated band is a planning-assumption failure to investigate, not silently accept.
