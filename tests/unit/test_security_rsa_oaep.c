/* tests/unit/test_security_rsa_oaep.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <string.h>

#ifdef MICRO_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"

static mu_crypto_adapter_t crypto;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&crypto);
}

void test_rsa_oaep_roundtrip(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));

    const opcua_byte_t secret[] = "Aes128-Sha256-RsaOaep_secret";
    opcua_byte_t ciphertext[512];
    size_t ct_len = sizeof(ciphertext);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.rsa_oaep_encrypt(crypto.context, cert, cert_len, secret, sizeof(secret),
                                                              ciphertext, &ct_len));
    TEST_ASSERT_GREATER_THAN(0, ct_len);

    opcua_byte_t recovered[512];
    size_t rec_len = sizeof(recovered);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.rsa_oaep_decrypt(crypto.context, ciphertext, ct_len, recovered, &rec_len));
    TEST_ASSERT_EQUAL(sizeof(secret), rec_len);
    TEST_ASSERT_EQUAL_MEMORY(secret, recovered, sizeof(secret));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rsa_oaep_roundtrip);
    return UNITY_END();
}

#else
void setUp(void) {}
void tearDown(void) {}
void test_skipped(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_skipped);
    return UNITY_END();
}
#endif
