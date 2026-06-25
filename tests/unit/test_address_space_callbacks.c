/* tests/unit/test_address_space_callbacks.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

static opcua_statuscode_t test_callback_success(void *context, const mu_nodeid_t *node_id, mu_variant_t *value) {
    (void)context;
    (void)node_id;
    value->type = MU_TYPE_INT32;
    value->value.i32 = 123;
    return MU_STATUS_GOOD;
}

void test_callback_value_source_lifetime(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_CALLBACK;
    source.data.callback.read = test_callback_success;
    source.data.callback.context = NULL;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(123, value.value.i32);
}

static opcua_statuscode_t test_callback_error(void *context, const mu_nodeid_t *node_id, mu_variant_t *value) {
    (void)context;
    (void)node_id;
    (void)value;
    return MU_STATUS_BAD_DEVICEFAILURE;
}

void test_callback_value_source_statuscode_propagation(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    
    source.type = MU_VALUESOURCE_CALLBACK;
    source.data.callback.read = test_callback_error;
    source.data.callback.context = NULL;
    
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DEVICEFAILURE, mu_value_source_read(&source, &id, &value));
}
