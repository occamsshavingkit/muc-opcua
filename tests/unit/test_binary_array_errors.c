/* tests/unit/test_binary_array_errors.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"

opcua_statuscode_t mu_binary_read_array_length(mu_binary_reader_t *reader, opcua_int32_t *length);

void setUp(void) {}
void tearDown(void) {}

void test_binary_array_null_length_is_accepted(void) {
    opcua_byte_t buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    opcua_int32_t length;
    /* OPC-10000-6 section 5.2.5: Int32 length -1 encodes a null array. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(-1, length);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, reader.status);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

void test_binary_array_empty_length_is_accepted(void) {
    opcua_byte_t buffer[4] = {0x00, 0x00, 0x00, 0x00};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    opcua_int32_t length;
    /* OPC-10000-6 section 5.2.5: Int32 length 0 encodes an empty array. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(0, length);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, reader.status);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

void test_binary_array_positive_length_is_accepted(void) {
    opcua_byte_t buffer[4] = {0x03, 0x00, 0x00, 0x00};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    opcua_int32_t length;
    /* OPC-10000-6 section 5.2.5: positive Int32 length encodes the element count. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(3, length);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, reader.status);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

void test_binary_array_length_below_minus_one_is_rejected(void) {
    opcua_byte_t buffer[5] = {0xFE, 0xFF, 0xFF, 0xFF, 0xAA};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    opcua_int32_t length = 0;
    /* OPC-10000-6 section 5.2.5: only -1 encodes a null array; lower counts are malformed. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(-2, length);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(4, reader.position);

    length = 123;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(123, length);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(4, reader.position);
}

void test_binary_array_truncated_length_preserves_position_and_value(void) {
    opcua_byte_t buffer[3] = {0x03, 0x00, 0x00};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    opcua_int32_t length = 77;
    /* OPC-10000-6 section 5.2.5: the Int32 array length must be complete before it is accepted. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(77, length);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(0, reader.position);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_array_length(&reader, &length));
    TEST_ASSERT_EQUAL_INT32(77, length);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(0, reader.position);
}

void test_binary_array_truncated(void) {
    opcua_byte_t buffer[6] = {2, 0, 0, 0, 42, 0}; /* Length 2, but only 2 bytes for the first uint16 */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    opcua_int32_t length;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &length));
    TEST_ASSERT_EQUAL(2, length);

    opcua_uint16_t val;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint16(&reader, &val));
    TEST_ASSERT_EQUAL(42, val);

    /* The second read should fail */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_uint16(&reader, &val));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);

    val = 99;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_uint16(&reader, &val));
    TEST_ASSERT_EQUAL(99, val);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_array_null_length_is_accepted);
    RUN_TEST(test_binary_array_empty_length_is_accepted);
    RUN_TEST(test_binary_array_positive_length_is_accepted);
    RUN_TEST(test_binary_array_length_below_minus_one_is_rejected);
    RUN_TEST(test_binary_array_truncated_length_preserves_position_and_value);
    RUN_TEST(test_binary_array_truncated);
    return UNITY_END();
}
