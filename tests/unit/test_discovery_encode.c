/* tests/unit/test_discovery_encode.c
 * Byte-level tests for the EndpointDescription / ApplicationDescription encoders
 * (OPC 10000-4 7.14, 7.2, 7.41). */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "../../src/services/discovery.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_endpoint_description_encode(void) {
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.application_uri = "urn:test:app";
    config.product_uri = "urn:test:product";
    config.application_name = "Test Server";
    config.endpoint_url = "opc.tcp://localhost:4840";

    mu_endpoint_description_t ep;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_discovery_get_endpoint_description(&config, &ep));

    opcua_byte_t buf[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_endpoint_description_encode(&w, &ep));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_string_t s;
    /* endpointUrl */
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL_MEMORY("opc.tcp://localhost:4840", s.data, 24);

    /* server (ApplicationDescription): applicationUri, productUri, applicationName(LT),
       applicationType, gatewayServerUri, discoveryProfileUri, discoveryUrls[] */
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL_MEMORY("urn:test:app", s.data, 12);
    mu_binary_read_string(&r, &s); /* productUri */
    TEST_ASSERT_EQUAL_MEMORY("urn:test:product", s.data, 16);
    opcua_byte_t lt_mask; mu_binary_read_byte(&r, &lt_mask);
    TEST_ASSERT_EQUAL(0x02, lt_mask); /* applicationName LocalizedText: text only */
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL_MEMORY("Test Server", s.data, 11);
    opcua_uint32_t app_type; mu_binary_read_uint32(&r, &app_type);
    TEST_ASSERT_EQUAL(MU_APPLICATION_TYPE_SERVER, app_type);
    mu_binary_read_string(&r, &s); TEST_ASSERT_EQUAL(-1, s.length); /* gatewayServerUri */
    mu_binary_read_string(&r, &s); TEST_ASSERT_EQUAL(-1, s.length); /* discoveryProfileUri */
    opcua_int32_t n_disc; mu_binary_read_int32(&r, &n_disc);
    TEST_ASSERT_EQUAL(1, n_disc);                                   /* discoveryUrls[] */
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL_MEMORY("opc.tcp://localhost:4840", s.data, 24);

    /* serverCertificate (null) */
    mu_binary_read_string(&r, &s); TEST_ASSERT_EQUAL(-1, s.length);
    /* securityMode */
    opcua_uint32_t mode; mu_binary_read_uint32(&r, &mode);
    TEST_ASSERT_EQUAL(MU_MESSAGE_SECURITY_MODE_NONE, mode);
    /* securityPolicyUri */
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL(47, s.length);
    /* userIdentityTokens[] : 1 x UserTokenPolicy */
    opcua_int32_t n_tok; mu_binary_read_int32(&r, &n_tok);
    TEST_ASSERT_EQUAL(1, n_tok);
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL_MEMORY("anonymous", s.data, 9);              /* policyId */
    opcua_uint32_t tok_type; mu_binary_read_uint32(&r, &tok_type);
    TEST_ASSERT_EQUAL(MU_USER_TOKEN_TYPE_ANONYMOUS, tok_type);
    mu_binary_read_string(&r, &s); TEST_ASSERT_EQUAL(-1, s.length); /* issuedTokenType */
    mu_binary_read_string(&r, &s); TEST_ASSERT_EQUAL(-1, s.length); /* issuerEndpointUrl */
    mu_binary_read_string(&r, &s); TEST_ASSERT_EQUAL(-1, s.length); /* securityPolicyUri */
    /* transportProfileUri */
    mu_binary_read_string(&r, &s);
    TEST_ASSERT_EQUAL(65, s.length);
    /* securityLevel */
    opcua_byte_t level; mu_binary_read_byte(&r, &level);
    TEST_ASSERT_EQUAL(0, level);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_endpoint_description_encode);
    return UNITY_END();
}
