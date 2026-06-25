/* tests/unit/test_binary_extension_object.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_extension_object_roundtrip(void) {
    TEST_IGNORE_MESSAGE("Implement binary ExtensionObject test");
#if 0
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    
    mu_nodeid_t type_id = { 1, MU_NODEID_NUMERIC, { 1000 } };
    opcua_byte_t body_data[] = { 0x01, 0x02, 0x03, 0x04 };
    
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_extension_object_header(&writer, &type_id, 4));
    
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_nodeid_t read_type_id;
    size_t length;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&reader, &read_type_id, &length));
    
    TEST_ASSERT_TRUE(mu_nodeid_equal(&type_id, &read_type_id));
    TEST_ASSERT_EQUAL(4, length);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_extension_object_roundtrip);
    return UNITY_END();
}
