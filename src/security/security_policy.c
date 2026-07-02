/* src/security/security_policy.c */
#include "security_policy.h"
#include <string.h>

static const char POLICY_NONE_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
#ifdef MUC_OPCUA_SECURITY
static const char POLICY_B256S256_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";
static const char POLICY_AES128_OAEP_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep";
static const char POLICY_AES256_PSS_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss";
#endif

/* Feature 025 (F12): one parameter row per SecurityPolicy replaces the parallel
   switch statements each accessor used to carry. Adding a policy is now one row
   plus its enum value. Values are unchanged from the prior switches (OPC-10000-7
   SecurityPolicy definitions). Secured rows exist only when MUC_OPCUA_SECURITY
   is built. */
typedef struct {
    mu_security_policy_id_t id;
    const char *uri;
    opcua_uint16_t sig_key_len;   /* HMAC-SHA256 key length */
    opcua_uint16_t enc_key_len;   /* symmetric encryption key length */
    opcua_uint16_t block_size;    /* symmetric cipher block size */
    opcua_uint16_t sig_len;       /* symmetric signature (MAC) length */
    opcua_uint16_t nonce_len;     /* per-channel nonce length */
    opcua_byte_t allows_username; /* password-bearing UserNameIdentityToken allowed */
} mu_security_policy_params_t;

static const mu_security_policy_params_t POLICY_TABLE[] = {
    {MU_SECURITY_POLICY_NONE_ID, POLICY_NONE_URI, 0, 0, 0, 0, 0, 0},
#ifdef MUC_OPCUA_SECURITY
    {MU_SECURITY_POLICY_BASIC256SHA256_ID, POLICY_B256S256_URI, 32, 32, 16, 32, 32, 1},
    {MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID, POLICY_AES128_OAEP_URI, 32, 16, 16, 32, 32, 1},
    {MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID, POLICY_AES256_PSS_URI, 32, 32, 16, 32, 32, 1},
#endif
};

static const mu_security_policy_params_t *policy_row(mu_security_policy_id_t id) {
    for (size_t i = 0; i < sizeof(POLICY_TABLE) / sizeof(POLICY_TABLE[0]); ++i) {
        if (POLICY_TABLE[i].id == id) {
            return &POLICY_TABLE[i];
        }
    }
    return NULL;
}

static int uri_equals(const opcua_byte_t *uri, size_t length, const char *expected) {
    size_t expected_len = strlen(expected);
    return length == expected_len && memcmp(uri, expected, length) == 0;
}

mu_security_policy_id_t mu_security_policy_from_uri(const opcua_byte_t *uri, size_t length) {
    if (uri == NULL || length == 0) {
        return MU_SECURITY_POLICY_NONE_ID;
    }
    for (size_t i = 0; i < sizeof(POLICY_TABLE) / sizeof(POLICY_TABLE[0]); ++i) {
        if (uri_equals(uri, length, POLICY_TABLE[i].uri)) {
            return POLICY_TABLE[i].id;
        }
    }
    return MU_SECURITY_POLICY_INVALID_ID;
}

const char *mu_security_policy_uri(mu_security_policy_id_t id) {
    const mu_security_policy_params_t *row = policy_row(id);
    return row ? row->uri : NULL;
}

size_t mu_security_policy_signature_key_length(mu_security_policy_id_t policy) {
    const mu_security_policy_params_t *row = policy_row(policy);
    return row ? row->sig_key_len : 0;
}

int mu_security_policy_allows_username_password_tokens(mu_security_policy_id_t policy) {
    /* OPC-10000-4 sections 5.7.3.3 and 7.40.2.1: password-bearing
       UserNameIdentityTokens are rejected by default over SecurityPolicy#None. */
    const mu_security_policy_params_t *row = policy_row(policy);
    return row ? row->allows_username : 0;
}

size_t mu_security_policy_encryption_key_length(mu_security_policy_id_t policy) {
    const mu_security_policy_params_t *row = policy_row(policy);
    return row ? row->enc_key_len : 0;
}

size_t mu_security_policy_encryption_block_size(mu_security_policy_id_t policy) {
    const mu_security_policy_params_t *row = policy_row(policy);
    return row ? row->block_size : 0;
}

size_t mu_security_policy_signature_length(mu_security_policy_id_t policy) {
    const mu_security_policy_params_t *row = policy_row(policy);
    return row ? row->sig_len : 0;
}

size_t mu_security_policy_nonce_length(mu_security_policy_id_t policy) {
    const mu_security_policy_params_t *row = policy_row(policy);
    return row ? row->nonce_len : 0;
}

#ifdef MUC_OPCUA_SECURITY
/* OPC-10000-7 SecurityPolicy asymmetric-signature algorithm URIs (feature 026). */
static const char SIG_URI_RSA_SHA256[] = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
static const char SIG_URI_RSA_PSS_SHA256[] = "http://opcfoundation.org/UA/security/rsa-pss-sha2-256";
#endif

const char *mu_security_policy_asym_signature_uri(mu_security_policy_id_t policy) {
#ifdef MUC_OPCUA_SECURITY
    switch (policy) {
    case MU_SECURITY_POLICY_BASIC256SHA256_ID:
    case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID:
        return SIG_URI_RSA_SHA256;
    case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:
        return SIG_URI_RSA_PSS_SHA256;
    default:
        return NULL;
    }
#else
    (void)policy;
    return NULL;
#endif
}

opcua_boolean_t mu_security_policy_uses_pss(mu_security_policy_id_t policy) {
#ifdef MUC_OPCUA_SECURITY
    return policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID;
#else
    (void)policy;
    return false;
#endif
}
