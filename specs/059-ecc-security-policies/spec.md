# Spec 059: ECC SecurityPolicies (curve25519 + nistP256)

**Status**: Planned | **Branch**: `059-ecc-security-policies`
**Roadmap**: Project A / A4 (final Project-A item) | **Implements**: spec 051 §4 (PG13/PG14)
**Plan**: `~/.claude/plans/robust-inventing-harbor.md`
**OPC UA**: OPC-10000-6 §6.8 (ECC handshake, key derivation), OPC-10000-4 §7.40.2.5
(EccEncryptedSecret), SecurityPolicy facets **[ECC-A] ECC-curve25519** and
**[ECC-B] ECC-nistP256**

## Profile-mandate correction (grounded against the live OPC profile DB)

The roadmap claimed "ECC-curve25519 is a hard v1.05.02 Nano mandatory." **That is wrong.**
Verified via `profiles.opcfoundation.org/api` (see [[opc-profile-reporting-api]]):

| Profile (2025) | id | ECC status |
|---|---|---|
| Nano Embedded Device | 2266 | **"Security ECC Policy" = OPTIONAL** (support ECC-A and/or ECC-B); defined here, inherited upward |
| Micro Embedded Device | 2267 | inherits Nano's optional ECC |
| Embedded UA Server | 2268 | adds "Security Policy Required" [MAND] — any policy; **RSA satisfies it**; ECC still optional |
| Standard UA Server | 2269 | same; ECC optional |

**ECC is OPTIONAL at every server tier, including Embedded 2025.** Implementing it lets us
advertise the optional *Security ECC Policy* CU. Gating is a product choice, not a mandate.

## Scope (locked with user)

Full ECC secure channel — ephemeral-ECDH handshake, HKDF key derivation, EccEncryptedSecret
user tokens — for **both server and client**, implementing **both** standard-named policies
**together**. No hand-rolled crypto (bind mbedTLS / wolfSSL / OpenSSL). Integration is
**Approach C**: an `asym_family` flag in the policy table plus an isolated
`src/security/asym_ecc.c` compiled only under a new `MUC_OPCUA_ECC` option; the RSA path is
untouched.

## Grounded algorithm suites

> Source: OPC SecurityPolicy facets 1374 (`[ECC-A] ECC-curve25519`) and 1435
> (`[ECC-B] ECC-nistP256`), each CU quoted from the profile DB. **The two policies do NOT
> share a symmetric layer** — nistP256 reuses the existing AES-CBC+HMAC path; curve25519
> needs a new ChaCha20-Poly1305 AEAD path.

| Property | ECC-A `#ECC_curve25519` | ECC-B `#ECC_nistP256` |
|---|---|---|
| Asym signature | **Ed25519** PureEdDSA (RFC 8032), 64B sig | **ECDSA-SHA2-256** (FIPS 186-4) |
| Key agreement | **X25519** ECDHE (RFC 7748), 32B keys, **little-endian** | **P-256** ECDHE (RFC 8422), **big-endian** x‖y, uncompressed |
| Key derivation | **HKDF-SHA2-256** (RFC 5869) | **HKDF-SHA2-256** (RFC 5869) |
| Sym encryption | **ChaCha20-Poly1305** AEAD (RFC 8439) | **AES128-CBC** |
| Sym signature | **Poly1305** (AEAD tag; no separate MAC key) | **HMAC-SHA2-256** |
| DerivedSignatureKeyLength | **0** (AEAD) | 256 bits |
| EncryptionKeyLength | 256 bits | 128 bits |
| InitializationVectorLength | 96 bits | 128 bits |
| SecureChannelNonceLength | **32 bytes** | **64 bytes** |
| MinAsymKeyLength | 256 (ECC) | 256 (ECC); Max 384 (CA only) |

Policy URIs: `http://opcfoundation.org/UA/SecurityPolicy#ECC_curve25519` and
`#ECC_nistP256`.

## Grounded handshake mechanics (OPC-10000-6 §6.8.1)

ClientNonce/ServerNonce carry the **ephemeral ECC public keys**. Shared secret via **ECDH**
(X25519 / P-256). Keys via **HKDF (RFC 5869)**:
`ServerSalt = L | UTF8("opcua-server") | ServerNonce | ClientNonce`, mirror `ClientSalt`;
IKM = the ECDH shared secret (x-coordinate big-endian for NIST; RFC 7748 for X25519); `info`
= the corresponding salt. NIST pubkeys encode x‖y zero-padded big-endian; Edwards per RFC 7748.

## Grounded EccEncryptedSecret layout (OPC-10000-4 §7.40.2.5, Table 186)

`TypeId, EncodingMask, Length, SecurityPolicyUri, Certificate(DER signing cert), SigningTime,
KeyDataLength(UInt16), KeyData(unencrypted), SenderPublicKey(ephemeral), ReceiverPublicKey
(ephemeral), Nonce, Secret, PayloadPadding, PayloadPaddingSize, Signature (over the structure
using the AsymmetricSignatureAlgorithm)`. Replaces the RSA-OAEP password/secret path in
ActivateSession.

## Requirements

- **FR-001 (crypto vtable + 3 backends)**: extend `mu_crypto_adapter_t`
  (`include/muc_opcua/platform.h`) with optional (NULL-able) callbacks:
  `generate_ephemeral_keypair` (per curve), `ecdh_derive`, `ecdsa_sha256_sign/verify`,
  `ed25519_sign/verify`, `hkdf_sha256`, and ChaCha20-Poly1305 `aead_encrypt/decrypt`.
  Implement in OpenSSL (`host_crypto/ecc.c`), mbedTLS, wolfSSL; RSA-only backends leave them
  NULL. **No hand-rolled crypto** (review gate).
- **FR-002 (KATs — non-circular conformance evidence)**: known-answer-vector unit tests for
  X25519 ECDH, Ed25519, P-256 ECDH, ECDSA-P256, HKDF-SHA256 (RFC 5869), ChaCha20-Poly1305
  (RFC 8439). Real crypto, no mocks.
- **FR-003 (HKDF)**: `mu_hkdf_sha256` in `key_derivation.c` (sibling to `mu_p_sha256`);
  `mu_sym_keys_derive` (`sym_chunk.c:32`) branches on `asym_family`. nistP256 keeps the
  sig/enc/iv split; curve25519 derives enc-key + IV only (no MAC key).
- **FR-004 (AEAD sym path)**: new ChaCha20-Poly1305 branch in the sym chunk codec (combined
  encrypt+authenticate; 96-bit IV; 16-byte tag; no separate HMAC). AES-CBC+HMAC path unchanged
  and reused by nistP256.
- **FR-005 (policy table)**: enum `..._ECC_CURVE25519_ID` / `..._ECC_NISTP256_ID`, URIs, and
  `POLICY_TABLE` rows (`security_policy.c`) wrapped in `#ifdef MUC_OPCUA_ECC`; add `asym_family`
  (RSA/ECC) and `sym_mode` (CBC-HMAC/AEAD) with helpers; per-policy nonce lengths (32B/64B).
- **FR-006 (server OPN)**: `osc_handler.c` — ECC ephemeral keygen for the server nonce,
  accept the client ephemeral pubkey nonce, ECDH+HKDF key derivation; asym-chunk
  (`asym_chunk/{wrap,unwrap}.c`) ECDSA/Ed25519 sign+verify and **skip** RSA-OAEP block
  encryption.
- **FR-007 (client OPN)**: build the real client OpenSecureChannel in
  `client_secure_channel.c` (currently a stub) with ECC ephemeral keygen + ECDH+HKDF.
- **FR-008 (user tokens)**: EccEncryptedSecret in ActivateSession username + x509 paths
  (`activate_session.c`), including ECDSA/Ed25519 verify.
- **FR-009 (gating)**: `MUC_OPCUA_ECC` option, in `MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS`,
  FORCE-ON for `standard`/`full`; droppable-TU wiring in `src/CMakeLists.txt`; `--gc-sections`
  keeps `asym_ecc.c` out of an ECC-OFF ELF.
- **FR-010 (e2e + size + docs)**: self-contained client↔server ECC handshake tests for both
  policies (open channel → create+activate session incl. EccEncryptedSecret username token →
  read → close); `scripts/measure_size.sh` deltas recorded in the size ledger; conformance
  doc claims the optional *Security ECC Policy* CU (ECC-A + ECC-B); per-profile ctest matrix
  green; correct the roadmap doc's erroneous "Nano-mandatory" claim.

## AEAD MSG-chunk framing (GROUNDED — OPC-10000-6 §6.7.2.1/§6.7.2.5.2/§6.8.1)

curve25519 MSG/CLO chunks use ChaCha20-Poly1305 (no CBC+HMAC):
- **Per-chunk IV (Table 69, §6.8.1):** derived 12-byte Client/Server InitializationVector with its
  **first 8 bytes XORed** with `TokenId(4) ‖ LastSequenceNumber(4)` (UInt32 LE §5.2.2.2;
  LastSequenceNumber = the previous chunk's SequenceNumber in that direction, 0 for the first).
- **Footer (Table 62, §6.7.2.5.2):** a single `Signature` = the 16-byte AEAD tag; **no
  Padding/PaddingSize**.
- **Structure (§6.7.2.1):** SequenceHeader+body are encrypted; the tag is computed during
  encryption and appended after the ciphertext; the **headers are the AAD** ("the signature
  includes the headers and all Message data").
- Layout: `[Header 16 | SecurityHeader/TokenId] [enc: SeqHeader(8)+body] [tag 16]`. Implemented as
  separate `mu_sym_chunk_wrap_aead`/`unwrap_aead` so the CBC+HMAC path is untouched.

## Open items to confirm during implementation

- Ephemeral-key delivery in CreateSession/ActivateSession responses for the user-token
  EccEncryptedSecret (OPC-10000-6 §6.8.2), incl. `Bad_SecurityPolicyRejected` when the client's
  ECDHPolicyUri is unsupported.

## Out of scope

- Other ECC curves/suites (nistP384, brainpool, curve448, AES-GCM variants) and RSA-DH.
- CTT verification (internal KAT + e2e tests are the conformance evidence).
- Enabling `MUC_OPCUA_ECC` on nano/micro/embedded by default (legal but a later product call
  pending `.text` measurement).
