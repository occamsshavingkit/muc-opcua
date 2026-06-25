/* tests/unit/test_address_space_values.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_static_value_source_boolean(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_BOOLEAN;
    source.data.static_value.value.b = true;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, value.type);
    TEST_ASSERT_TRUE(value.value.b);
}

void test_static_value_source_int32(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_INT32;
    source.data.static_value.value.i32 = -42;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(-42, value.value.i32);
}

void test_static_value_source_uint32(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_UINT32;
    source.data.static_value.value.ui32 = 42;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, value.type);
    TEST_ASSERT_EQUAL_UINT32(42, value.value.ui32);
}

void test_static_value_source_float(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_FLOAT;
    source.data.static_value.value.f = 3.14f;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, value.type);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, value.value.f);
}

void test_static_value_source_string_empty(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_STRING;
    source.data.static_value.value.str.length = 0;
    source.data.static_value.value.str.data = NULL;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_INT32(0, value.value.str.length);
}

void test_static_value_source_string_64_bytes(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    opcua_byte_t str_data[64] = {0};
    
    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_STRING;
    source.data.static_value.value.str.length = 64;
    source.data.static_value.value.str.data = str_data;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_INT32(64, value.value.str.length);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_static_value_source_boolean);
    RUN_TEST(test_static_value_source_int32);
    RUN_TEST(test_static_value_source_uint32);
    RUN_TEST(test_static_value_source_float);
    RUN_TEST(test_static_value_source_string_empty);
    RUN_TEST(test_static_value_source_string_64_bytes);
    return UNITY_END();
}
