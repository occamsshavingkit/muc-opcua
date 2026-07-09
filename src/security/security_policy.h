/* src/security/security_policy.h
 * SecurityPolicy identification and the Basic256Sha256 algorithm-parameter table
 * (OPC 10000-7). The server supports SecurityPolicy None and Basic256Sha256. */
#ifndef MUC_OPCUA_SECURITY_POLICY_H
#define MUC_OPCUA_SECURITY_POLICY_H

#include "muc_opcua/opcua_types.h"
#include "muc_opcua/platform.h" /* mu_ecc_curve_t */
#include <stddef.h>

typedef enum {
    MU_SECURITY_POLICY_INVALID_ID = 0,
    MU_SECURITY_POLICY_NONE_ID,
    MU_SECURITY_POLICY_BASIC256SHA256_ID,
    MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID,
    MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID,
    /* ECC SecurityPolicies (spec 059). Present in the enum regardless of build so
       switch statements stay total; only advertised/selectable when MUC_OPCUA_ECC. */
    MU_SECURITY_POLICY_ECC_NISTP256_ID,
    MU_SECURITY_POLICY_ECC_CURVE25519_ID
} mu_security_policy_id_t;

/* Asymmetric-crypto family of a policy: RSA (the classic policies) or ECC
   (ephemeral-ECDH handshake, spec 059). Determines the OPN handshake shape. */
typedef enum {
    MU_ASYM_FAMILY_NONE = 0,
    MU_ASYM_FAMILY_RSA,
    MU_ASYM_FAMILY_ECC
} mu_asym_family_t;

/* Symmetric protection mode: AES-CBC + HMAC-SHA256 (RSA policies and ECC-nistP256)
   or the ChaCha20-Poly1305 AEAD (ECC-curve25519). */
typedef enum {
    MU_SYM_MODE_NONE = 0,
    MU_SYM_MODE_CBC_HMAC,
    MU_SYM_MODE_AEAD_CHACHA20POLY1305
} mu_sym_mode_t;

/* MessageSecurityMode (OPC 10000-4 §7.15) — values match the wire encoding. */
typedef enum {
    MU_MESSAGE_SECURITY_MODE_INVALID = 0,
    MU_MESSAGE_SECURITY_MODE_NONE = 1,
    MU_MESSAGE_SECURITY_MODE_SIGN = 2,
    MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT = 3
} mu_message_security_mode_t;

size_t mu_security_policy_signature_key_length(mu_security_policy_id_t policy);
size_t mu_security_policy_encryption_key_length(mu_security_policy_id_t policy);
size_t mu_security_policy_encryption_block_size(mu_security_policy_id_t policy);
size_t mu_security_policy_signature_length(mu_security_policy_id_t policy);
size_t mu_security_policy_nonce_length(mu_security_policy_id_t policy);
/* Symmetric IV length (bytes): the cipher block size for CBC policies, 12 for the
   ChaCha20-Poly1305 AEAD policy. */
size_t mu_security_policy_iv_length(mu_security_policy_id_t policy);
int mu_security_policy_allows_username_password_tokens(mu_security_policy_id_t policy);

/* Asymmetric family (RSA vs ECC) and symmetric mode (CBC-HMAC vs AEAD) of a policy;
   the handshake and chunk codec branch on these instead of enumerating policy ids. */
mu_asym_family_t mu_security_policy_asym_family(mu_security_policy_id_t policy);
mu_sym_mode_t mu_security_policy_sym_mode(mu_security_policy_id_t policy);
/* For ECC policies, the named curve; unspecified for non-ECC policies. Returns 1
   and sets *curve for ECC policies, 0 otherwise. */
int mu_security_policy_ecc_curve(mu_security_policy_id_t policy, mu_ecc_curve_t *curve);

/* Basic256Sha256 algorithm parameters (OPC 10000-7 §6.x profile table). */
#define MU_B256S256_SIGNATURE_KEY_LENGTH 32  /* HMAC-SHA256 key (bytes) */
#define MU_B256S256_ENCRYPTION_KEY_LENGTH 32 /* AES-256 key (bytes) */
#define MU_B256S256_ENCRYPTION_BLOCK_SIZE 16 /* AES block / IV (bytes) */
#define MU_B256S256_SIGNATURE_LENGTH 32      /* HMAC-SHA256 output (bytes) */
#define MU_B256S256_NONCE_LENGTH 32          /* SecureChannelNonceLength */
#define MU_B256S256_MIN_ASYMMETRIC_KEY_BITS 2048
#define MU_B256S256_MAX_ASYMMETRIC_KEY_BITS 4096

/* Bytes of derived key material per direction: signing key + encryption key + IV. */
#define MU_B256S256_DERIVED_KEY_LENGTH                                                                                 \
    (MU_B256S256_SIGNATURE_KEY_LENGTH + MU_B256S256_ENCRYPTION_KEY_LENGTH + MU_B256S256_ENCRYPTION_BLOCK_SIZE)

/* ECC SecurityPolicy parameters (spec 059), from the OPC-10000-7 facet limits.
   ECC-nistP256 [ECC-B]: ECDSA-SHA256 + AES128-CBC + HMAC-SHA256; 64-byte channel
   nonce (== ephemeral P-256 pubkey x||y). ECC-curve25519 [ECC-A]: Ed25519 +
   ChaCha20-Poly1305 AEAD; 32-byte channel nonce (== X25519 pubkey), no MAC key. */
#define MU_ECC_NISTP256_SIGNATURE_KEY_LENGTH 32  /* HMAC-SHA256 key */
#define MU_ECC_NISTP256_ENCRYPTION_KEY_LENGTH 16 /* AES-128 key */
#define MU_ECC_NISTP256_IV_LENGTH 16
#define MU_ECC_NISTP256_MAC_LENGTH 32 /* HMAC-SHA256 output */
#define MU_ECC_NISTP256_NONCE_LENGTH 64
#define MU_ECC_NISTP256_DERIVED_KEY_LENGTH                                                                             \
    (MU_ECC_NISTP256_SIGNATURE_KEY_LENGTH + MU_ECC_NISTP256_ENCRYPTION_KEY_LENGTH + MU_ECC_NISTP256_IV_LENGTH)

#define MU_ECC_CURVE25519_ENCRYPTION_KEY_LENGTH 32 /* ChaCha20 key */
#define MU_ECC_CURVE25519_IV_LENGTH 12
#define MU_ECC_CURVE25519_MAC_LENGTH 16 /* Poly1305 tag */
#define MU_ECC_CURVE25519_NONCE_LENGTH 32
/* No derived signature key (AEAD): encryption key + IV only. */
#define MU_ECC_CURVE25519_DERIVED_KEY_LENGTH (MU_ECC_CURVE25519_ENCRYPTION_KEY_LENGTH + MU_ECC_CURVE25519_IV_LENGTH)

/* Classify a SecurityPolicyUri. A NULL/empty URI means None (an OPN may omit it).
   Unknown URIs return MU_SECURITY_POLICY_INVALID_ID. */
mu_security_policy_id_t mu_security_policy_from_uri(const opcua_byte_t *uri, size_t length);

/* The canonical URI string for a policy id, or NULL for INVALID. */
const char *mu_security_policy_uri(mu_security_policy_id_t id);

/* The SignatureData.algorithm URI for the policy's asymmetric (RSA) signatures, or
   NULL for None/Invalid. OPC-10000-7 SecurityPolicy definitions: the PKCS#1.5
   policies use the xmldsig rsa-sha256 URI; Aes256_Sha256_RsaPss uses the
   rsa-pss-sha2-256 URI. */
const char *mu_security_policy_asym_signature_uri(mu_security_policy_id_t policy);

/* True iff the policy's asymmetric signatures use RSA-PSS-SHA256
   (Aes256_Sha256_RsaPss); false for the PKCS#1.5 policies and None/Invalid. */
opcua_boolean_t mu_security_policy_uses_pss(mu_security_policy_id_t policy);

#endif /* MUC_OPCUA_SECURITY_POLICY_H */
