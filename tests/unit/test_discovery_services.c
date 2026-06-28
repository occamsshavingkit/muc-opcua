/* tests/unit/test_discovery_services.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/discovery.h"

void test_discovery_findservers_response(void) {
    mu_server_config_t config = {0};
    config.application_uri = "urn:test:app";
    config.product_uri = "urn:test:product";
    config.application_name = "Test App";
    config.endpoint_url = "opc.tcp://localhost:4840";

    mu_application_description_t desc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_discovery_get_application_description(&config, &desc));

    TEST_ASSERT_EQUAL_STRING("urn:test:app", desc.application_uri);
    TEST_ASSERT_EQUAL_STRING("urn:test:product", desc.product_uri);
    TEST_ASSERT_EQUAL_STRING("Test App", desc.application_name);
    TEST_ASSERT_EQUAL(MU_APPLICATION_TYPE_SERVER, desc.application_type);
    TEST_ASSERT_EQUAL_STRING("opc.tcp://localhost:4840", desc.discovery_url);
}

void test_discovery_getendpoints_response(void) {
    mu_server_config_t config = {0};
    config.application_uri = "urn:test:app";
    config.product_uri = "urn:test:product";
    config.application_name = "Test App";
    config.endpoint_url = "opc.tcp://localhost:4840";

    mu_endpoint_description_t desc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_discovery_get_endpoint_description(&config, &desc));

    TEST_ASSERT_EQUAL_STRING("opc.tcp://localhost:4840", desc.endpoint_url);
    TEST_ASSERT_EQUAL_STRING("urn:test:app", desc.server.application_uri);

    TEST_ASSERT_EQUAL(MU_MESSAGE_SECURITY_MODE_NONE, desc.security_mode);
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#None", desc.security_policy_uri);

#ifdef MICRO_OPCUA_USER_AUTH
    TEST_ASSERT_EQUAL(2, desc.num_user_identity_tokens);
    TEST_ASSERT_EQUAL_STRING("username", desc.user_identity_tokens[1].policy_id);
    TEST_ASSERT_EQUAL(MU_USER_TOKEN_TYPE_USERNAME, desc.user_identity_tokens[1].token_type);
#else
    TEST_ASSERT_EQUAL(1, desc.num_user_identity_tokens);
#endif
    TEST_ASSERT_EQUAL_STRING("anonymous", desc.user_identity_tokens[0].policy_id);
    TEST_ASSERT_EQUAL(MU_USER_TOKEN_TYPE_ANONYMOUS, desc.user_identity_tokens[0].token_type);

    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary",
                             desc.transport_profile_uri);
    TEST_ASSERT_EQUAL(0, desc.security_level);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_discovery_findservers_response);
    RUN_TEST(test_discovery_getendpoints_response);
    return UNITY_END();
}
