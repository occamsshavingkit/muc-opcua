/* tests/unit/test_binary_nodeid.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_nodeid_numeric_roundtrip(void) {
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_nodeid_t id = {1, MU_NODEID_NUMERIC, {1000}};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &id));

    /* Byte-level assertion: Numeric NodeId encoding (OPC 10000-6 §5.2.2.9):
       FourByte format 0x01 (ns<=255, id<=65535), ns=1, id=1000=0x3E8 */
    const opcua_byte_t expected[] = {0x01, 0x01, 0xE8, 0x03};
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), writer.position);
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_nodeid_t read_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &read_id));

    TEST_ASSERT_TRUE(mu_nodeid_equal(&id, &read_id));
}

void test_binary_nodeid_string_roundtrip(void) {
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_nodeid_t id = {1, MU_NODEID_STRING, {.string = {4, (const opcua_byte_t *)"test"}}};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &id));

    /* Byte-level assertion: String NodeId encoding (OPC 10000-6 §5.2.2.9):
       format 0x03 (String), uint16 ns=1, String "test" */
    const opcua_byte_t expected[] = {0x03, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x74, 0x65, 0x73, 0x74};
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), writer.position);
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_nodeid_t read_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &read_id));

    TEST_ASSERT_TRUE(mu_nodeid_equal(&id, &read_id));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_nodeid_numeric_roundtrip);
    RUN_TEST(test_binary_nodeid_string_roundtrip);
    return UNITY_END();
}
