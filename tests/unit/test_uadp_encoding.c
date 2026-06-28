#include "unity.h"
#include "micro_opcua/pubsub.h"
#include "micro_opcua/status.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

void test_uadp_network_message_encoding(void) {
    mu_pubsub_connection_t conn = {
        .port = 4840,
        .enabled = true,
        .publisher_id = 0x12345678
    };
    mu_pubsub_writer_group_t wg = {
        .writer_group_id = 1,
        .publishing_interval_ms = 1000,
        .dataset_writer = { .data_set_writer_id = 1 }
    };

    opcua_byte_t buffer[64];
    size_t bytes_written = 0;

    opcua_statuscode_t status = mu_encode_uadp_network_message(&conn, &wg, buffer, sizeof(buffer), &bytes_written);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    
    // Expected based on fixture:
    // Flags1 = 0x51
    // ExtendedFlags1 = 0x20
    // PublisherId = 0x78, 0x56, 0x34, 0x12 (little endian UInt32)
    // Payload = 0x01, 0x02, 0x03
    
    TEST_ASSERT_EQUAL_UINT32(9, bytes_written);
    TEST_ASSERT_EQUAL_HEX8(0x51, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x20, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x78, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x56, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x34, buffer[4]);
    TEST_ASSERT_EQUAL_HEX8(0x12, buffer[5]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[6]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[7]);
    TEST_ASSERT_EQUAL_HEX8(0x03, buffer[8]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uadp_network_message_encoding);
    return UNITY_END();
}
