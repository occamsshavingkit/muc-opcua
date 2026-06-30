/* tests/unit/test_message_chunk.c */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/message_chunk.h"

void test_message_header_secure_conversation_parse_write_byte_equivalence(void) {
    /* OPC-10000-6 6.7.2.2 and 7.1.2.2: fixed MessageHeader bytes preserve
       type, finality, size, and channel id. */
    static const struct {
        opcua_byte_t bytes[12];
        opcua_uint32_t message_size;
        opcua_uint32_t secure_channel_id;
    } cases[] = {
        {{'O', 'P', 'N', 'F', 0x1C, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01}, 28u, 0x01020304u},
        {{'M', 'S', 'G', 'F', 0x20, 0x00, 0x00, 0x00, 0x0D, 0x0C, 0x0B, 0x0A}, 32u, 0x0A0B0C0Du},
        {{'C', 'L', 'O', 'F', 0x18, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11}, 24u, 0x11223344u},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mu_message_header_t parsed;
        opcua_byte_t input[32] = {0};
        opcua_byte_t encoded[12] = {0};

        TEST_ASSERT_TRUE(cases[i].message_size <= sizeof(input));
        for (size_t j = 0; j < sizeof(cases[i].bytes); ++j) {
            input[j] = cases[i].bytes[j];
        }

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_parse_message_header(input, cases[i].message_size, &parsed));
        TEST_ASSERT_EQUAL_UINT8(cases[i].bytes[0], parsed.message_type[0]);
        TEST_ASSERT_EQUAL_UINT8(cases[i].bytes[1], parsed.message_type[1]);
        TEST_ASSERT_EQUAL_UINT8(cases[i].bytes[2], parsed.message_type[2]);
        TEST_ASSERT_EQUAL_UINT8(cases[i].bytes[3], parsed.chunk_type);
        TEST_ASSERT_EQUAL_UINT32(cases[i].message_size, parsed.message_size);
        TEST_ASSERT_EQUAL_UINT32(cases[i].secure_channel_id, parsed.secure_channel_id);

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_write_message_header(encoded, sizeof(encoded), &parsed));
        TEST_ASSERT_EQUAL_UINT8_ARRAY(cases[i].bytes, encoded, sizeof(cases[i].bytes));
    }
}

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
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'F', 20, 0, 0, 0, 1, 0, 0, 0};
    mu_message_header_t parsed;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETOOLARGE, mu_parse_message_header(buffer, 12, &parsed));
}

#include "../../src/core/sequence.h"

void test_sequence_validation(void) {
    mu_sequence_validator_t validator;
    mu_sequence_validator_init(&validator);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 10));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 11));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURITYCHECKSFAILED, mu_sequence_validate(&validator, 11));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURITYCHECKSFAILED, mu_sequence_validate(&validator, 13));

    /* Valid again if we re-init? No, it just aborts. Let's re-init and test wrap around */
    mu_sequence_validator_init(&validator);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 4294966272U));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 4294966273U));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 1));

    /* OPC 10000-6 6.7.2.4: the first number after wrap shall be *less than 1024*,
       not necessarily exactly 1. A value in [0,1024) is accepted; >= 1024 is not. */
    mu_sequence_validator_init(&validator);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 4294967000U));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 500));

    mu_sequence_validator_init(&validator);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sequence_validate(&validator, 4294967000U));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURITYCHECKSFAILED, mu_sequence_validate(&validator, 2000));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_message_header_secure_conversation_parse_write_byte_equivalence);
    RUN_TEST(test_message_chunk_parser_valid_header);
    RUN_TEST(test_message_chunk_parser_invalid_message_type);
    RUN_TEST(test_message_chunk_parser_invalid_size);
    RUN_TEST(test_sequence_validation);
    return UNITY_END();
}
