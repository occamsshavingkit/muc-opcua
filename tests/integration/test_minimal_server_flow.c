/* tests/integration/test_minimal_server_flow.c
 * End-to-end: drive mu_server_poll through the minimal OPC UA server lifecycle:
 * HEL -> ACK -> OPN (OpenSecureChannel) -> GetEndpoints -> CreateSession ->
 * ActivateSession -> Read -> CloseSession -> server cleanup.
 *
 * Uses shared mock_transport.h helpers for transport simulation.
 *
 * OPC-10000-6 §7.1.2  OPC UA TCP transport
 * OPC-10000-4 §5.5.2  OpenSecureChannel
 * OPC-10000-4 §5.5.4  GetEndpoints
 * OPC-10000-4 §5.6.2  CreateSession
 * OPC-10000-4 §5.7.2  ActivateSession
 * OPC-10000-4 §5.11.2 Read
 * OPC-10000-4 §5.6.3.2 CloseSession
 */
#include "fake_platform.h"
#include "mock_transport.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* OPC-10000-4 §5.6.2 / §5.7.2 / §5.6.3.2 / §5.11.2 — full server lifecycle */
void test_minimal_server_lifecycle(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    /* ---- Build the inbound queue ---- */
    opcua_byte_t tmp[512];
    opcua_byte_t chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    /*
     * Step 1: HEL -> ACK handshake
     * OPC-10000-6 §7.1.2.3 Hello / §7.1.2.4 Acknowledge
     * The client sends a HELLO with protocol version 0, buffer sizes 8192.
     */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    tmp[0] = 'H';
    tmp[1] = 'E';
    tmp[2] = 'L';
    tmp[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);    /* chunk size placeholder */
    mu_binary_write_uint32(&w, 0);    /* ProtocolVersion */
    mu_binary_write_uint32(&w, 8192); /* ReceiveBufferSize */
    mu_binary_write_uint32(&w, 8192); /* SendBufferSize */
    mu_binary_write_uint32(&w, 0);    /* MaxMessageSize */
    mu_binary_write_uint32(&w, 0);    /* MaxChunkCount */
    {
        mu_string_t url = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &url);
    }
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    mock_enqueue(&mock, tmp, w.position);

    /*
     * Step 2: OPN (OpenSecureChannelRequest, SecurityPolicy#None)
     * OPC-10000-4 §5.5.2.2 OpenSecureChannel
     * OPC-10000-6 §7.1.2.4 OPN chunk framing
     */
    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0); /* size placeholder */
    mu_binary_write_uint32(&w, 0); /* SecureChannelId (unassigned) */
    {
        mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
        mu_binary_write_string(&w, &pol);
    }
    mu_binary_write_int32(&w, -1); /* SenderCertificate (null) */
    mu_binary_write_int32(&w, -1); /* ReceiverThumbprint (null) */
    mu_binary_write_uint32(&w, 1); /* SequenceNumber */
    mu_binary_write_uint32(&w, 1); /* RequestId */
    {
        mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
        mu_binary_write_nodeid(&w, &opn_type);
    }
    test_write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0);       /* ClientProtocolVersion */
    mu_binary_write_uint32(&w, 0);       /* RequestType */
    mu_binary_write_uint32(&w, 1);       /* SecurityMode NONE */
    mu_binary_write_int32(&w, -1);       /* ClientNonce (null) */
    mu_binary_write_uint32(&w, 3600000); /* RequestedLifetime (1 hour) */
    {
        mu_binary_writer_t os;
        mu_binary_writer_init(&os, chunk, sizeof(chunk));
        os.position = 4;
        mu_binary_write_uint32(&os, (opcua_uint32_t)w.position);
    }
    mock_enqueue(&mock, chunk, w.position);

    /*
     * Step 3: GetEndpoints (discovery service, no session required)
     * OPC-10000-4 §5.5.4.2 GetEndpoints
     */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_GETENDPOINTSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, 0, 2);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str); /* endpointUrl */
        mu_binary_write_int32(&w, 0);          /* localeIds */
        mu_binary_write_int32(&w, 0);          /* profileUris */
    }
    clen = mock_build_msg(chunk, sizeof(chunk), 2, 2, 1, 1, tmp, w.position);
    mock_enqueue(&mock, chunk, clen);

    /*
     * Step 4: CreateSession
     * OPC-10000-4 §5.6.2.2 CreateSession
     */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_create_session_body(&w, 3, 60000.0);
    clen = mock_build_msg(chunk, sizeof(chunk), 3, 3, 1, 1, tmp, w.position);
    mock_enqueue(&mock, chunk, clen);

    /*
     * Step 5: ActivateSession
     * OPC-10000-4 §5.7.2.2 ActivateSession
     */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};
        mu_binary_write_string(&w, &ns);
        mu_binary_write_bytestring(&w, &nb);
    }
    mu_binary_write_int32(&w, 0); /* ClientSoftwareCertificates */
    mu_binary_write_int32(&w, 0); /* LocaleIds */
    {
        mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY}};
        mu_binary_write_extension_object_header(&w, &anon, 0);
    }
    clen = mock_build_msg(chunk, sizeof(chunk), 4, 4, 1, 1, tmp, w.position);
    mock_enqueue(&mock, chunk, clen);

    /*
     * Step 6: Read ServerStatus node (i=2256, ns=0)
     * OPC-10000-4 §5.11.2.2 Read
     * OPC-10000-3 §5.6.2 — ServerStatus variable exists in the base address space.
     */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_double(&w, 0.0); /* maxAge */
    mu_binary_write_uint32(&w, 3);   /* TimestampsToReturn NEITHER */
    mu_binary_write_int32(&w, 1);    /* NodesToRead count = 1 */
    {
        mu_nodeid_t sv = {0, MU_NODEID_NUMERIC, {2256}}; /* Server_ServerStatus */
        mu_string_t ns = {-1, NULL};
        mu_binary_write_nodeid(&w, &sv);
        mu_binary_write_uint32(&w, 13); /* AttributeId Value */
        mu_binary_write_string(&w, &ns);
        mu_binary_write_uint16(&w, 0); /* DataEncoding QualifiedName */
        mu_binary_write_string(&w, &ns);
    }
    clen = mock_build_msg(chunk, sizeof(chunk), 5, 5, 1, 1, tmp, w.position);
    mock_enqueue(&mock, chunk, clen);

    /*
     * Step 7: CloseSession
     * OPC-10000-4 §5.6.3.2 CloseSession
     */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CLOSESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_boolean(&w, false); /* deleteSubscriptions */
    clen = mock_build_msg(chunk, sizeof(chunk), 6, 6, 1, 1, tmp, w.position);
    mock_enqueue(&mock, chunk, clen);

    /* ---- Configure the server ---- */
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
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;
    opcua_uint32_t channel_id;

    /*
     * --- Drive the lifecycle ---
     */

    /* OPC-10000-6 §7.1.2 — accept new connection */
    mu_server_poll(server);

    /* OPC-10000-6 §7.1.2.3 Hello → §7.1.2.4 Acknowledge */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.last_write, 4);

    /* OPC-10000-4 §5.5.2 OpenSecureChannel → OpenSecureChannelResponse */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.last_write, 4);
    channel_id = mock_read_opn_channel_id(mock.last_write, mock.last_write_len);
    mock_patch_msg_channel_ids(&mock, channel_id);

    /* OPC-10000-4 §5.5.4.2 GetEndpoints → GetEndpointsResponse */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, mock_parse_response(mock.last_write, mock.last_write_len, &body));

    /* OPC-10000-4 §5.6.2 CreateSession → CreateSessionResponse */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, mock_parse_response(mock.last_write, mock.last_write_len, &body));

    /* OPC-10000-4 §5.7.2 ActivateSession → ActivateSessionResponse */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, mock_parse_response(mock.last_write, mock.last_write_len, &body));

    /* OPC-10000-4 §5.11.2 Read → ReadResponse */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_ID_READRESPONSE, mock_parse_response(mock.last_write, mock.last_write_len, &body));

    /* OPC-10000-4 §5.6.3.2 CloseSession → CloseSessionResponse */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_ID_CLOSESESSIONRESPONSE, mock_parse_response(mock.last_write, mock.last_write_len, &body));

    /* Server cleanup */
    mu_server_close(server);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_minimal_server_lifecycle);
    return UNITY_END();
}
