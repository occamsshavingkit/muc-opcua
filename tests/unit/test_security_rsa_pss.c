/* tests/unit/test_security_rsa_pss.c */
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

void test_rsa_pss_roundtrip(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));

    const opcua_byte_t data[] = "Aes256-Sha256-RsaPss_data";
    opcua_byte_t sig[512];
    size_t sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.rsa_pss_sha256_sign(crypto.context, data, sizeof(data), sig, &sig_len));
    TEST_ASSERT_GREATER_THAN(0, sig_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.rsa_pss_sha256_verify(crypto.context, cert, cert_len, data, sizeof(data), sig, sig_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rsa_pss_roundtrip);
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
