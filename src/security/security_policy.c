/* src/security/security_policy.c */
#include "security_policy.h"
#include <string.h>

static const char POLICY_NONE_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
static const char POLICY_B256S256_URI[] = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";

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
    if (uri_equals(uri, length, POLICY_B256S256_URI)) {
        return MU_SECURITY_POLICY_BASIC256SHA256_ID;
    }
    return MU_SECURITY_POLICY_INVALID_ID;
}

const char *mu_security_policy_uri(mu_security_policy_id_t id) {
    switch (id) {
        case MU_SECURITY_POLICY_NONE_ID:           return POLICY_NONE_URI;
        case MU_SECURITY_POLICY_BASIC256SHA256_ID: return POLICY_B256S256_URI;
        case MU_SECURITY_POLICY_INVALID_ID:
        default:                                   return NULL;
    }
}
