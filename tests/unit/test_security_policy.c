/* tests/unit/test_security_policy.c
 * SecurityPolicy URI parsing and the Basic256Sha256 algorithm-parameter table
 * (OPC 10000-7 profiles). Pure logic; no crypto backend required. */
#include "unity.h"
#include "security/security_policy.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static mu_security_policy_id_t from_uri(const char *uri) {
    return mu_security_policy_from_uri((const opcua_byte_t *)uri, strlen(uri));
}

void test_policy_none_recognized(void) {
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_NONE_ID,
        from_uri("http://opcfoundation.org/UA/SecurityPolicy#None"));
}

void test_policy_basic256sha256_recognized(void) {
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_BASIC256SHA256_ID,
        from_uri("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256"));
}

void test_unknown_policy_rejected(void) {
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_INVALID_ID,
        from_uri("http://opcfoundation.org/UA/SecurityPolicy#Basic128Rsa15"));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_INVALID_ID, from_uri("garbage"));
    /* A non-empty but truncated prefix must not match None. */
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_INVALID_ID,
        from_uri("http://opcfoundation.org/UA/SecurityPolicy#Non"));
}

void test_empty_policy_is_none(void) {
    /* An OPN may carry a zero-length SecurityPolicyUri; that means None. */
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_NONE_ID, mu_security_policy_from_uri(NULL, 0));
}

void test_uri_roundtrip(void) {
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#None",
        mu_security_policy_uri(MU_SECURITY_POLICY_NONE_ID));
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256",
        mu_security_policy_uri(MU_SECURITY_POLICY_BASIC256SHA256_ID));
    TEST_ASSERT_NULL(mu_security_policy_uri(MU_SECURITY_POLICY_INVALID_ID));
}

void test_basic256sha256_parameters(void) {
    /* OPC 10000-7 Basic256Sha256 profile algorithm parameters. */
    TEST_ASSERT_EQUAL(32, MU_B256S256_SIGNATURE_KEY_LENGTH);   /* HMAC-SHA256 key */
    TEST_ASSERT_EQUAL(32, MU_B256S256_ENCRYPTION_KEY_LENGTH);  /* AES-256 key */
    TEST_ASSERT_EQUAL(16, MU_B256S256_ENCRYPTION_BLOCK_SIZE);  /* AES block / IV */
    TEST_ASSERT_EQUAL(32, MU_B256S256_SIGNATURE_LENGTH);       /* HMAC-SHA256 output */
    TEST_ASSERT_EQUAL(32, MU_B256S256_NONCE_LENGTH);           /* SecureChannelNonceLength */
    TEST_ASSERT_EQUAL(2048, MU_B256S256_MIN_ASYMMETRIC_KEY_BITS);
    TEST_ASSERT_EQUAL(4096, MU_B256S256_MAX_ASYMMETRIC_KEY_BITS);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_policy_none_recognized);
    RUN_TEST(test_policy_basic256sha256_recognized);
    RUN_TEST(test_unknown_policy_rejected);
    RUN_TEST(test_empty_policy_is_none);
    RUN_TEST(test_uri_roundtrip);
    RUN_TEST(test_basic256sha256_parameters);
    return UNITY_END();
}
