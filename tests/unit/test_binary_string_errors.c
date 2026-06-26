/* tests/unit/test_binary_string_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

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
    RUN_TEST(test_binary_string_embedded_overrun);
    return UNITY_END();
}
