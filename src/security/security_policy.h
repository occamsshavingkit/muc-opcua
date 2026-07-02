/* src/security/security_policy.h
 * SecurityPolicy identification and the Basic256Sha256 algorithm-parameter table
 * (OPC 10000-7). The server supports SecurityPolicy None and Basic256Sha256. */
#ifndef MUC_OPCUA_SECURITY_POLICY_H
#define MUC_OPCUA_SECURITY_POLICY_H

#include "muc_opcua/opcua_types.h"
#include <stddef.h>

typedef enum {
    MU_SECURITY_POLICY_INVALID_ID = 0,
    MU_SECURITY_POLICY_NONE_ID,
    MU_SECURITY_POLICY_BASIC256SHA256_ID,
    MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID,
    MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID
} mu_security_policy_id_t;

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
int mu_security_policy_allows_username_password_tokens(mu_security_policy_id_t policy);

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
