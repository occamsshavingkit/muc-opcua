/* tests/unit/test_address_space_string_limits.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_over_limit_bounded_string_validation(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    opcua_byte_t str_data[65] = {0};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_STRING;
    source.data.static_value.value.str.length = 65;
    source.data.static_value.value.str.data = str_data;
    
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, mu_value_source_read(&source, &id, &value));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_over_limit_bounded_string_validation);
    return UNITY_END();
}
