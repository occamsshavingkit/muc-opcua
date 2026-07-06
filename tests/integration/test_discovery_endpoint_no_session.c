/* tests/integration/test_discovery_endpoint_no_session.c
 * OPC-10000-4 §5.5.4: GetEndpoints and FindServers are usable on an open
 * SecureChannel without an activated Session. Drive mu_server_poll through
 * HEL → OPN → service requests with a mock transport. */
#include "../../src/core/server_internal.h"
#include "../../src/services/discovery.h"
#include "fake_platform.h"
#include "mock_transport.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Build a HEL chunk and enqueue it on the mock. */
static void enqueue_hel(mock_t *mock) {
    opcua_byte_t tmp[64];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    tmp[0] = 'H';
    tmp[1] = 'E';
    tmp[2] = 'L';
    tmp[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t url = {20, (const opcua_byte_t *)"opc.tcp://host:4840"};
    mu_binary_write_string(&w, &url);
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    mock_enqueue(mock, tmp, w.position);
}

/* Build an OPN (OpenSecureChannel, SecurityPolicy#None) chunk. */
static void enqueue_opn_none(mock_t *mock) {
    opcua_byte_t chunk[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    {
        mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
        mu_binary_write_string(&w, &pol);
    }
    mu_binary_write_int32(&w, -1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 3600000);
    {
        mu_binary_writer_t os;
        mu_binary_writer_init(&os, chunk, sizeof(chunk));
        os.position = 4;
        mu_binary_write_uint32(&os, (opcua_uint32_t)w.position);
    }
    mock_enqueue(mock, chunk, w.position);
}

/* Set up a server with mock transport, perform HEL→ACK→OPN handshake,
 * and return the server. The caller receives channel_id for request building. */
static mu_server_t *setup_server_with_channel(mock_t *mock, opcua_uint32_t *channel_id) {
    enqueue_hel(mock);
    enqueue_opn_none(mock);

    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    static opcua_byte_t rx[8192], tx[8192];
    config.receive_buffer = rx;
    config.receive_buffer_size = sizeof(rx);
    config.send_buffer = tx;
    config.send_buffer_size = sizeof(tx);
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL → ACK */
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock->last_write, 4);

    mu_server_poll(server); /* OPN → OpenSecureChannelResponse */
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock->last_write, 4);
    *channel_id = mock_read_opn_channel_id(mock->last_write, mock->last_write_len);
    mock_patch_msg_channel_ids(mock, *channel_id);

    return server;
}

/* Build a FindServersRequest body with optional server_uris filter.
 * If server_uris is NULL (length -1), no filter is applied.
 * OPC-10000-4 §5.5.2.2 FindServers request layout. */
static size_t build_find_servers_body(opcua_byte_t *buf, size_t cap, opcua_uint32_t req_handle,
                                      const mu_string_t *server_uris) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_FINDSERVERSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, 0, req_handle);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str); /* endpointUrl */
        mu_binary_write_int32(&w, 0);          /* localeIds[] */
    }
    /* serverUris[] filter (OPC-10000-4 §5.5.4.3) */
    if (server_uris == NULL || server_uris->length <= 0) {
        mu_binary_write_int32(&w, 0);
    } else {
        mu_binary_write_int32(&w, 1);
        mu_binary_write_string(&w, server_uris);
    }
    mu_binary_write_int32(&w, 0); /* serverTypes[] */
    return w.position;
}

/* Build a GetEndpointsRequest body.
 * OPC-10000-4 §5.5.4.2 GetEndpoints request layout. */
static size_t build_get_endpoints_body(opcua_byte_t *buf, size_t cap, opcua_uint32_t req_handle) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_GETENDPOINTSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, 0, req_handle);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str); /* endpointUrl */
        mu_binary_write_int32(&w, 0);          /* localeIds[] */
        mu_binary_write_int32(&w, 0);          /* profileUris[] */
    }
    return w.position;
}

/* OPC-10000-4 §5.5.4.2 GetEndpoints: return endpoints for the given endpoint_url
 * and profile_uris. Discovery services do not require a Session. */
void test_get_endpoints_without_session(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    opcua_uint32_t channel_id = 0;
    mu_server_t *server = setup_server_with_channel(&mock, &channel_id);

    opcua_byte_t body[256];
    size_t body_len = build_get_endpoints_body(body, sizeof(body), 2);

    opcua_byte_t chunk[512];
    size_t clen = mock_build_msg(chunk, sizeof(chunk), 2, 2, channel_id, 1, body, body_len);
    mock_enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* GetEndpoints → GetEndpointsResponse */
    mu_binary_reader_t body_reader;
    opcua_uint32_t resp_type = mock_parse_response(mock.last_write, mock.last_write_len, &body_reader);
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, resp_type);

    opcua_int32_t count;
    mu_binary_read_int32(&body_reader, &count);
    TEST_ASSERT_GREATER_THAN_INT32(0, count);

    /* Read first EndpointDescription and verify SecurityPolicy + TransportProfile.
     * OPC-10000-4 §5.5.4.2 EndpointDescription layout. */
    mu_string_t ignored;
    mu_binary_read_string(&body_reader, &ignored); /* endpointUrl */
    /* ApplicationDescription: applicationUri, productUri, applicationName */
    mu_string_t app_uri;
    mu_binary_read_string(&body_reader, &app_uri);
    mu_binary_read_string(&body_reader, &ignored); /* productUri */
    opcua_byte_t name_mask;
    mu_binary_read_byte(&body_reader, &name_mask);
    if (name_mask & 0x01)
        mu_binary_read_string(&body_reader, &ignored);
    if (name_mask & 0x02)
        mu_binary_read_string(&body_reader, &ignored);
    opcua_uint32_t app_type;
    mu_binary_read_uint32(&body_reader, &app_type);
    mu_binary_read_string(&body_reader, &ignored); /* gatewayServerUri */
    mu_binary_read_string(&body_reader, &ignored); /* discoveryProfileUri */
    /* discoveryUrls[] */
    opcua_int32_t disc_urls;
    mu_binary_read_int32(&body_reader, &disc_urls);
    for (opcua_int32_t i = 0; i < disc_urls; ++i)
        mu_binary_read_string(&body_reader, &ignored);
    /* serverCertificate (ByteString) */
    mu_bytestring_t cert;
    mu_binary_read_bytestring(&body_reader, &cert);
    /* securityMode */
    opcua_uint32_t sec_mode;
    mu_binary_read_uint32(&body_reader, &sec_mode);
    /* OPC-10000-4 §5.5.4.2: securityPolicyUri must be present */
    mu_string_t sec_pol;
    mu_binary_read_string(&body_reader, &sec_pol);
    TEST_ASSERT_GREATER_THAN_INT32(0, sec_pol.length);
    /* Skip user identity tokens */
    opcua_int32_t token_count;
    mu_binary_read_int32(&body_reader, &token_count);
    for (opcua_int32_t i = 0; i < token_count; ++i) {
        opcua_uint32_t tt;
        mu_binary_read_string(&body_reader, &ignored);
        mu_binary_read_uint32(&body_reader, &tt);
        mu_binary_read_string(&body_reader, &ignored);
        mu_binary_read_string(&body_reader, &ignored);
        mu_binary_read_string(&body_reader, &ignored);
    }
    /* OPC-10000-4 §5.5.4.2: transportProfileUri must be present */
    mu_string_t transport;
    mu_binary_read_string(&body_reader, &transport);
    TEST_ASSERT_GREATER_THAN_INT32(0, transport.length);

    (void)app_uri;
    (void)app_type;
    (void)sec_mode;
    (void)cert;
}

/* OPC-10000-4 §5.5.4.3 FindServers: find servers known to the Discovery
 * Server. Works on an open SecureChannel without a Session. */
void test_find_servers_without_session(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    opcua_uint32_t channel_id = 0;
    mu_server_t *server = setup_server_with_channel(&mock, &channel_id);

    opcua_byte_t body[256];
    size_t body_len = build_find_servers_body(body, sizeof(body), 2, NULL);

    opcua_byte_t chunk[512];
    size_t clen = mock_build_msg(chunk, sizeof(chunk), 2, 2, channel_id, 1, body, body_len);
    mock_enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* FindServers → FindServersResponse */
    mu_binary_reader_t body_reader;
    opcua_uint32_t resp_type = mock_parse_response(mock.last_write, mock.last_write_len, &body_reader);
    TEST_ASSERT_EQUAL(MU_ID_FINDSERVERSRESPONSE, resp_type);

    /* OPC-10000-4 §5.5.4.3: response contains at least one server */
    opcua_int32_t count;
    mu_binary_read_int32(&body_reader, &count);
    TEST_ASSERT_GREATER_THAN_INT32(0, count);

    /* Read first ApplicationDescription: applicationUri, productUri,
     * applicationName, applicationType. */
    mu_string_t app_uri;
    mu_binary_read_string(&body_reader, &app_uri);
    mu_string_t prod_uri;
    mu_binary_read_string(&body_reader, &prod_uri);
    /* ApplicationName (LocalizedText) */
    opcua_byte_t name_mask;
    mu_binary_read_byte(&body_reader, &name_mask);
    mu_string_t name_locale = {-1, NULL};
    mu_string_t name_text = {-1, NULL};
    if (name_mask & 0x01)
        mu_binary_read_string(&body_reader, &name_locale);
    if (name_mask & 0x02)
        mu_binary_read_string(&body_reader, &name_text);
    opcua_uint32_t app_type;
    mu_binary_read_uint32(&body_reader, &app_type);

    /* OPC-10000-4 §5.5.4.3: verify ApplicationName and ApplicationURI */
    TEST_ASSERT_GREATER_THAN_INT32(0, app_uri.length);
    TEST_ASSERT_GREATER_THAN_INT32(0, name_text.length);
    TEST_ASSERT_EQUAL(MU_APPLICATION_TYPE_SERVER, (mu_application_type_t)app_type);

    (void)prod_uri;
    (void)name_locale;
}

/* OPC-10000-4 §5.5.4.3: the server_uris filter in FindServersRequest limits
 * results to servers whose application_uri matches one of the listed URIs. */
void test_find_servers_filtered_by_application_uri(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    opcua_uint32_t channel_id = 0;
    mu_server_t *server = setup_server_with_channel(&mock, &channel_id);

    mu_string_t filter_uri = {8, (const opcua_byte_t *)"urn:test"};
    opcua_byte_t body[256];
    size_t body_len = build_find_servers_body(body, sizeof(body), 2, &filter_uri);

    opcua_byte_t chunk[512];
    size_t clen = mock_build_msg(chunk, sizeof(chunk), 2, 2, channel_id, 1, body, body_len);
    mock_enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* FindServers → FindServersResponse */
    mu_binary_reader_t body_reader;
    opcua_uint32_t resp_type = mock_parse_response(mock.last_write, mock.last_write_len, &body_reader);
    TEST_ASSERT_EQUAL(MU_ID_FINDSERVERSRESPONSE, resp_type);

    /* OPC-10000-4 §5.5.4.3: matching server(s) returned */
    opcua_int32_t count;
    mu_binary_read_int32(&body_reader, &count);
    TEST_ASSERT_GREATER_THAN_INT32(0, count);

    /* Read the application_uri and verify it matches the filter */
    mu_string_t app_uri;
    mu_binary_read_string(&body_reader, &app_uri);
    TEST_ASSERT_EQUAL_INT32(filter_uri.length, app_uri.length);
    TEST_ASSERT_EQUAL_MEMORY(filter_uri.data, app_uri.data, (size_t)filter_uri.length);
}

/* OPC-10000-4 §5.5.4: MU_DISCOVERY_MAX_ENDPOINTS is a compile-time bound
 * on the number of endpoints advertised via GetEndpoints. Verify the constant
 * is known and the server respects the bound at runtime. */
void test_discovery_max_endpoints_bound(void) {
    /* Compile-time assertion that the constant is defined and reasonable. */
    TEST_ASSERT_EQUAL_UINT32(5, MU_DISCOVERY_MAX_ENDPOINTS);

    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    opcua_uint32_t channel_id = 0;
    mu_server_t *server = setup_server_with_channel(&mock, &channel_id);

    opcua_byte_t body[256];
    size_t body_len = build_get_endpoints_body(body, sizeof(body), 2);

    opcua_byte_t chunk[512];
    size_t clen = mock_build_msg(chunk, sizeof(chunk), 2, 2, channel_id, 1, body, body_len);
    mock_enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* GetEndpoints → GetEndpointsResponse */
    mu_binary_reader_t body_reader;
    opcua_uint32_t resp_type = mock_parse_response(mock.last_write, mock.last_write_len, &body_reader);
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, resp_type);

    opcua_int32_t count;
    mu_binary_read_int32(&body_reader, &count);
    TEST_ASSERT_GREATER_THAN_INT32(0, count);
    /* OPC-10000-4 §5.5.4: endpoint count must not exceed the max */
    TEST_ASSERT_LESS_OR_EQUAL_INT32(MU_DISCOVERY_MAX_ENDPOINTS, count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_get_endpoints_without_session);
    RUN_TEST(test_find_servers_without_session);
    RUN_TEST(test_find_servers_filtered_by_application_uri);
    RUN_TEST(test_discovery_max_endpoints_bound);
    return UNITY_END();
}
