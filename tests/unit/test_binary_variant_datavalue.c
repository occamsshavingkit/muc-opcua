/* tests/unit/test_binary_variant_datavalue.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_variant_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_variant_t variant = {MU_TYPE_INT32, {.i32 = 42}};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &variant));

    /* Byte-level assertion: encoding mask = INT32, value = 42 LE */
    TEST_ASSERT_EQUAL(5u, writer.position);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, buffer[0]);
    TEST_ASSERT_EQUAL(42, buffer[1]); /* low byte of int32 */

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_variant_t read_variant;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &read_variant));

    TEST_ASSERT_EQUAL(MU_TYPE_INT32, read_variant.type);
    TEST_ASSERT_EQUAL(42, read_variant.value.i32);
}

void test_binary_datavalue_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_datavalue_t datavalue;
    (void)memset(&datavalue, 0, sizeof(datavalue));
    datavalue.has_value = true;
    datavalue.value.type = MU_TYPE_INT32;
    datavalue.value.value.i32 = 42;
    datavalue.has_status = true;
    datavalue.status = MU_STATUS_GOOD;

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_datavalue(&writer, &datavalue));

    /* Byte-level assertion: DataValue mask (0x01=hasValue|0x02=hasStatus),
       then Variant: encoding mask INT32 (0x06), int32 42 LE,
       then StatusCode: GOOD (0x00000000) */
    TEST_ASSERT_EQUAL(0x03, buffer[0]);          /* has_value | has_status */
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, buffer[1]); /* variant encoding mask */
    TEST_ASSERT_EQUAL(42, buffer[2]);            /* int32 value low byte */

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_datavalue_t read_datavalue;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_datavalue(&reader, &read_datavalue));

    TEST_ASSERT_TRUE(read_datavalue.has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, read_datavalue.value.type);
    TEST_ASSERT_EQUAL(42, read_datavalue.value.value.i32);
    TEST_ASSERT_TRUE(read_datavalue.has_status);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, read_datavalue.status);
}

static mu_datavalue_t datavalue_for_timestamp_mask(opcua_byte_t mask) {
    mu_datavalue_t value = {0};
    value.has_source_timestamp = (mask & 0x01u) != 0u;
    value.has_source_picoseconds = (mask & 0x02u) != 0u;
    value.has_server_timestamp = (mask & 0x04u) != 0u;
    value.has_server_picoseconds = (mask & 0x08u) != 0u;
    value.source_timestamp = 123456789012345678LL;
    value.source_picoseconds = 1234u;
    value.server_timestamp = 223456789012345678LL;
    value.server_picoseconds = 5678u;
    return value;
}

static opcua_byte_t datavalue_expected_timestamp_mask(const mu_datavalue_t *value) {
    opcua_byte_t mask = 0u;
    if (value->has_source_timestamp) {
        mask |= 0x04u;
    }
    if (value->has_source_picoseconds) {
        mask |= 0x10u;
    }
    if (value->has_server_timestamp) {
        mask |= 0x08u;
    }
    if (value->has_server_picoseconds) {
        mask |= 0x20u;
    }
    return mask;
}

static void assert_datavalue_timestamp_fields_equal(const mu_datavalue_t *expected, const mu_datavalue_t *actual) {
    TEST_ASSERT_EQUAL(expected->has_source_timestamp, actual->has_source_timestamp);
    TEST_ASSERT_EQUAL(expected->has_source_picoseconds, actual->has_source_picoseconds);
    TEST_ASSERT_EQUAL(expected->has_server_timestamp, actual->has_server_timestamp);
    TEST_ASSERT_EQUAL(expected->has_server_picoseconds, actual->has_server_picoseconds);
    if (expected->has_source_timestamp) {
        TEST_ASSERT_EQUAL_INT64(expected->source_timestamp, actual->source_timestamp);
    }
    if (expected->has_source_picoseconds) {
        TEST_ASSERT_EQUAL_UINT16(expected->source_picoseconds, actual->source_picoseconds);
    }
    if (expected->has_server_timestamp) {
        TEST_ASSERT_EQUAL_INT64(expected->server_timestamp, actual->server_timestamp);
    }
    if (expected->has_server_picoseconds) {
        TEST_ASSERT_EQUAL_UINT16(expected->server_picoseconds, actual->server_picoseconds);
    }
}

void test_binary_datavalue_timestamp_picosecond_masks_roundtrip(void) {
    opcua_byte_t buffer[64];

    /* OPC-10000-6 section 5.2.2.17: DataValue timestamp and picosecond mask
       bits control the exact fields on the wire and must survive decode. */
    for (opcua_byte_t mask = 0u; mask < 16u; ++mask) {
        mu_datavalue_t datavalue = datavalue_for_timestamp_mask(mask);
        mu_binary_writer_t writer;
        mu_binary_writer_init(&writer, buffer, sizeof(buffer));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_datavalue(&writer, &datavalue));
        TEST_ASSERT_GREATER_THAN(0u, writer.position);
        TEST_ASSERT_EQUAL_HEX8(datavalue_expected_timestamp_mask(&datavalue), buffer[0]);

        mu_binary_reader_t reader;
        mu_binary_reader_init(&reader, buffer, writer.position);
        mu_datavalue_t read_datavalue = {0};
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_datavalue(&reader, &read_datavalue));
        assert_datavalue_timestamp_fields_equal(&datavalue, &read_datavalue);
        TEST_ASSERT_EQUAL_size_t(writer.position, reader.position);
    }
}

void test_binary_variant_qualifiedname_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_variant_t variant;
    (void)memset(&variant, 0, sizeof(variant));
    variant.type = MU_TYPE_QUALIFIEDNAME;
    variant.value.qualified_name.namespace_index = 1;
    variant.value.qualified_name.name = (mu_string_t){6, (const opcua_byte_t *)"MyVar1"};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &variant));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out));
    TEST_ASSERT_EQUAL(MU_TYPE_QUALIFIEDNAME, out.type);
    TEST_ASSERT_EQUAL(1, out.value.qualified_name.namespace_index);
    TEST_ASSERT_EQUAL(6, out.value.qualified_name.name.length);
    TEST_ASSERT_EQUAL_MEMORY("MyVar1", out.value.qualified_name.name.data, 6);
}

void test_binary_variant_localizedtext_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_variant_t variant;
    (void)memset(&variant, 0, sizeof(variant));
    variant.type = MU_TYPE_LOCALIZEDTEXT;
    variant.value.localized_text.locale = (mu_string_t){-1, NULL}; /* null locale -> absent */
    variant.value.localized_text.text = (mu_string_t){6, (const opcua_byte_t *)"MyVar1"};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &variant));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out));
    TEST_ASSERT_EQUAL(MU_TYPE_LOCALIZEDTEXT, out.type);
    TEST_ASSERT_EQUAL(-1, out.value.localized_text.locale.length); /* absent */
    TEST_ASSERT_EQUAL(6, out.value.localized_text.text.length);
    TEST_ASSERT_EQUAL_MEMORY("MyVar1", out.value.localized_text.text.data, 6);
}

/* A Variant carrying a 1-D array sets the array bit (0x80) in the encoding byte,
   then Int32 length, then the elements (OPC 10000-6 5.2.2.16). */
void test_binary_variant_string_array_encode(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    static const mu_string_t arr[2] = {{3, (const opcua_byte_t *)"foo"}, {3, (const opcua_byte_t *)"bar"}};
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_STRING;
    v.is_array = true;
    v.array_length = 2;
    v.value.array = arr;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &v));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, writer.position);
    opcua_byte_t mask;
    mu_binary_read_byte(&r, &mask);
    TEST_ASSERT_EQUAL(MU_TYPE_STRING | 0x80, mask);
    opcua_int32_t len;
    mu_binary_read_int32(&r, &len);
    TEST_ASSERT_EQUAL(2, len);
    mu_string_t s0, s1;
    mu_binary_read_string(&r, &s0);
    mu_binary_read_string(&r, &s1);
    TEST_ASSERT_EQUAL_MEMORY("foo", s0.data, 3);
    TEST_ASSERT_EQUAL_MEMORY("bar", s1.data, 3);
}

void test_binary_variant_int32_array_encode(void) {
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    static const opcua_int32_t arr[3] = {10, 20, 30};
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_INT32;
    v.is_array = true;
    v.array_length = 3;
    v.value.array = arr;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &v));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, writer.position);
    opcua_byte_t mask;
    mu_binary_read_byte(&r, &mask);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32 | 0x80, mask);
    opcua_int32_t len, v0, v1, v2;
    mu_binary_read_int32(&r, &len);
    mu_binary_read_int32(&r, &v0);
    mu_binary_read_int32(&r, &v1);
    mu_binary_read_int32(&r, &v2);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL(10, v0);
    TEST_ASSERT_EQUAL(20, v1);
    TEST_ASSERT_EQUAL(30, v2);
}

/* OPC-10000-6 section 5.2.2.16 / 5.2.5: a Variant with the array bit set
   decodes to a 1-D array (Int32 length + elements). */
void test_binary_variant_int32_array_roundtrip(void) {
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    static const opcua_int32_t arr[3] = {10, 20, 30};
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_INT32;
    v.is_array = true;
    v.array_length = 3;
    v.value.array = arr;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &v));

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, out.type);
    TEST_ASSERT_TRUE(out.is_array);
    TEST_ASSERT_EQUAL(3, out.array_length);
    TEST_ASSERT_EQUAL_size_t(writer.position, reader.position);

    const opcua_int32_t *got = (const opcua_int32_t *)out.value.array;
    TEST_ASSERT_EQUAL(10, got[0]);
    TEST_ASSERT_EQUAL(20, got[1]);
    TEST_ASSERT_EQUAL(30, got[2]);
    free((void *)out.value.array);
}

void test_binary_variant_string_array_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    static const mu_string_t arr[2] = {{3, (const opcua_byte_t *)"foo"}, {3, (const opcua_byte_t *)"bar"}};
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_STRING;
    v.is_array = true;
    v.array_length = 2;
    v.value.array = arr;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &v));

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out));
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, out.type);
    TEST_ASSERT_TRUE(out.is_array);
    TEST_ASSERT_EQUAL(2, out.array_length);

    const mu_string_t *got = (const mu_string_t *)out.value.array;
    TEST_ASSERT_EQUAL(3, got[0].length);
    TEST_ASSERT_EQUAL_MEMORY("foo", got[0].data, 3);
    TEST_ASSERT_EQUAL(3, got[1].length);
    TEST_ASSERT_EQUAL_MEMORY("bar", got[1].data, 3);
    free((void *)out.value.array);
}

/* Int32 length of -1 is a null array: is_array set, no element buffer. */
void test_binary_variant_null_array_decodes(void) {
    /* encoding mask = INT32 | 0x80, length = -1 */
    const opcua_byte_t buffer[] = {MU_TYPE_INT32 | 0x80, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, out.type);
    TEST_ASSERT_TRUE(out.is_array);
    TEST_ASSERT_EQUAL(-1, out.array_length);
    TEST_ASSERT_NULL(out.value.array);
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), reader.position);
}

/* A claimed element count that exceeds the remaining wire bytes is rejected. */
void test_binary_variant_array_truncated_returns_decoding_error(void) {
    /* encoding mask = INT32 | 0x80, length = 3, only one element present */
    const opcua_byte_t buffer[] = {MU_TYPE_INT32 | 0x80, 0x03, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_variant(&reader, &out));
}

/* OPC-10000-6 section 5.2.2.16: the dimensions bit requires the array bit. */
void test_binary_variant_dimensions_without_array_returns_decoding_error(void) {
    const opcua_byte_t buffer[] = {MU_TYPE_INT32 | 0x40};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_variant_t out;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_variant(&reader, &out));
}

void test_binary_qualifiedname_truncated_name_returns_decoding_error(void) {
    const opcua_byte_t buffer[] = {0x01u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 't'};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_qualified_name_t value;
    /* OPC-10000-6 section 5.2.2.13: QualifiedName.name is a complete String. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_qualified_name(&reader, &value));
}

void test_binary_localizedtext_truncated_text_returns_decoding_error(void) {
    const opcua_byte_t buffer[] = {MU_TYPE_LOCALIZEDTEXT, 0x02u, 0x04u, 0x00u, 0x00u, 0x00u, 't'};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_variant_t value;
    /* OPC-10000-6 section 5.2.2.14: LocalizedText text is a complete String.
       This is wire-reachable through the Variant decoder. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_variant(&reader, &value));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_variant_roundtrip);
    RUN_TEST(test_binary_datavalue_roundtrip);
    RUN_TEST(test_binary_datavalue_timestamp_picosecond_masks_roundtrip);
    RUN_TEST(test_binary_variant_qualifiedname_roundtrip);
    RUN_TEST(test_binary_variant_localizedtext_roundtrip);
    RUN_TEST(test_binary_variant_string_array_encode);
    RUN_TEST(test_binary_variant_int32_array_encode);
    RUN_TEST(test_binary_variant_int32_array_roundtrip);
    RUN_TEST(test_binary_variant_string_array_roundtrip);
    RUN_TEST(test_binary_variant_null_array_decodes);
    RUN_TEST(test_binary_variant_array_truncated_returns_decoding_error);
    RUN_TEST(test_binary_variant_dimensions_without_array_returns_decoding_error);
    RUN_TEST(test_binary_qualifiedname_truncated_name_returns_decoding_error);
    RUN_TEST(test_binary_localizedtext_truncated_text_returns_decoding_error);
    return UNITY_END();
}
