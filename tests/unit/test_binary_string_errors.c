/* tests/unit/test_binary_string_errors.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

void test_binary_string_truncated(void) {
    opcua_byte_t buffer[6] = {4, 0, 0, 0, 'A', 'B'}; /* Length 4, but only 2 bytes of data */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
}

void test_binary_string_negative_length(void) {
    opcua_byte_t buffer[4] = {0xFE, 0xFF, 0xFF, 0xFF}; /* Length -2 */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
}

void test_binary_string_excessive_length(void) {
    opcua_byte_t buffer[4] = {0x01, 0x10, 0x00, 0x00}; /* Length 4097, > MU_MAX_ENCODED_STRING_LENGTH (4096) */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, mu_binary_read_string(&reader, &str));
}

void test_binary_string_null_length_is_preserved(void) {
    /* OPC-10000-6 section 5.2.2.4: String length -1 is a null String. */
    opcua_byte_t buffer[4];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    mu_string_t null_str = {-1, (const opcua_byte_t *)"ignored"};
    mu_string_t read_str = {0, (const opcua_byte_t *)"unchanged"};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &null_str));
    TEST_ASSERT_EQUAL(4u, writer.position);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[3]);

    mu_binary_reader_init(&reader, buffer, writer.position);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(-1, read_str.length);
    TEST_ASSERT_NULL(read_str.data);
    TEST_ASSERT_EQUAL(4u, reader.position);
}

void test_binary_string_empty_length_is_preserved(void) {
    /* OPC-10000-6 section 5.2.2.4: zero length is an empty non-null String. */
    opcua_byte_t buffer[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    mu_string_t empty_str = {0, NULL};
    mu_string_t read_str = {-1, NULL};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &empty_str));
    TEST_ASSERT_EQUAL(4u, writer.position);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[3]);

    mu_binary_reader_init(&reader, buffer, writer.position);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(0, read_str.length);
    TEST_ASSERT_EQUAL_PTR(&buffer[4], read_str.data);
    TEST_ASSERT_EQUAL(4u, reader.position);
}

void test_binary_string_positive_length_is_preserved_including_embedded_nul(void) {
    /* OPC-10000-6 section 5.2.2.4: decoders use the byte length, not a terminator. */
    opcua_byte_t buffer[16];
    const opcua_byte_t data[5] = {'O', 'P', 0x00, 'U', 'A'};
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    mu_string_t str = {5, data};
    mu_string_t read_str = {-1, NULL};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &str));
    TEST_ASSERT_EQUAL(9u, writer.position);
    TEST_ASSERT_EQUAL_HEX8(0x05, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[3]);
    TEST_ASSERT_EQUAL_MEMORY(data, &buffer[4], sizeof(data));

    mu_binary_reader_init(&reader, buffer, writer.position);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(5, read_str.length);
    TEST_ASSERT_EQUAL_MEMORY(data, read_str.data, sizeof(data));
    TEST_ASSERT_EQUAL(9u, reader.position);
}

void test_binary_string_excessive_declared_length_is_preserved_on_current_status(void) {
    /* OPC-10000-6 section 5.2.2.4 length is decoded before the configured wire limit is applied. */
    opcua_byte_t buffer[4] = {0x01, 0x10, 0x00, 0x00}; /* 4097 */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, mu_binary_read_string(&reader, &str));
    TEST_ASSERT_EQUAL(4097, str.length);
    TEST_ASSERT_EQUAL(4u, reader.position);
}

void test_binary_string_truncated_payload_preserves_declared_length_on_current_status(void) {
    /* OPC-10000-6 section 5.2.2.4: declared length must fit available encoded bytes. */
    opcua_byte_t buffer[6] = {4, 0, 0, 0, 'O', 'P'};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
    TEST_ASSERT_EQUAL(4, str.length);
    TEST_ASSERT_EQUAL(4u, reader.position);
}

void test_binary_string_malformed_length_field_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2.2.4: String length is an Int32 and must be fully present. */
    opcua_byte_t buffer[3] = {4, 0, 0};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
    TEST_ASSERT_EQUAL(0u, reader.position);
}

void test_binary_string_overdeclared_length_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2: declared String length must fit in the remaining encoded bytes. */
    opcua_byte_t buffer[7] = {5, 0, 0, 0, 'O', 'P', 'C'};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
}

void test_binary_bytestring_overdeclared_length_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2: declared ByteString length must fit in the remaining encoded bytes. */
    opcua_byte_t buffer[7] = {5, 0, 0, 0, 0xDE, 0xAD, 0xBE};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_bytestring_t bytes;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_bytestring(&reader, &bytes));
}

void test_binary_string_reader_position_overflow_returns_bad_decoding_error(void) {
    /* OPC-10000-6 section 5.2: declared String length plus reader position must not overflow. */
    opcua_byte_t backing[5] = {1, 0, 0, 0, 'A'};
    mu_binary_reader_t reader;
    size_t overflow_position = SIZE_MAX - 4u;

    mu_binary_reader_init(&reader, (const opcua_byte_t *)((uintptr_t)&backing[0] - (uintptr_t)overflow_position),
                          SIZE_MAX);
    reader.position = overflow_position;

    mu_string_t str;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
}

void test_binary_string_embedded_overrun(void) {
    /* If the length fits in MU_MAX_STRING_LENGTH, but is larger than the rest of the stream */
    opcua_byte_t buffer[6] = {4, 0, 0, 0, 'A', 'B'};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_string_t str;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_string(&reader, &str));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_string_truncated);
    RUN_TEST(test_binary_string_negative_length);
    RUN_TEST(test_binary_string_excessive_length);
    RUN_TEST(test_binary_string_null_length_is_preserved);
    RUN_TEST(test_binary_string_empty_length_is_preserved);
    RUN_TEST(test_binary_string_positive_length_is_preserved_including_embedded_nul);
    RUN_TEST(test_binary_string_excessive_declared_length_is_preserved_on_current_status);
    RUN_TEST(test_binary_string_truncated_payload_preserves_declared_length_on_current_status);
    RUN_TEST(test_binary_string_malformed_length_field_returns_bad_decoding_error);
    RUN_TEST(test_binary_string_overdeclared_length_returns_bad_decoding_error);
    RUN_TEST(test_binary_bytestring_overdeclared_length_returns_bad_decoding_error);
    RUN_TEST(test_binary_string_reader_position_overflow_returns_bad_decoding_error);
    RUN_TEST(test_binary_string_embedded_overrun);
    return UNITY_END();
}
