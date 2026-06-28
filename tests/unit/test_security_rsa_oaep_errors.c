/* tests/unit/test_security_rsa_oaep_errors.c */
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

void test_rsa_oaep_malformed_input(void) {
    opcua_byte_t malformed[256];
    memset(malformed, 0x42, sizeof(malformed));

    opcua_byte_t recovered[256];
    size_t rec_len = sizeof(recovered);

    /* Decrypting malformed input should fail */
    opcua_statuscode_t rc = crypto.rsa_oaep_decrypt(crypto.context, malformed, sizeof(malformed), recovered, &rec_len);
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, rc);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rsa_oaep_malformed_input);
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
