/* tests/unit/test_security_policy_selection.c */
#include "unity.h"
#include "security/security_policy.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static mu_security_policy_id_t from_uri(const char *uri) {
    return mu_security_policy_from_uri((const opcua_byte_t *)uri, strlen(uri));
}

void test_modern_policies_recognized(void) {
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID,
        from_uri("http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep"));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID,
        from_uri("http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss"));
}

void test_modern_policies_uri_roundtrip(void) {
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep",
        mu_security_policy_uri(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID));
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss",
        mu_security_policy_uri(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID));
}

void test_modern_policies_parameters(void) {
    /* Aes128_Sha256_RsaOaep */
    TEST_ASSERT_EQUAL(32, mu_security_policy_signature_key_length(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID));
    TEST_ASSERT_EQUAL(16, mu_security_policy_encryption_key_length(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID));
    TEST_ASSERT_EQUAL(16, mu_security_policy_encryption_block_size(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID));
    TEST_ASSERT_EQUAL(32, mu_security_policy_signature_length(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID));
    TEST_ASSERT_EQUAL(32, mu_security_policy_nonce_length(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID));

    /* Aes256_Sha256_RsaPss */
    TEST_ASSERT_EQUAL(32, mu_security_policy_signature_key_length(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID));
    TEST_ASSERT_EQUAL(32, mu_security_policy_encryption_key_length(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID));
    TEST_ASSERT_EQUAL(16, mu_security_policy_encryption_block_size(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID));
    TEST_ASSERT_EQUAL(32, mu_security_policy_signature_length(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID));
    TEST_ASSERT_EQUAL(32, mu_security_policy_nonce_length(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_modern_policies_recognized);
    RUN_TEST(test_modern_policies_uri_roundtrip);
    RUN_TEST(test_modern_policies_parameters);
    return UNITY_END();
}
