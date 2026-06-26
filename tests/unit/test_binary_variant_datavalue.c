/* tests/unit/test_binary_variant_datavalue.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_variant_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    
    mu_variant_t variant = { MU_TYPE_INT32, { .i32 = 42 } };
    
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &variant));
    
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
    memset(&datavalue, 0, sizeof(datavalue));
    datavalue.has_value = true;
    datavalue.value.type = MU_TYPE_INT32;
    datavalue.value.value.i32 = 42;
    datavalue.has_status = true;
    datavalue.status = MU_STATUS_GOOD;
    
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_datavalue(&writer, &datavalue));
    
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_datavalue_t read_datavalue;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_datavalue(&reader, &read_datavalue));
    
    TEST_ASSERT_TRUE(read_datavalue.has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, read_datavalue.value.type);
    TEST_ASSERT_EQUAL(42, read_datavalue.value.value.i32);
    TEST_ASSERT_TRUE(read_datavalue.has_status);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, read_datavalue.status);
}

void test_binary_variant_qualifiedname_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_variant_t variant;
    variant.type = MU_TYPE_QUALIFIEDNAME;
    variant.value.qualified_name.namespace_index = 1;
    variant.value.qualified_name.name = (mu_string_t){ 6, (const opcua_byte_t *)"MyVar1" };

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
    variant.type = MU_TYPE_LOCALIZEDTEXT;
    variant.value.localized_text.locale = (mu_string_t){ -1, NULL }; /* null locale -> absent */
    variant.value.localized_text.text = (mu_string_t){ 6, (const opcua_byte_t *)"MyVar1" };

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_variant_roundtrip);
    RUN_TEST(test_binary_datavalue_roundtrip);
    RUN_TEST(test_binary_variant_qualifiedname_roundtrip);
    RUN_TEST(test_binary_variant_localizedtext_roundtrip);
    return UNITY_END();
}
