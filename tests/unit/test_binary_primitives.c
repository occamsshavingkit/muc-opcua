/* tests/unit/test_binary_primitives.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_primitive_roundtrip(void) {
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
}

void test_binary_reader_rejects_overflowed_bounds_check(void) {
    opcua_byte_t backing[6] = {0x78, 0x56, 0x34, 0x12, 0x00, 0x00};
    mu_binary_reader_t reader;
    opcua_uint32_t value = 0;
    size_t overflow_position = SIZE_MAX - 1u;

    /* OPC-10000-6 section 5.2 binary encoding length rules require decoded
       bytes to come from the encoded stream; OPC-10000-4 section 7.38.2 defines
       Bad_DecodingError for invalid stream data. This places the reader one
       byte before SIZE_MAX so a UInt32 read's position + 4 check overflows if
       implemented as direct addition. */
    mu_binary_reader_init(&reader, (const opcua_byte_t *)((uintptr_t)&backing[0] - (uintptr_t)overflow_position),
                          SIZE_MAX);
    reader.position = overflow_position;

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_uint32(&reader, &value));
    TEST_ASSERT_EQUAL_size_t(overflow_position, reader.position);
}

void test_binary_reader_preserves_status_and_position_after_primitive_error(void) {
    const opcua_byte_t buffer[1] = {0x01};
    mu_binary_reader_t reader;
    opcua_uint32_t value = 0xA5A5A5A5u;
    opcua_boolean_t boolean_value = false;

    /* OPC-10000-6 section 5.2.1 defines Binary DataEncoding as a concrete
       encoded stream. A primitive read that cannot consume all required bytes
       must fail without consuming partial stream bytes, and the reader's sticky
       error status must make later primitive reads no-ops. */
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_uint32(&reader, &value));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(0u, reader.position);
    TEST_ASSERT_EQUAL_HEX32(0xA5A5A5A5u, value);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_boolean(&reader, &boolean_value));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
    TEST_ASSERT_EQUAL_size_t(0u, reader.position);
    TEST_ASSERT_FALSE(boolean_value);
}

void test_binary_writer_preserves_status_and_position_after_primitive_error(void) {
    opcua_byte_t buffer[1] = {0xCCu};
    mu_binary_writer_t writer;

    /* OPC-10000-6 section 5.2.1 Binary DataEncoding writes complete primitive
       values to the stream. A primitive write that cannot fit must fail without
       partial output, and the writer's sticky error status must make later
       primitive writes no-ops. */
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGERROR, mu_binary_write_uint32(&writer, 0x12345678u));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGERROR, writer.status);
    TEST_ASSERT_EQUAL_size_t(0u, writer.position);
    TEST_ASSERT_EQUAL_HEX8(0xCCu, buffer[0]);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGERROR, mu_binary_write_boolean(&writer, true));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGERROR, writer.status);
    TEST_ASSERT_EQUAL_size_t(0u, writer.position);
    TEST_ASSERT_EQUAL_HEX8(0xCCu, buffer[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_primitive_roundtrip);
    RUN_TEST(test_binary_reader_rejects_overflowed_bounds_check);
    RUN_TEST(test_binary_reader_preserves_status_and_position_after_primitive_error);
    RUN_TEST(test_binary_writer_preserves_status_and_position_after_primitive_error);
    return UNITY_END();
}
