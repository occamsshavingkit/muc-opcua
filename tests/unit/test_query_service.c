#include "unity.h"
#include "micro_opcua/config.h"
#include "micro_opcua/server.h"
#include "../../src/services/query.h"
#include "fake_platform.h"

void setUp(void) {}
void tearDown(void) {}

#ifdef MICRO_OPCUA_SERVICE_QUERY

static opcua_byte_t rx_buf[8192];
static opcua_byte_t tx_buf[8192];

void test_query_first_empty_address_space(void) {
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);
    mu_server_t *server = NULL;
    
    opcua_statuscode_t status = mu_server_init(storage, sizeof(storage), &config, &server);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    
    mu_query_first_request_t req = {0};
    mu_query_first_response_t resp = {0};
    mu_query_data_set_t data_sets[10];
    
    status = mu_query_first_process(server, &req, &resp, data_sets, 10);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_UINT32(0, resp.query_data_sets_count);
    TEST_ASSERT_NULL(resp.query_data_sets);
}

void test_query_next_release(void) {
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);
    mu_server_t *server = NULL;
    
    opcua_statuscode_t status = mu_server_init(storage, sizeof(storage), &config, &server);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    
    mu_query_next_request_t req = {0};
    req.release_continuation_point = true;
    
    mu_query_next_response_t resp = {0};
    mu_query_data_set_t data_sets[10];
    
    status = mu_query_next_process(server, &req, &resp, data_sets, 10);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_UINT32(0, resp.query_data_sets_count);
}

#endif /* MICRO_OPCUA_SERVICE_QUERY */

int main(void) {
    UNITY_BEGIN();
#ifdef MICRO_OPCUA_SERVICE_QUERY
    RUN_TEST(test_query_first_empty_address_space);
    RUN_TEST(test_query_next_release);
#endif
    return UNITY_END();
}
