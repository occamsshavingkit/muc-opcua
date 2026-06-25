/* tests/unit/test_discovery_services.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

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
    TEST_IGNORE_MESSAGE("Implement GetEndpoints response fields test");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_discovery_findservers_response);
    RUN_TEST(test_discovery_getendpoints_response);
    return UNITY_END();
}
