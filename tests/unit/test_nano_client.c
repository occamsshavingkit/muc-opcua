/* tests/unit/test_nano_client.c */
#include "muc_opcua/client.h"
#include "unity.h"
#include <string.h>

typedef struct {
    opcua_statuscode_t connect_status;
    opcua_statuscode_t recv_status;
    size_t send_calls;
    size_t recv_calls;
    opcua_boolean_t closed;
} fake_transport_t;

static opcua_statuscode_t fake_connect(void *context, const char *endpoint_url, opcua_uint32_t timeout_ms) {
    fake_transport_t *fake = (fake_transport_t *)context;
    (void)timeout_ms;
    TEST_ASSERT_NOT_NULL(endpoint_url);
    return fake->connect_status;
}

static opcua_statuscode_t fake_send(void *context, const opcua_byte_t *buffer, size_t length, size_t *bytes_sent,
                                    opcua_uint32_t timeout_ms) {
    fake_transport_t *fake = (fake_transport_t *)context;
    (void)buffer;
    (void)timeout_ms;
    fake->send_calls++;
    *bytes_sent = length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t fake_recv(void *context, opcua_byte_t *buffer, size_t capacity, size_t *bytes_read,
                                    opcua_uint32_t timeout_ms) {
    fake_transport_t *fake = (fake_transport_t *)context;
    (void)timeout_ms;
    fake->recv_calls++;
    if (fake->recv_status != MU_STATUS_GOOD) {
        return fake->recv_status;
    }
    if (capacity > 0) {
        buffer[0] = 'A';
        *bytes_read = 1;
    } else {
        *bytes_read = 0;
    }
    return MU_STATUS_GOOD;
}

static void fake_close(void *context) {
    fake_transport_t *fake = (fake_transport_t *)context;
    fake->closed = true;
}

static opcua_byte_t send_buffer[256];
static opcua_byte_t receive_buffer[256];
static fake_transport_t fake;
static mu_client_transport_t transport;
static mu_client_t client;

void setUp(void) {
    (void)memset(&fake, 0, sizeof(fake));
    fake.connect_status = MU_STATUS_GOOD;
    fake.recv_status = MU_STATUS_GOOD;
    transport.context = &fake;
    transport.connect = fake_connect;
    transport.send = fake_send;
    transport.recv = fake_recv;
    transport.close = fake_close;

    mu_client_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.security_policy = MU_CLIENT_SECURITY_BASIC256SHA256;
    config.timeout_ms = 100;
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_init(&client, &config, &transport));
}

void tearDown(void) {}

static void connect_and_activate(void) {
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_connect(&client));
    TEST_ASSERT_EQUAL(MU_CLIENT_CHANNEL_OPEN, client.state);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_create_session(&client));
    TEST_ASSERT_EQUAL(MU_CLIENT_ACTIVATING, client.state);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_activate_session(&client, NULL));
    TEST_ASSERT_EQUAL(MU_CLIENT_ACTIVE, client.state);
}

void test_nano_client_connect_session_and_disconnect(void) {
    connect_and_activate();

    size_t endpoint_count = 1;
    mu_endpoint_description_t endpoint;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_get_endpoints(&client, &endpoint, &endpoint_count));
    TEST_ASSERT_EQUAL(1, endpoint_count);
    TEST_ASSERT_EQUAL_STRING("opc.tcp://localhost:4840", endpoint.endpoint_url);
    TEST_ASSERT_EQUAL(MU_CLIENT_SECURITY_BASIC256SHA256, endpoint.security_policy);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_close_session(&client));
    TEST_ASSERT_EQUAL(MU_CLIENT_CHANNEL_OPEN, client.state);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_disconnect(&client));
    TEST_ASSERT_EQUAL(MU_CLIENT_DISCONNECTED, client.state);
    TEST_ASSERT_TRUE(fake.closed);
}

void test_nano_client_security_policy_none_connects(void) {
    client.config.security_policy = MU_CLIENT_SECURITY_NONE;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_connect(&client));
    size_t endpoint_count = 1;
    mu_endpoint_description_t endpoint;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_get_endpoints(&client, &endpoint, &endpoint_count));
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#None", endpoint.security_policy_uri);
}

void test_nano_client_connection_loss_reports_status(void) {
    fake.recv_status = MU_STATUS_BAD_CONNECTIONCLOSED;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_CONNECTIONCLOSED, mu_client_connect(&client));
    TEST_ASSERT_EQUAL(MU_CLIENT_ERROR, client.state);
}

void test_nano_client_read_values_and_errors(void) {
    connect_and_activate();

    mu_nodeid_t nodes[3];
    (void)memset(nodes, 0, sizeof(nodes));
    nodes[0].identifier_type = MU_NODEID_NUMERIC;
    nodes[0].identifier.numeric = 2258;
    nodes[1].identifier_type = MU_NODEID_NUMERIC;
    nodes[1].identifier.numeric = 999999;
    nodes[2].identifier_type = MU_NODEID_NUMERIC;
    nodes[2].identifier.numeric = 85;

    mu_read_params_t params;
    (void)memset(&params, 0, sizeof(params));
    params.node_ids = nodes;
    params.num_nodes = 3;

    mu_read_value_t results[3];
    size_t result_count = 3;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_read(&client, &params, results, &result_count));
    TEST_ASSERT_EQUAL(3, result_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, results[0].status);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, results[0].value.type);
    TEST_ASSERT_EQUAL_UINT32(42, results[0].value.value.ui32);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NODEIDUNKNOWN, results[1].status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NODEIDINVALID, results[2].status);
}

void test_nano_client_browse_and_translate(void) {
    connect_and_activate();

    mu_browse_params_t params;
    (void)memset(&params, 0, sizeof(params));
    params.node_id.identifier_type = MU_NODEID_NUMERIC;
    params.node_id.identifier.numeric = 84;

    mu_browse_result_t result;
    size_t result_count = 1;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_browse(&client, &params, &result, &result_count));
    TEST_ASSERT_EQUAL(1, result_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result.status);
    TEST_ASSERT_EQUAL(1, result.reference_count);
    TEST_ASSERT_EQUAL_UINT32(85, result.references[0].node_id.identifier.numeric);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOCONTINUATIONPOINTS, mu_client_browse_next(&client, &result, &result_count));

    mu_browse_path_t path;
    (void)memset(&path, 0, sizeof(path));
    path.starting_node = params.node_id;
    path.relative_path_length = 1;
    mu_nodeid_t target;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_client_translate_browse_paths_to_node_ids(&client, &path, 1, &target));
    TEST_ASSERT_EQUAL_UINT32(85, target.identifier.numeric);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nano_client_connect_session_and_disconnect);
    RUN_TEST(test_nano_client_security_policy_none_connects);
    RUN_TEST(test_nano_client_connection_loss_reports_status);
    RUN_TEST(test_nano_client_read_values_and_errors);
    RUN_TEST(test_nano_client_browse_and_translate);
    return UNITY_END();
}
