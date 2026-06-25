/* tests/unit/test_binary_nodeid.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_nodeid_numeric_roundtrip(void) {
    TEST_IGNORE_MESSAGE("Implement binary NodeId numeric test");
#if 0
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    
    mu_nodeid_t id = { 1, MU_NODEID_NUMERIC, { 1000 } };
    
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &id));
    
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_nodeid_t read_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &read_id));
    
    TEST_ASSERT_TRUE(mu_nodeid_equal(&id, &read_id));
#endif
}

void test_binary_nodeid_string_roundtrip(void) {
    TEST_IGNORE_MESSAGE("Implement binary NodeId string test");
#if 0
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    
    mu_nodeid_t id = { 1, MU_NODEID_STRING, { .string = { 4, (const opcua_byte_t*)"test" } } };
    
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &id));
    
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_nodeid_t read_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &read_id));
    
    TEST_ASSERT_TRUE(mu_nodeid_equal(&id, &read_id));
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_nodeid_numeric_roundtrip);
    RUN_TEST(test_binary_nodeid_string_roundtrip);
    return UNITY_END();
}
