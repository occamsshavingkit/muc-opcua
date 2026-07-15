<!-- markdownlint-disable MD013 -->

# Feature Specification: Nano SecurityPolicy-None Crypto Gating

**Feature Branch**: `072-nano-securitypolicy-none-gating`  
**Created**: 2026-07-15  
**Status**: Draft  
**Input**: Size audit of the nano profile (2026-07-15) finding ~6 KB of secure-channel crypto compiled into a profile that only requires SecurityPolicy None. Prototype validated in an isolated worktree.

## Context and Grounding

The `NanoEmbeddedDevice` Server Profile is defined (per `profiles/opcua-profile-snapshot.json`) as: *"Requires OPC UA TCP binary transport, SecurityPolicy None, and Anonymous identity. Single client/SecureChannel/Session baseline."* SecurityPolicy None uses plaintext UA Secure Conversation framing with no message encryption, signing, key derivation, or certificate validation (OPC-10000-6 §6.7.2, §7.2).

Today, `src/CMakeLists.txt` compiles the entire `cu/core_2022_server/security/*.c` set behind a single `MUC_OPCUA_FACET_CORE_2022_SERVER` glob. That facet macro does double duty: it is both the umbrella that (via the Kconfig cascade) enables nano's mandatory services (Read, View, Discovery, Session) **and** the compile-time guard for the symmetric/asymmetric chunk crypto, key derivation, certificate, and trustlist code. As a result a nano build links the full crypto stack even though:

- The Nano profile requires only SecurityPolicy None (grounded above).
- The project's own manifest already marks `opc_cu_2600` SecurityPolicy Support **unimplemented** and the Core 2022 Server Facet **deferred** for nano — the crypto was never intended to ship in nano.
- Nano builds with no crypto backend (`MUC_OPCUA_HAVE_MBEDTLS=OFF`, `MUC_OPCUA_HAVE_WOLFSSL=OFF`), so `config.crypto_adapter` is always `NULL` and every crypto branch is already dead at runtime — it is merely still linked.

### Validated prototype evidence (2026-07-15)

A prototype in an isolated worktree introduced a `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` gate separating message crypto from the facet umbrella, excluded the heavy crypto translation units when off, and switched the crypto call-site guards (`data_chunk.c`, `process_message.c`, `osc_handler.c`, `create_session.c`, `activate_session.c`, `secure_channel.c`) from the facet macro to the new gate. Measured on ARM Cortex-M0+ (`-Os`, LTO, `--gc-sections`):

| nano build | archive `.text` | linked ELF `.text` |
| --- | --- | --- |
| baseline | 29,442 | 27,220 |
| crypto gated out | 23,660 | 21,236 |
| **recovered** | **−5,782** | **−5,984** |

Correctness was confirmed both directions: with crypto gated out, 11 SecurityPolicy-None test executables passed (`minimal_server_flow`, `service_dispatch`, `session`, `session_auth`, `discovery_services`, `discovery_endpoint`, `browse_service`, `read_service`, `secure_channel`, `uasc_framing`, `server_handshake`); with a mbedTLS backend present the gate resolved on and the crypto tests still passed (`secure_handshake_modern`, `sym_chunk`, `asym_chunk`, `user_auth_encrypted`). The recovery figure is conservative — the prototype kept `key_derivation.c` compiled to satisfy `mu_secure_zero`; relocating that utility recovers a few hundred additional bytes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Nano ships only what SecurityPolicy None needs (Priority: P1)

As an embedded integrator selecting the nano server profile, I need the build to exclude secure-channel message crypto that the Nano profile does not require, so that flash footprint reflects the profile's actual conformance surface instead of dead code.

**Why this priority**: This is the entire point of the feature — recover ~6 KB of flash from a profile whose defining constraint is SecurityPolicy None.

**Independent Test**: Build the nano profile and confirm the symmetric/asymmetric chunk crypto, certificate, and trustlist translation units are absent from the archive, that the linked size drops by the measured delta, and that a SecurityPolicy-None client can still connect, open a secure channel, create and activate an anonymous session, read, and browse.

**Acceptance Scenarios**:

1. **Given** a nano-profile build, **When** the library archive is inspected, **Then** `sym_chunk.c`, `certificate.c`, `trustlist.c`, and the asymmetric chunk wrap/unwrap objects are not present, and `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` is undefined.
2. **Given** a nano-profile server, **When** a client connects with SecurityPolicy None and Anonymous identity and issues Hello/OpenSecureChannel/CreateSession/ActivateSession/Read/Browse, **Then** every step succeeds exactly as before this change.
3. **Given** a nano-profile server, **When** a client attempts to open a secure channel with a non-None SecurityPolicy, **Then** the server rejects it with an explicit OPC UA StatusCode and no partial/insecure session is established.
4. **Given** the nano profile, **When** its size is measured with `scripts/measure_size.sh nano`, **Then** the recorded `.text` drops by approximately the validated delta and BSS is unchanged.

---

### User Story 2 - Secured profiles are byte-for-byte unaffected (Priority: P1)

As an integrator selecting micro or higher with a crypto backend, I need secure-channel message crypto to remain fully present and unchanged, so that gating nano does not regress any secured build.

**Why this priority**: The gate must be a pure subtraction for nano only; any behavioral or size change to secured profiles would be a regression.

**Independent Test**: Build micro/embedded/standard/full with a crypto backend, confirm `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` is defined, and run the full crypto handshake/chunk/user-auth test suite.

**Acceptance Scenarios**:

1. **Given** any profile built with `MUC_OPCUA_HAVE_MBEDTLS` or `MUC_OPCUA_HAVE_WOLFSSL`, **When** the build is configured, **Then** `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` is defined and all crypto translation units are compiled.
2. **Given** a secured build, **When** the secure handshake, symmetric chunk, asymmetric chunk, and encrypted user-auth tests run, **Then** they all pass unchanged.
3. **Given** a secured build, **When** its ARM `.text` is measured, **Then** it is byte-identical to the pre-change baseline for that profile.

---

### User Story 3 - The conformance model tells the truth about nano security (Priority: P2)

As a maintainer auditing profile claims, I need the manifest and generated conformance docs to state clearly that nano supports SecurityPolicy None only and does not claim the message-crypto CUs, so that the build gating and the published conformance surface agree.

**Why this priority**: A silent size win that leaves the manifest implying nano ships crypto would reintroduce the same dishonesty this project's gating work exists to prevent.

**Independent Test**: Inspect the manifest and generated docs and confirm nano's security posture (SecurityPolicy None, no message-crypto CUs) is explicit and consistent with the build.

**Acceptance Scenarios**:

1. **Given** the manifest, **When** nano's security-related CU entries are read, **Then** SecurityPolicy None is represented as nano's supported policy and the message-crypto CUs (`opc_cu_3080` and any Sign/Encrypt policy CUs) are not claimed for nano.
2. **Given** regenerated conformance docs, **When** nano is described, **Then** its security capability matches the gated build.

## Requirements *(mandatory)*

- **FR-001**: The system MUST provide a dedicated build symbol (working name `MUC_OPCUA_SECURE_CHANNEL_CRYPTO`) that gates secure-channel message crypto independently of the `MUC_OPCUA_FACET_CORE_2022_SERVER` service umbrella.
- **FR-002**: When the crypto gate is off, the build MUST exclude the symmetric chunk, asymmetric chunk (wrap/unwrap), certificate, and trustlist translation units from compilation, and MUST NOT reference their symbols from any linked call site.
- **FR-003**: When the crypto gate is off, the server MUST retain full SecurityPolicy-None behavior: plaintext UA Secure Conversation framing, OpenSecureChannel with policy None, CreateSession, ActivateSession with Anonymous (and, where the profile enables it, UserName) identity, and all service dispatch.
- **FR-004**: When a client requests any non-None SecurityPolicy against a crypto-gated build, the server MUST reject it with an explicit OPC UA StatusCode and MUST NOT establish a partial or insecure channel/session.
- **FR-005**: The crypto gate MUST be driven by a Kconfig symbol whose per-profile default encodes profile intent (not toolchain availability): it defaults **off for nano only** (the sole SecurityPolicy-None-only profile) and **on for micro/embedded/standard/full**. **Resolved (planning, 2026-07-15): profile intent, not backend presence.** A security-capable profile (micro and above) keeps the gate on even when built without a crypto backend — its crypto compiles as dead code rather than silently dropping its claimed security surface — so the conformance surface stays tied to the profile, not the build's toolchain. Only nano, whose profile definition is SecurityPolicy None only, gates crypto out by default.
- **FR-006**: Secured profiles (crypto gate on) MUST be byte-identical in ARM `.text` to the pre-change baseline for each profile, and MUST pass the full existing crypto test suite unchanged.
- **FR-007**: Crypto-dependent test targets MUST be gated so they are not built when the crypto gate is off, and SecurityPolicy-None test coverage MUST be added/confirmed for a no-crypto nano build (server flow, OPN None, session, discovery, read, browse).
- **FR-008**: Small always-needed utilities currently housed in a crypto translation unit (notably `mu_secure_zero`) MUST remain available to non-crypto builds, either relocated to an always-compiled utility unit or otherwise provided, without pulling the heavy crypto back in.
- **FR-009**: The manifest and generated conformance artifacts MUST be updated so nano's security posture (SecurityPolicy None only; message-crypto CUs unclaimed for nano) is explicit and consistent with the gated build.
- **FR-010**: Profile-gating tests (`scripts/test_profile_gating.sh`) MUST assert the crypto gate resolves off for nano and on for secured profiles, and the size ledger MUST record the nano recovery.

### Key Entities

- **Secure-channel crypto gate** — the new build symbol separating message crypto from the service facet.
- **Crypto translation units** — `sym_chunk.c`, `asym_chunk/wrap.c`, `asym_chunk/unwrap.c`, `certificate.c`, `trustlist.c` (and the utility split-out from `key_derivation.c`).
- **Crypto call sites** — the guarded blocks in `data_chunk.c`, `process_message.c`, `osc_handler.c`, `create_session.c`, `activate_session.c`, `secure_channel.c`, each of which already has a SecurityPolicy-None fallback.

## Success Criteria *(mandatory)*

- **SC-001**: Nano ARM `.text` drops by approximately the validated delta (~5.8–6.0 KB) with no BSS change.
- **SC-002**: A SecurityPolicy-None client completes connect → OPN(None) → CreateSession → ActivateSession(Anonymous) → Read → Browse against a crypto-gated nano build.
- **SC-003**: Every secured profile is byte-identical in ARM `.text` to its pre-change baseline and passes the full crypto test suite.
- **SC-004**: `scripts/test_profile_gating.sh` and `scripts/profile_manifest/validate.py --all` pass, and the size ledger records the nano recovery.

## Scope

- **In scope**: Separating message crypto from the facet umbrella; excluding crypto TUs for no-crypto/nano builds; preserving SecurityPolicy-None behavior; keeping secured builds unchanged; reconciling nano's security posture in the manifest/docs; size + gating evidence.
- **Out of scope**: Adding any new SecurityPolicy or crypto algorithm; changing secured-profile behavior; the CU-based conformance completion reporting layer (tracked separately in spec 073).

## Dependencies and Assumptions

- Assumes the Nano profile's SecurityPolicy-None-only definition is authoritative (grounded in `profiles/opcua-profile-snapshot.json`; to be re-confirmed against OPC-10000-7 §4.3 during planning).
- Assumes secured profiles always build with a crypto backend in CI; the gate's default policy for a backend-less micro/embedded build is a planning clarification (FR-005).
- Independent of spec 073, but spec 073's completion report will make this change's manifest reconciliation visible and auditable.
