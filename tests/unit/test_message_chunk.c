/* tests/unit/test_message_chunk.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/message_chunk.h"

void test_message_chunk_parser_valid_header(void) {
    opcua_byte_t buffer[128];
    mu_message_header_t header;
    header.message_type[0] = 'M';
    header.message_type[1] = 'S';
    header.message_type[2] = 'G';
    header.chunk_type = 'F';
    header.message_size = 12;
    header.secure_channel_id = 1;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_write_message_header(buffer, sizeof(buffer), &header));
    
    mu_message_header_t parsed;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_parse_message_header(buffer, 12, &parsed));
    TEST_ASSERT_EQUAL('M', parsed.message_type[0]);
    TEST_ASSERT_EQUAL('S', parsed.message_type[1]);
    TEST_ASSERT_EQUAL('G', parsed.message_type[2]);
    TEST_ASSERT_EQUAL('F', parsed.chunk_type);
    TEST_ASSERT_EQUAL(12, parsed.message_size);
    TEST_ASSERT_EQUAL(1, parsed.secure_channel_id);
}

void test_message_chunk_parser_invalid_message_type(void) {
    /* To be done in US4 */
}

void test_message_chunk_parser_invalid_size(void) {
    opcua_byte_t buffer[12];
    mu_message_header_t parsed;
    /* Buffer size smaller than message_size -> BAD_TCPMESSAGETOOLARGE */
    buffer[4] = 20; buffer[5] = 0; buffer[6] = 0; buffer[7] = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETOOLARGE, mu_parse_message_header(buffer, 12, &parsed));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_message_chunk_parser_valid_header);
    RUN_TEST(test_message_chunk_parser_invalid_message_type);
    RUN_TEST(test_message_chunk_parser_invalid_size);
    return UNITY_END();
}
