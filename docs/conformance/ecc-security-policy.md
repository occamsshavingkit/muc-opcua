# Conformance: Security ECC Policy (spec 059)

This server implements the optional **Security ECC Policy** conformance unit, which the
OPC-10000-7 v1.05.02 profile database attaches to the 2025 device server profiles (Nano
2266, Micro 2267, Embedded 2268) as **OPTIONAL**:

> *Support one of SecurityPolicy [ECC-A] or SecurityPolicy [ECC-B] … It is recommended
> that both be supported.*

ECC is **optional at every server tier, including Embedded 2025** — it is not a Nano
mandate. RSA (Basic256Sha256) already satisfies the mandatory *Security Policy Required*
CU. Implementing ECC lets this project advertise the optional *Security ECC Policy* CU.
Verified against the live profile database (`profiles.opcfoundation.org/api`), not memory.

## Policies implemented

Both named policies the CU references:

| | ECC-A | ECC-B |
|---|---|---|
| URI | `…/SecurityPolicy#ECC_curve25519` | `…/SecurityPolicy#ECC_nistP256` |
| Ephemeral key agreement | X25519 ECDH (RFC 7748, 32 B LE pubkey) | P-256 ECDH (64 B BE x‖y) |
| Asymmetric signature | Ed25519 PureEdDSA (RFC 8032), 64 B | ECDSA-SHA2-256 (FIPS 186-4), 64 B r‖s |
| Symmetric protection | ChaCha20-Poly1305 AEAD (RFC 8439) | AES-128-CBC + HMAC-SHA-256 |
| KDF | HKDF-SHA-256 (RFC 5869) | HKDF-SHA-256 |
| SecureChannelNonce | 32 B (= X25519 pubkey) | 64 B (= P-256 pubkey) |

Handshake mechanics follow **OPC-10000-6 §6.8.1**: the Client/ServerNonce carry the
peers' ephemeral ECC public keys; the shared secret is ECDH; channel keys are HKDF-derived
with `ServerSalt = L | UTF8("opcua-server") | ServerNonce | ClientNonce` (and the mirror
`ClientSalt`), the salt doubling as HKDF info. The OPN chunk is **signed only** (ECC has no
asymmetric encryption); the ReceiverCertificateThumbprint is present (§6.7.2.3). The AEAD
per-chunk nonce XORs the derived IV with `TokenId ‖ LastSequenceNumber` (Table 69). The
ActivateSession application `ClientSignature.Algorithm` for ECC is empty or the
SecurityPolicy URI (the .NET/open62541 interop convention), **not** an xmldsig ecdsa URI.

## Crypto backend support

The SecureChannel/UASC protocol is backend-agnostic; the ECC primitives come from the
configured crypto adapter:

| Backend | ECC-A (curve25519) | ECC-B (nistP256) |
|---|---|---|
| OpenSSL (host) | ✅ | ✅ |
| wolfSSL | ✅ | ✅ |
| mbedTLS | ✖ (no Ed25519 in mbedTLS 2.28) | ✅ |

An **mbedTLS deployment offers ECC-B only** and still satisfies the CU (which requires
*one of* ECC-A/ECC-B). curve25519 fails closed on mbedTLS (`Bad_NotSupported`).

## Gating

`MUC_OPCUA_ECC` is a build option: **default ON for standard/full**, OFF for
nano/micro/embedded. It is legal (an optional CU) to enable at any tier. Enabling it costs
~3.1 KB `.text` and it lands only in ECC-ON builds; an ECC-OFF `--gc-sections` ELF carries
zero ECC crypto (see the [size ledger](../size/feature-size-ledger.md)).

## Verification (test-first, internal conformance evidence)

- **KAT primitives** — `tests/unit/test_ecc_crypto.c` (X25519/Ed25519/P-256 ECDH/ECDSA,
  HKDF RFC 5869, ChaCha20-Poly1305 RFC 8439) against known-answer vectors, plus
  `test_{mbedtls,wolfssl}_adapter.c` cross-validate each embedded backend against the
  OpenSSL host adapter (host-signs↔backend-verifies, cross-ECDH secrets equal).
- **End-to-end handshake** — `tests/integration/test_ecc_handshake_e2e.c` runs a full
  client↔server exchange for **both** policies: OpenSecureChannel → CreateSession →
  ActivateSession (with an ECC application ClientSignature) → Read, plus a tamper case per
  policy (a corrupted ECC signature blocks activation). The "client" is the library's own
  primitives (as in the RSA `test_secure_handshake_modern.c`).

Still profile-targeting pending external CTT — the internal tests are the conformance
evidence (see `docs/conformance/services.md`).
