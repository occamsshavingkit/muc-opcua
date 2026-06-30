/* tests/unit/test_binary_bytestring_errors.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

void test_binary_bytestring_read_null_preserves_negative_one_length(void) {
    /* OPC-10000-6 section 5.2.2.7: ByteString length is an Int32; -1 encodes a null value. */
    opcua_byte_t buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes = {0, (const opcua_byte_t *)0x1};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&reader, &bytes));
    TEST_ASSERT_EQUAL_INT32(-1, bytes.length);
    TEST_ASSERT_NULL(bytes.data);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

void test_binary_bytestring_read_empty_preserves_zero_length(void) {
    /* OPC-10000-6 section 5.2.2.7: a zero length ByteString has no payload bytes. */
    opcua_byte_t buffer[4] = {0, 0, 0, 0};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&reader, &bytes));
    TEST_ASSERT_EQUAL_INT32(0, bytes.length);
    TEST_ASSERT_EQUAL_PTR(&buffer[4], bytes.data);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

void test_binary_bytestring_read_positive_preserves_declared_length_and_data(void) {
    /* OPC-10000-6 section 5.2.2.7: payload length is the exact byte count after the Int32 length. */
    opcua_byte_t buffer[7] = {3, 0, 0, 0, 0xDE, 0xAD, 0xBE};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&reader, &bytes));
    TEST_ASSERT_EQUAL_INT32(3, bytes.length);
    TEST_ASSERT_EQUAL_PTR(&buffer[4], bytes.data);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(&buffer[4], bytes.data, 3);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

void test_binary_bytestring_write_null_preserves_negative_one_length(void) {
    /* OPC-10000-6 section 5.2.2.7: null ByteString writes only the Int32 -1 length. */
    opcua_byte_t buffer[4] = {0};
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    mu_bytestring_t bytes = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&writer, &bytes));
    TEST_ASSERT_EQUAL_UINT8(0xFF, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, buffer[2]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, buffer[3]);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), writer.position);
}

void test_binary_bytestring_write_empty_preserves_zero_length(void) {
    /* OPC-10000-6 section 5.2.2.7: empty ByteString writes a zero Int32 length and no payload. */
    opcua_byte_t buffer[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    mu_bytestring_t bytes = {0, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&writer, &bytes));
    TEST_ASSERT_EQUAL_UINT8(0, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[2]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[3]);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), writer.position);
}

void test_binary_bytestring_write_positive_preserves_declared_length_and_data(void) {
    /* OPC-10000-6 section 5.2.2.7: positive length is written before the exact payload bytes. */
    const opcua_byte_t payload[3] = {0xDE, 0xAD, 0xBE};
    opcua_byte_t buffer[7] = {0};
    opcua_byte_t expected[7] = {3, 0, 0, 0, 0xDE, 0xAD, 0xBE};
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    mu_bytestring_t bytes = {3, payload};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&writer, &bytes));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, sizeof(expected));
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), writer.position);
}

void test_binary_bytestring_read_excessive_length_returns_bad_encoding_limits_exceeded(void) {
    /* Current limit: MU_MAX_ENCODED_STRING_LENGTH bounds ByteString payloads on the wire. */
    opcua_byte_t buffer[4] = {0x01, 0x10, 0x00, 0x00}; /* 4097 */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, mu_binary_read_bytestring(&reader, &bytes));
}

void test_binary_bytestring_read_malformed_negative_length_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2.2.7 reserves only -1 for null; other negative lengths are malformed. */
    opcua_byte_t buffer[4] = {0xFE, 0xFF, 0xFF, 0xFF}; /* -2 */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_bytestring(&reader, &bytes));
}

void test_binary_bytestring_read_truncated_length_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2.2.7 length field is a 32-bit signed integer. */
    opcua_byte_t buffer[3] = {1, 0, 0};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_bytestring(&reader, &bytes));
}

void test_binary_bytestring_read_overdeclared_payload_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2.2.7 declared ByteString bytes must be present in the encoded stream. */
    opcua_byte_t buffer[6] = {4, 0, 0, 0, 0xDE, 0xAD};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_bytestring(&reader, &bytes));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_bytestring_read_null_preserves_negative_one_length);
    RUN_TEST(test_binary_bytestring_read_empty_preserves_zero_length);
    RUN_TEST(test_binary_bytestring_read_positive_preserves_declared_length_and_data);
    RUN_TEST(test_binary_bytestring_write_null_preserves_negative_one_length);
    RUN_TEST(test_binary_bytestring_write_empty_preserves_zero_length);
    RUN_TEST(test_binary_bytestring_write_positive_preserves_declared_length_and_data);
    RUN_TEST(test_binary_bytestring_read_excessive_length_returns_bad_encoding_limits_exceeded);
    RUN_TEST(test_binary_bytestring_read_malformed_negative_length_returns_bad_decoding_error);
    RUN_TEST(test_binary_bytestring_read_truncated_length_returns_bad_decoding_error);
    RUN_TEST(test_binary_bytestring_read_overdeclared_payload_returns_bad_decoding_error);
    return UNITY_END();
}
