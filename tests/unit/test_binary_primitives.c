/* tests/unit/test_binary_primitives.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_primitive_roundtrip(void) {
    TEST_IGNORE_MESSAGE("Implement binary primitive roundtrip test");
#if 0
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&writer, true));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&writer, -12345));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&writer, 12345));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_float(&writer, 3.14f));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_statuscode(&writer, MU_STATUS_BAD_INTERNALERROR));
    
    mu_binary_reader_init(&reader, buffer, writer.position);
    opcua_boolean_t b = false;
    opcua_int32_t i = 0;
    opcua_uint32_t u = 0;
    opcua_float_t f = 0.0f;
    opcua_statuscode_t s = MU_STATUS_GOOD;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_boolean(&reader, &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &i));
    TEST_ASSERT_EQUAL(-12345, i);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &u));
    TEST_ASSERT_EQUAL(12345, u);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_float(&reader, &f));
    TEST_ASSERT_EQUAL_FLOAT(3.14f, f);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, &s));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, s);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_primitive_roundtrip);
    return UNITY_END();
}
