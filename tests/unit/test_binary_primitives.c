/* tests/unit/test_binary_primitives.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <float.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const opcua_sbyte_t s_sbyte_values[] = {INT8_MIN, -1, 0, INT8_MAX};
static const opcua_int16_t s_int16_values[] = {INT16_MIN, -1, 0, INT16_MAX};
static const opcua_uint16_t s_uint16_values[] = {0u, 1u, UINT16_MAX};
static const opcua_int64_t s_int64_values[] = {INT64_MIN, -1, 0, INT64_MAX};
static const opcua_uint64_t s_uint64_values[] = {0u, 1u, UINT64_MAX};
static const opcua_double_t s_double_values[] = {-DBL_MAX, -1.5, 0.0, DBL_MAX};
static const opcua_datetime_t s_datetime_values[] = {INT64_MIN, -116444736000000000LL, 0, INT64_MAX};

static void write_sbyte_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_sbyte_values) / sizeof(s_sbyte_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_sbyte(writer, s_sbyte_values[i]));
    }
}

static void write_int16_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_int16_values) / sizeof(s_int16_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int16(writer, s_int16_values[i]));
    }
}

static void write_uint16_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_uint16_values) / sizeof(s_uint16_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(writer, s_uint16_values[i]));
    }
}

static void write_int64_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_int64_values) / sizeof(s_int64_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(writer, s_int64_values[i]));
    }
}

static void write_uint64_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_uint64_values) / sizeof(s_uint64_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint64(writer, s_uint64_values[i]));
    }
}

static void write_double_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_double_values) / sizeof(s_double_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_double(writer, s_double_values[i]));
    }
}

static void write_datetime_values(mu_binary_writer_t *writer) {
    for (size_t i = 0u; i < sizeof(s_datetime_values) / sizeof(s_datetime_values[0]); ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(writer, s_datetime_values[i]));
    }
}

static void read_sbyte_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_sbyte_values) / sizeof(s_sbyte_values[0]); ++i) {
        opcua_sbyte_t value = 0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_sbyte(reader, &value));
        TEST_ASSERT_EQUAL_INT(s_sbyte_values[i], value);
    }
}

static void read_int16_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_int16_values) / sizeof(s_int16_values[0]); ++i) {
        opcua_int16_t value = 0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int16(reader, &value));
        TEST_ASSERT_EQUAL_INT16(s_int16_values[i], value);
    }
}

static void read_uint16_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_uint16_values) / sizeof(s_uint16_values[0]); ++i) {
        opcua_uint16_t value = 0u;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint16(reader, &value));
        TEST_ASSERT_EQUAL_UINT16(s_uint16_values[i], value);
    }
}

static void read_int64_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_int64_values) / sizeof(s_int64_values[0]); ++i) {
        opcua_int64_t value = 0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(reader, &value));
        TEST_ASSERT_EQUAL_INT64(s_int64_values[i], value);
    }
}

static void read_uint64_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_uint64_values) / sizeof(s_uint64_values[0]); ++i) {
        opcua_uint64_t value = 0u;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint64(reader, &value));
        TEST_ASSERT_EQUAL_UINT64(s_uint64_values[i], value);
    }
}

static void read_double_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_double_values) / sizeof(s_double_values[0]); ++i) {
        opcua_double_t value = 0.0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_double(reader, &value));
        TEST_ASSERT_EQUAL_MEMORY(&s_double_values[i], &value, sizeof(value));
    }
}

static void read_datetime_values(mu_binary_reader_t *reader) {
    for (size_t i = 0u; i < sizeof(s_datetime_values) / sizeof(s_datetime_values[0]); ++i) {
        opcua_datetime_t value = 0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(reader, &value));
        TEST_ASSERT_EQUAL_INT64(s_datetime_values[i], value);
    }
}

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

void test_binary_missing_primitive_boundary_roundtrips(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    /* OPC-10000-6 sections 5.2.2.2, 5.2.2.3, and 5.2.2.5: primitive scalar
       values are little-endian fixed-width values and must round-trip at bounds. */
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    write_sbyte_values(&writer);
    write_int16_values(&writer);
    write_uint16_values(&writer);
    write_int64_values(&writer);
    write_uint64_values(&writer);
    write_double_values(&writer);
    write_datetime_values(&writer);

    mu_binary_reader_init(&reader, buffer, writer.position);
    read_sbyte_values(&reader);
    read_int16_values(&reader);
    read_uint16_values(&reader);
    read_int64_values(&reader);
    read_uint64_values(&reader);
    read_double_values(&reader);
    read_datetime_values(&reader);
    TEST_ASSERT_EQUAL_size_t(writer.position, reader.position);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_primitive_roundtrip);
    RUN_TEST(test_binary_reader_rejects_overflowed_bounds_check);
    RUN_TEST(test_binary_reader_preserves_status_and_position_after_primitive_error);
    RUN_TEST(test_binary_writer_preserves_status_and_position_after_primitive_error);
    RUN_TEST(test_binary_missing_primitive_boundary_roundtrips);
    return UNITY_END();
}
