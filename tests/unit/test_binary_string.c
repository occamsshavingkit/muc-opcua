/* tests/unit/test_binary_string.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* These tests assume binary encoding APIs that will be implemented in subsequent tasks */
void test_binary_string_roundtrip(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    mu_string_t empty_str = {0, NULL};
    mu_string_t null_str = {-1, NULL};
    mu_string_t valid_str = {4, (const opcua_byte_t *)"test"};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &empty_str));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &null_str));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &valid_str));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_string_t read_str;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(0, read_str.length);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(-1, read_str.length);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(4, read_str.length);
    TEST_ASSERT_EQUAL_MEMORY("test", read_str.data, 4);
}

void test_binary_string_over_limit(void) {
    opcua_byte_t buffer[128];
    mu_binary_writer_t writer;

    /* 4097-byte string exceeds the 4096-byte wire encoding limit */
    static opcua_byte_t long_str_data[4097] = {0};
    mu_string_t long_str = {4097, long_str_data};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, mu_binary_write_string(&writer, &long_str));
}

/* A string longer than 64 bytes but within the wire limit must round-trip:
   the 64-byte cap is for bounded String *Variable values*, not every wire string
   (e.g. the Hello EndpointUrl can be up to 4096 bytes, OPC 10000-6 7.1.2.3). */
void test_binary_string_above_value_limit_roundtrips(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;

    static opcua_byte_t data[100];
    for (int i = 0; i < 100; ++i) {
        data[i] = (opcua_byte_t)('A' + (i % 26));
    }
    mu_string_t str = {100, data};

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &str));

    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_string_t read_str;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &read_str));
    TEST_ASSERT_EQUAL(100, read_str.length);
    TEST_ASSERT_EQUAL_MEMORY(data, read_str.data, 100);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_string_roundtrip);
    RUN_TEST(test_binary_string_over_limit);
    RUN_TEST(test_binary_string_above_value_limit_roundtrips);
    return UNITY_END();
}
