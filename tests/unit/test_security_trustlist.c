/* tests/unit/test_security_trustlist.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <string.h>

static mu_trust_list_t trust_list;
static const opcua_byte_t cert1[] = "Certificate1_Data";
static const opcua_byte_t cert2[] = "Certificate2_Data";
static const opcua_byte_t *certs[] = {cert1, cert2};
static size_t cert_lengths[] = {sizeof(cert1), sizeof(cert2)};

void setUp(void) {
    trust_list.count = 2;
    trust_list.certificates = certs;
    trust_list.lengths = cert_lengths;
}

void tearDown(void) {}

void test_trust_list_match_success(void) {
    opcua_statuscode_t status = mu_trust_list_match(&trust_list, cert1, sizeof(cert1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    status = mu_trust_list_match(&trust_list, cert2, sizeof(cert2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
}

void test_trust_list_match_failure_unknown_cert(void) {
    const opcua_byte_t unknown_cert[] = "Unknown_Certificate";
    opcua_statuscode_t status = mu_trust_list_match(&trust_list, unknown_cert, sizeof(unknown_cert));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURITYCHECKSFAILED, status);
}

void test_trust_list_match_failure_empty_list(void) {
    mu_trust_list_t empty_list = {0, NULL, NULL};
    opcua_statuscode_t status = mu_trust_list_match(&empty_list, cert1, sizeof(cert1));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, status);
}

void test_trust_list_match_failure_null_cert(void) {
    opcua_statuscode_t status = mu_trust_list_match(&trust_list, NULL, 0);
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_trust_list_match_success);
    RUN_TEST(test_trust_list_match_failure_unknown_cert);
    RUN_TEST(test_trust_list_match_failure_empty_list);
    RUN_TEST(test_trust_list_match_failure_null_cert);
    return UNITY_END();
}
