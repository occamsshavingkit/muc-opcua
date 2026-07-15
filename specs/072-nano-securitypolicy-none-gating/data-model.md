<!-- markdownlint-disable MD013 -->

# Data Model: Nano SecurityPolicy-None Crypto Gating

**Feature**: 072-nano-securitypolicy-none-gating | **Date**: 2026-07-15

This feature introduces no runtime data structures. Its "model" is the build-configuration graph: one new gate symbol, the translation units it selects, and the call sites it guards.

## Entity: Secure-channel crypto gate

- **Symbol**: `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` (Kconfig bool; emitted as `-DMUC_OPCUA_SECURE_CHANNEL_CRYPTO=1` or absent).
- **Per-profile default** (FR-005):

  | Profile | Default | Reason |
  | --- | --- | --- |
  | nano | **off** | SecurityPolicy None only (OPC-10000-7 §4.3) |
  | micro | on | security-capable profile |
  | embedded | on | security-capable profile |
  | standard | on | security-capable profile |
  | full | on | security-capable profile |
  | custom | inherits explicit selection | user-configured |

- **Dependencies**: `MUC_OPCUA_CU_SECURITY_ECC` and the mbedTLS/wolfSSL/OpenSSL backend blocks depend on this gate (ECC/backends cannot exist without it). The gate is independent of `MUC_OPCUA_FACET_CORE_2022_SERVER` (a nano build has the facet on, the gate off).
- **Invariant**: gate off ⇒ none of the crypto TUs (below) are compiled and no linked symbol references them; gate on ⇒ the pre-change source set compiles byte-identically.

## Entity: Crypto translation-unit membership

Selected on the gate instead of the facet:

| TU | Role |
| --- | --- |
| `sym_chunk.c` | symmetric MessageChunk encrypt/sign |
| `asym_chunk/wrap.c` | asymmetric (OPN) chunk wrap |
| `asym_chunk/unwrap.c` | asymmetric (OPN) chunk unwrap |
| `certificate.c` | certificate parse/validate |
| `trustlist.c` | trust list |
| `key_derivation.c` | KDF + (currently) `mu_secure_zero`/`mu_secure_memeq` |

Always-compiled (NOT gated by this feature): `security_policy.c` (policy/mode enums, `mu_security_policy_from_uri`), and the relocated `mu_secure_zero`/`mu_secure_memeq` utilities.

## Entity: Guarded call sites (guard migrates facet → gate)

`data_chunk.c`, `process_message.c` (both `src/core/server/`), `osc_handler.c`, `create_session.c`, `activate_session.c` (`src/core/service_dispatch/`), `secure_channel.c` (`src/services/`). Each retains its existing SecurityPolicy-None fallback path. See research.md Decision 3 for the verified per-file guard sites.

## Non-entity: non-None rejection

`secure_channel.c` non-None rejection (`Bad_SecurityPolicyRejected`) is already crypto-independent (outside the facet guard) and is unchanged — asserted by a regression test, not modified.
