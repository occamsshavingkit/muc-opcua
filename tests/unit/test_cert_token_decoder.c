/* tests/unit/test_cert_token_decoder.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_decode_cert_token_direct(void) {
    /* Hardcoded binary representation of CertificateIdentityToken body:
       - policyId: "cert_policy" (len 11) -> 0b 00 00 00 + "cert_policy"
       - certificateData: "mock_cert" (len 9) -> 09 00 00 00 + "mock_cert"
    */
    opcua_byte_t buf[] = {0x0b, 0x00, 0x00, 0x00, 'c', 'e', 'r', 't', '_', 'p', 'o', 'l', 'i', 'c', 'y',
                          0x09, 0x00, 0x00, 0x00, 'm', 'o', 'c', 'k', '_', 'c', 'e', 'r', 't'};

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buf, sizeof(buf));

    mu_certificate_identity_token_t token;
    memset(&token, 0, sizeof(token));

    opcua_statuscode_t status = mu_binary_read_certificate_identity_token(&reader, &token);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(11, token.policy_id.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.policy_id.data, "cert_policy", 11));

    TEST_ASSERT_EQUAL(9, token.certificate_data.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.certificate_data.data, "mock_cert", 9));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_cert_token_direct);
    return UNITY_END();
}
