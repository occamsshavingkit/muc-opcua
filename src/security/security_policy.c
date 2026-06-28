/* src/security/security_policy.c */
#include "security_policy.h"
#include <string.h>

static const char POLICY_NONE_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
#ifdef MICRO_OPCUA_SECURITY
static const char POLICY_B256S256_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";
static const char POLICY_AES128_OAEP_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep";
static const char POLICY_AES256_PSS_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss";
#endif

static int uri_equals(const opcua_byte_t *uri, size_t length, const char *expected) {
    size_t expected_len = strlen(expected);
    return length == expected_len && memcmp(uri, expected, length) == 0;
}

mu_security_policy_id_t mu_security_policy_from_uri(const opcua_byte_t *uri, size_t length) {
    if (uri == NULL || length == 0) {
        return MU_SECURITY_POLICY_NONE_ID;
    }
    if (uri_equals(uri, length, POLICY_NONE_URI)) {
        return MU_SECURITY_POLICY_NONE_ID;
    }
#ifdef MICRO_OPCUA_SECURITY
    if (uri_equals(uri, length, POLICY_B256S256_URI)) {
        return MU_SECURITY_POLICY_BASIC256SHA256_ID;
    }
    if (uri_equals(uri, length, POLICY_AES128_OAEP_URI)) {
        return MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID;
    }
    if (uri_equals(uri, length, POLICY_AES256_PSS_URI)) {
        return MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID;
    }
#endif
    return MU_SECURITY_POLICY_INVALID_ID;
}

const char *mu_security_policy_uri(mu_security_policy_id_t id) {
    switch (id) {
        case MU_SECURITY_POLICY_NONE_ID:                 return POLICY_NONE_URI;
#ifdef MICRO_OPCUA_SECURITY
        case MU_SECURITY_POLICY_BASIC256SHA256_ID:       return POLICY_B256S256_URI;
        case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID: return POLICY_AES128_OAEP_URI;
        case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:   return POLICY_AES256_PSS_URI;
#endif
        case MU_SECURITY_POLICY_INVALID_ID:
        default:                                         return NULL;
    }
}

size_t mu_security_policy_signature_key_length(mu_security_policy_id_t policy) {
#ifdef MICRO_OPCUA_SECURITY
    switch (policy) {
        case MU_SECURITY_POLICY_BASIC256SHA256_ID:
        case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID:
        case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:
            return 32;
        default:
            return 0;
    }
#else
    (void)policy;
    return 0;
#endif
}

size_t mu_security_policy_encryption_key_length(mu_security_policy_id_t policy) {
#ifdef MICRO_OPCUA_SECURITY
    switch (policy) {
        case MU_SECURITY_POLICY_BASIC256SHA256_ID:
        case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:
            return 32;
        case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID:
            return 16;
        default:
            return 0;
    }
#else
    (void)policy;
    return 0;
#endif
}

size_t mu_security_policy_encryption_block_size(mu_security_policy_id_t policy) {
#ifdef MICRO_OPCUA_SECURITY
    switch (policy) {
        case MU_SECURITY_POLICY_BASIC256SHA256_ID:
        case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID:
        case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:
            return 16;
        default:
            return 0;
    }
#else
    (void)policy;
    return 0;
#endif
}

size_t mu_security_policy_signature_length(mu_security_policy_id_t policy) {
#ifdef MICRO_OPCUA_SECURITY
    switch (policy) {
        case MU_SECURITY_POLICY_BASIC256SHA256_ID:
        case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID:
        case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:
            return 32;
        default:
            return 0;
    }
#else
    (void)policy;
    return 0;
#endif
}

size_t mu_security_policy_nonce_length(mu_security_policy_id_t policy) {
#ifdef MICRO_OPCUA_SECURITY
    switch (policy) {
        case MU_SECURITY_POLICY_BASIC256SHA256_ID:
        case MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID:
        case MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID:
            return 32;
        default:
            return 0;
    }
#else
    (void)policy;
    return 0;
#endif
}
