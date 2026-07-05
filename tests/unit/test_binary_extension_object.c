/* tests/unit/test_binary_extension_object.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_extension_object_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_nodeid_t type_id = {1, MU_NODEID_NUMERIC, {1000}};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_extension_object_header(&writer, &type_id, 4));

    /* Byte-level assertion: ExtensionObject header (OPC 10000-6 §5.2.2.15):
       NodeId FourByte (fmt=0x01, ns=1, id=1000=0x3E8), encoding=0x01, length=4 */
    const opcua_byte_t expected[] = {0x01, 0x01, 0xE8, 0x03,
                                     0x01, 0x04, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), writer.position);
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_nodeid_t read_type_id;
    size_t length;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&reader, &read_type_id, &length));

    TEST_ASSERT_TRUE(mu_nodeid_equal(&type_id, &read_type_id));
    TEST_ASSERT_EQUAL(4, length);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_extension_object_roundtrip);
    return UNITY_END();
}
