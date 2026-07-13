#include "../../src/services/query.h"
#include "fake_platform.h"
#include "muc_opcua/config.h"
#include "muc_opcua/server.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_CU_QUERY

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

void test_query_next_invalid_continuation_point_returns_bad_continuation_point_invalid(void) {
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

    /* OPC-10000-4 section B.2.4 QueryNext uses section 7.9 ContinuationPoint handles. */
    /* OPC-10000-4 section 7.9: unknown continuation points are invalid. */
    static const opcua_byte_t invalid_continuation_point[] = {0xde, 0xad, 0xbe, 0xef};
    mu_query_next_request_t req = {0};
    req.release_continuation_point = false;
    req.continuation_point.length = (opcua_int32_t)sizeof(invalid_continuation_point);
    req.continuation_point.data = invalid_continuation_point;

    mu_query_next_response_t resp = {0};
    mu_query_data_set_t data_sets[10];

    status = mu_query_next_process(server, &req, &resp, data_sets, 10);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_CONTINUATIONPOINTINVALID, status);
}

void test_query_first_content_filter_element_count_overflow_returns_decoding_error(void) {
    opcua_byte_t body[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    mu_query_first_request_t req = {0};
    mu_content_filter_element_t filter_elements[2];
    mu_filter_operand_t filter_operands[1];
    mu_nodeid_t null_id = {0u, MU_NODEID_NUMERIC, {0u}};

    mu_binary_writer_init(&writer, body, sizeof(body));
    mu_binary_write_nodeid(&writer, &null_id);
    mu_binary_write_int64(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);

    /* OPC-10000-4 section B.2.3 QueryFirst contains the section 7.7.1 ContentFilter. */
    /* OPC-10000-4 section 7.7.1: elements is a counted array; truncated counts are malformed. */
    mu_binary_write_uint32(&writer, 2);
    mu_binary_write_uint32(&writer, MU_FILTEROPERATOR_EQUALS);
    mu_binary_write_uint32(&writer, 0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, writer.status);

    mu_binary_reader_init(&reader, body, writer.position);
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_DECODINGERROR,
        mu_query_first_request_decode(&reader, &req, NULL, 0, filter_elements, 2, filter_operands, 1));
}

void test_query_first_content_filter_exceeding_fixed_capacity_returns_too_many_operations(void) {
    opcua_byte_t body[64];
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    mu_query_first_request_t req = {0};
    mu_content_filter_element_t filter_elements[1];
    mu_filter_operand_t filter_operands[1];
    mu_nodeid_t null_id = {0u, MU_NODEID_NUMERIC, {0u}};

    mu_binary_writer_init(&writer, body, sizeof(body));
    mu_binary_write_nodeid(&writer, &null_id);
    mu_binary_write_int64(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);

    /* OPC-10000-4 section B.2.3 QueryFirst contains the section 7.7.1 ContentFilter. */
    /* OPC-10000-4 section 7.7.1: complete filter arrays can still exceed local fixed capacity. */
    mu_binary_write_uint32(&writer, 2);
    mu_binary_write_uint32(&writer, MU_FILTEROPERATOR_EQUALS);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, MU_FILTEROPERATOR_ISNULL);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, writer.status);

    mu_binary_reader_init(&reader, body, writer.position);
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_TOOMANYOPERATIONS,
        mu_query_first_request_decode(&reader, &req, NULL, 0, filter_elements, 1, filter_operands, 1));
}

#endif /* MUC_OPCUA_CU_QUERY */

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_CU_QUERY
    RUN_TEST(test_query_first_empty_address_space);
    RUN_TEST(test_query_next_release);
    RUN_TEST(test_query_next_invalid_continuation_point_returns_bad_continuation_point_invalid);
    RUN_TEST(test_query_first_content_filter_element_count_overflow_returns_decoding_error);
    RUN_TEST(test_query_first_content_filter_exceeding_fixed_capacity_returns_too_many_operations);
#endif
    return UNITY_END();
}
