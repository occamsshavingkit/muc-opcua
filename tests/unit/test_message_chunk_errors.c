/* tests/unit/test_message_chunk_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "../../src/core/message_chunk.h"

void setUp(void) {}
void tearDown(void) {}

void test_message_chunk_too_small(void) {
    opcua_byte_t buffer[7] = {'H', 'E', 'L', 'F', 0, 0, 0};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPINTERNALERROR, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_too_large(void) {
    /* If the message size is extremely large, but length doesn't match, we return too large */
    /* Wait, the specification returns MU_STATUS_BAD_TCPMESSAGETOOLARGE if size > length in this implementation */
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'F', 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETOOLARGE, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_inconsistent_length(void) {
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'F', 16, 0, 0, 0, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETOOLARGE, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_invalid_message_type(void) {
    opcua_byte_t buffer[12] = {'X', 'Y', 'Z', 'F', 12, 0, 0, 0, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETYPEINVALID, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_invalid_chunk_type(void) {
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'X', 12, 0, 0, 0, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETYPEINVALID, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_message_chunk_too_small);
    RUN_TEST(test_message_chunk_too_large);
    RUN_TEST(test_message_chunk_inconsistent_length);
    RUN_TEST(test_message_chunk_invalid_message_type);
    RUN_TEST(test_message_chunk_invalid_chunk_type);
    return UNITY_END();
}
