/* tests/unit/test_secure_channel.c */
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h"
#include "../../src/core/uasc.h"
#include "../../src/services/secure_channel.h"

opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy);

#define TEST_MAX_INBOUND 2u
#define TEST_MAX_WRITES 2u

typedef struct {
    int accept_count;
    opcua_byte_t inbound[TEST_MAX_INBOUND][512];
    size_t inbound_len[TEST_MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    int write_count;
    opcua_byte_t writes[TEST_MAX_WRITES][512];
    size_t write_len[TEST_MAX_WRITES];
} secure_channel_transport_t;

static opcua_statuscode_t test_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static void test_shutdown(void *context) {
    (void)context;
}

static void test_close(void *context, void *handle) {
    (void)context;
    (void)handle;
}

static opcua_statuscode_t test_accept(void *context, void **handle) {
    secure_channel_transport_t *transport = (secure_channel_transport_t *)context;
    transport->accept_count++;
    *handle = (transport->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    secure_channel_transport_t *transport = (secure_channel_transport_t *)context;
    (void)handle;
    if (transport->read_index >= transport->inbound_count) {
        *bytes_read = 0;
        return MU_STATUS_GOOD;
    }

    size_t len = transport->inbound_len[transport->read_index];
    TEST_ASSERT_TRUE(len <= capacity);
    (void)memcpy(buffer, transport->inbound[transport->read_index], len);
    transport->read_index++;
    *bytes_read = len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_write(void *context, void *handle, const opcua_byte_t *buffer, size_t len,
                                     size_t *bytes_written) {
    secure_channel_transport_t *transport = (secure_channel_transport_t *)context;
    (void)handle;
    TEST_ASSERT_TRUE(transport->write_count < (int)TEST_MAX_WRITES);
    TEST_ASSERT_TRUE(len <= sizeof(transport->writes[0]));
    (void)memcpy(transport->writes[transport->write_count], buffer, len);
    transport->write_len[transport->write_count] = len;
    transport->write_count++;
    *bytes_written = len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t failing_entropy(void *context, opcua_byte_t *buffer, size_t len) {
    (void)context;
    (void)buffer;
    (void)len;
    return MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static opcua_statuscode_t test_entropy(void *context, opcua_byte_t *buffer, size_t len) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x42, len);
    }
    return MU_STATUS_GOOD;
}

static mu_entropy_adapter_t channel_test_entropy_adapter(void) {
    mu_entropy_adapter_t adapter;
    adapter.context = NULL;
    adapter.generate_random = test_entropy;
    return adapter;
}

static void enqueue_request(secure_channel_transport_t *transport, const opcua_byte_t *bytes, size_t len) {
    TEST_ASSERT_TRUE(transport->inbound_count < TEST_MAX_INBOUND);
    TEST_ASSERT_TRUE(len <= sizeof(transport->inbound[0]));
    (void)memcpy(transport->inbound[transport->inbound_count], bytes, len);
    transport->inbound_len[transport->inbound_count] = len;
    transport->inbound_count++;
}

static void patch_message_size(opcua_byte_t *buffer, size_t len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, len);
    w.position = 4;
    (void)mu_binary_write_uint32(&w, (opcua_uint32_t)len);
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    (void)mu_binary_write_nodeid(w, &null_id);
    (void)mu_binary_write_int64(w, 0);
    (void)mu_binary_write_uint32(w, handle);
    (void)mu_binary_write_uint32(w, 0);
    (void)mu_binary_write_string(w, &null_str);
    (void)mu_binary_write_uint32(w, 0);
    (void)mu_binary_write_extension_object_header(w, &null_id, 0);
}

static size_t build_hello(opcua_byte_t *out, size_t capacity) {
    static const opcua_byte_t endpoint[] = "opc.tcp://host:4840";
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);

    out[0] = 'H';
    out[1] = 'E';
    out[2] = 'L';
    out[3] = 'F';
    w.position = 4;
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 8192);
    (void)mu_binary_write_uint32(&w, 8192);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    {
        mu_string_t url = {(opcua_int32_t)(sizeof(endpoint) - 1u), endpoint};
        (void)mu_binary_write_string(&w, &url);
    }
    patch_message_size(out, w.position);
    return w.position;
}

static size_t build_opn(opcua_byte_t *out, size_t capacity, const opcua_byte_t *policy_uri,
                        opcua_int32_t policy_uri_len, mu_message_security_mode_t security_mode,
                        opcua_uint32_t request_type) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);

    out[0] = 'O';
    out[1] = 'P';
    out[2] = 'N';
    out[3] = 'F';
    w.position = 4;
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    {
        mu_string_t policy = {policy_uri_len, policy_uri};
        (void)mu_binary_write_string(&w, &policy);
    }
    (void)mu_binary_write_int32(&w, -1);
    (void)mu_binary_write_int32(&w, -1);
    (void)mu_binary_write_uint32(&w, 1);
    (void)mu_binary_write_uint32(&w, 1);
    {
        mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
        (void)mu_binary_write_nodeid(&w, &opn_type);
    }
    write_request_header(&w, 77);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, request_type);
    (void)mu_binary_write_uint32(&w, (opcua_uint32_t)security_mode);
    (void)mu_binary_write_int32(&w, -1);
    (void)mu_binary_write_uint32(&w, 3600000);
    patch_message_size(out, w.position);
    return w.position;
}

static void configure_transport_server(mu_server_config_t *config, secure_channel_transport_t *transport,
                                       opcua_byte_t *rx, opcua_byte_t *tx) {
    (void)memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "urn:test";
    config->product_uri = "urn:test";
    config->application_name = "test";
    config->receive_buffer = rx;
    config->receive_buffer_size = 8192;
    config->send_buffer = tx;
    config->send_buffer_size = 8192;
    config->max_chunk_count = 1;
    config->max_message_size = 8192;
    config->max_sessions = 1;
    config->max_secure_channels = 1;
    fake_platform_init(NULL, &config->time_adapter, &config->entropy_adapter);
    config->tcp_adapter.context = transport;
    config->tcp_adapter.listen = test_listen;
    config->tcp_adapter.accept = test_accept;
    config->tcp_adapter.read = test_read;
    config->tcp_adapter.write = test_write;
    config->tcp_adapter.close_connection = test_close;
    config->tcp_adapter.shutdown = test_shutdown;
}

static opcua_statuscode_t read_opn_response_service_result(const opcua_byte_t *buffer, size_t len,
                                                           opcua_uint32_t *response_type,
                                                           opcua_statuscode_t *service_result) {
    mu_binary_reader_t r;
    mu_nodeid_t type;
    opcua_int64_t timestamp;
    opcua_uint32_t handle;
    opcua_statuscode_t status;

    mu_binary_reader_init(&r, buffer, len);
    r.position = MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE;

    status = mu_binary_read_nodeid(&r, &type);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    if (type.identifier_type != MU_NODEID_NUMERIC || type.namespace_index != 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    *response_type = type.identifier.numeric;

    status = mu_binary_read_int64(&r, &timestamp);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_uint32(&r, &handle);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    (void)timestamp;
    (void)handle;
    return mu_binary_read_statuscode(&r, service_result);
}

static void assert_opn_rejected(const opcua_byte_t *policy_uri, opcua_int32_t policy_uri_len,
                                mu_message_security_mode_t security_mode, opcua_uint32_t request_type,
                                opcua_statuscode_t expected_status) {
    secure_channel_transport_t transport;
    opcua_byte_t chunk[512];
    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;
    size_t len;
    opcua_uint32_t response_type;
    opcua_statuscode_t service_result;

    (void)memset(&transport, 0, sizeof(transport));

    len = build_hello(chunk, sizeof(chunk));
    enqueue_request(&transport, chunk, len);
    len = build_opn(chunk, sizeof(chunk), policy_uri, policy_uri_len, security_mode, request_type);
    enqueue_request(&transport, chunk, len);

    configure_transport_server(&config, &transport, rx, tx);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL_MEMORY("ACKF", transport.writes[0], 4);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(2, transport.write_count);
    TEST_ASSERT_EQUAL_MEMORY("OPNF", transport.writes[1], 4);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, read_opn_response_service_result(transport.writes[1], transport.write_len[1],
                                                                       &response_type, &service_result));
    TEST_ASSERT_TRUE(response_type == MU_ID_SERVICEFAULT || response_type == MU_ID_OPENSECURECHANNELRESPONSE);
    TEST_ASSERT_EQUAL_HEX32(expected_status, service_result);
    TEST_ASSERT_FALSE(server->secure_channel.is_open);
}

static void assert_opn_entropy_failure_returns_security_checks_failed(void) {
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
    secure_channel_transport_t transport;
    opcua_byte_t chunk[512];
    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;
    size_t len;
    opcua_uint32_t response_type;
    opcua_statuscode_t service_result;

    (void)memset(&transport, 0, sizeof(transport));

    len = build_hello(chunk, sizeof(chunk));
    enqueue_request(&transport, chunk, len);
    len = build_opn(chunk, sizeof(chunk), policy_uri, (opcua_int32_t)(sizeof(policy_uri) - 1u),
                    MU_MESSAGE_SECURITY_MODE_NONE, 0);
    enqueue_request(&transport, chunk, len);

    configure_transport_server(&config, &transport, rx, tx);
    config.entropy_adapter.generate_random = failing_entropy;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL_MEMORY("ACKF", transport.writes[0], 4);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(2, transport.write_count);
    TEST_ASSERT_EQUAL_MEMORY("OPNF", transport.writes[1], 4);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, read_opn_response_service_result(transport.writes[1], transport.write_len[1],
                                                                       &response_type, &service_result));
    TEST_ASSERT_TRUE(response_type == MU_ID_SERVICEFAULT || response_type == MU_ID_OPENSECURECHANNELRESPONSE);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SECURITYCHECKSFAILED, service_result);
    TEST_ASSERT_FALSE(server->secure_channel.is_open);
}

#if defined(MUC_OPCUA_MULTIPLE_CONNECTIONS) && MU_INTERN_MAX_CONNECTIONS > 1
static size_t build_opn_request_body(opcua_byte_t *out, size_t capacity, opcua_uint32_t request_handle) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);
    write_request_header(&w, request_handle);
    (void)mu_binary_write_uint32(&w, 0); /* clientProtocolVersion */
    (void)mu_binary_write_uint32(&w, 0); /* requestType = Issue */
    (void)mu_binary_write_uint32(&w, MU_MESSAGE_SECURITY_MODE_NONE);
    (void)mu_binary_write_int32(&w, -1); /* clientNonce */
    (void)mu_binary_write_uint32(&w, 3600000);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);
    return w.position;
}

void test_open_secure_channel_ids_are_unique_across_connections(void) {
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
    mu_string_t policy = {(opcua_int32_t)(sizeof(policy_uri) - 1u), policy_uri};
    mu_server_config_t config;
    secure_channel_transport_t transport;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    opcua_byte_t request[256];
    opcua_byte_t response[512];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;
    size_t request_len;
    size_t response_len;
    mu_binary_reader_t reader;
    mu_binary_writer_t writer;

    (void)memset(&transport, 0, sizeof(transport));
    configure_transport_server(&config, &transport, rx, tx);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));
    mu_service_dispatch_set_opn_security_policy(server, &policy);

    server->conns[0].client_handle = (void *)1;
    server->conns[1].client_handle = (void *)2;

    request_len = build_opn_request_body(request, sizeof(request), 1u);
    mu_binary_reader_init(&reader, request, request_len);
    mu_binary_writer_init(&writer, response, sizeof(response));
    response_len = sizeof(response);
    server->active_conn = &server->conns[0];
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            handle_open_secure_channel(server, &reader, &writer, &response_len));
    TEST_ASSERT_TRUE(server->conns[0].secure_channel.is_open);

    request_len = build_opn_request_body(request, sizeof(request), 2u);
    mu_binary_reader_init(&reader, request, request_len);
    mu_binary_writer_init(&writer, response, sizeof(response));
    response_len = sizeof(response);
    server->active_conn = &server->conns[1];
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            handle_open_secure_channel(server, &reader, &writer, &response_len));
    TEST_ASSERT_TRUE(server->conns[1].secure_channel.is_open);

    TEST_ASSERT_NOT_EQUAL_UINT32(server->conns[0].secure_channel.channel_id,
                                 server->conns[1].secure_channel.channel_id);
    server->active_conn = NULL;
}
#endif

void test_secure_channel_open_none(void) {
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);
    TEST_ASSERT_FALSE(channel.is_open);

    mu_entropy_adapter_t entropy = channel_test_entropy_adapter();
    opcua_uint32_t revised_lifetime = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_open(&channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 1000,
                                                             &entropy, &revised_lifetime));
    TEST_ASSERT_TRUE(channel.is_open);
    TEST_ASSERT_EQUAL(10000, revised_lifetime);      /* Bounded to min */
    TEST_ASSERT_GREATER_THAN(0, channel.channel_id); /* non-zero random id */
    TEST_ASSERT_EQUAL(1, channel.token_id);

    /* Renew keeps the channel id; only the token rolls. */
    opcua_uint32_t saved_channel_id = channel.channel_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_open(&channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 5000000,
                                                             &entropy, &revised_lifetime));
    TEST_ASSERT_EQUAL(3600000, revised_lifetime); /* Bounded to max */
    TEST_ASSERT_EQUAL(saved_channel_id, channel.channel_id);
    TEST_ASSERT_EQUAL(2, channel.token_id);
}

void test_secure_channel_open_rejects_when_entropy_unavailable(void) {
    /* OPC-10000-6 section 6.7.2.2 requires a restart-unique SecureChannelId; the
       id is drawn from the entropy adapter, so a missing or failing adapter must
       reject the open (fail closed) rather than fall back to a predictable id. */
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);

    mu_entropy_adapter_t broken = {NULL, failing_entropy};
    opcua_uint32_t revised_lifetime = 0;
    TEST_ASSERT_EQUAL(
        MU_STATUS_BAD_SECURITYCHECKSFAILED,
        mu_secure_channel_open(&channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 1000, &broken, &revised_lifetime));
    TEST_ASSERT_FALSE(channel.is_open);

    TEST_ASSERT_EQUAL(
        MU_STATUS_BAD_SECURITYCHECKSFAILED,
        mu_secure_channel_open(&channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 1000, NULL, &revised_lifetime));
    TEST_ASSERT_FALSE(channel.is_open);
}

void test_secure_channel_close(void) {
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN, mu_secure_channel_close(&channel));

    mu_entropy_adapter_t entropy = channel_test_entropy_adapter();
    opcua_uint32_t revised_lifetime = 0;
    mu_secure_channel_open(&channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 3600000, &entropy, &revised_lifetime);
    TEST_ASSERT_TRUE(channel.is_open);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_close(&channel));
    TEST_ASSERT_FALSE(channel.is_open);
}

#ifdef MUC_OPCUA_SECURITY
static void populate_derived_session_keys(mu_secure_channel_t *channel) {
    (void)memset(&channel->client_keys, 0xA5, sizeof(channel->client_keys));
    (void)memset(&channel->server_keys, 0x5A, sizeof(channel->server_keys));
    channel->client_keys.cipher_ctx_valid = false;
    channel->client_keys.crypto = NULL;
    channel->server_keys.cipher_ctx_valid = false;
    channel->server_keys.crypto = NULL;
    channel->keys_valid = true;
}

static void assert_symmetric_keys_zeroized(const mu_sym_keys_t *keys) {
    const opcua_byte_t zeros[sizeof(*keys)] = {0};
    TEST_ASSERT_EQUAL_MEMORY(zeros, keys, sizeof(*keys));
}

static void assert_channel_keys_zeroized(const mu_secure_channel_t *channel) {
    TEST_ASSERT_FALSE(channel->keys_valid);
    assert_symmetric_keys_zeroized(&channel->client_keys);
    assert_symmetric_keys_zeroized(&channel->server_keys);
}

void test_secure_channel_close_and_init_zeroize_derived_session_keys(void) {
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";
    mu_secure_channel_t channel;
    mu_string_t policy = {(opcua_int32_t)(sizeof(policy_uri) - 1u), policy_uri};
    opcua_uint32_t revised_lifetime = 0;

    mu_secure_channel_init(&channel);
    channel.policy = MU_SECURITY_POLICY_BASIC256SHA256_ID;
    mu_entropy_adapter_t entropy = channel_test_entropy_adapter();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_secure_channel_open(&channel, &policy, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, 3600000,
                                             &entropy, &revised_lifetime));
    populate_derived_session_keys(&channel);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_close(&channel));
    assert_channel_keys_zeroized(&channel);

    populate_derived_session_keys(&channel);
    mu_secure_channel_init(&channel);
    assert_channel_keys_zeroized(&channel);
}
#endif

void test_opn_rejects_unacceptable_security_policy_without_crypto_adapter(void) {
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";

    /* OPC-10000-4 §5.6.2.2 defines securityPolicyUri as the OPN policy
       parameter; §5.6.2.3 defines Bad_SecurityPolicyRejected for a policy that
       does not meet the server requirements. This server has no crypto adapter,
       so its active SecureChannel capability is SecurityPolicy None. */
    assert_opn_rejected(policy_uri, (opcua_int32_t)(sizeof(policy_uri) - 1u), MU_MESSAGE_SECURITY_MODE_NONE, 0,
                        MU_STATUS_BAD_SECURITYPOLICYREJECTED);
}

void test_opn_rejects_signing_modes_for_security_policy_none(void) {
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
    static const mu_message_security_mode_t modes[] = {MU_MESSAGE_SECURITY_MODE_SIGN,
                                                       MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT};

    /* OPC-10000-4 §5.6.2.2 defines securityMode as the OPN mode parameter;
       §5.6.2.3 defines Bad_SecurityModeRejected for a mode that does not meet
       the server requirements. With SecurityPolicy None / no crypto adapter,
       Sign and SignAndEncrypt are inconsistent with the active capability. */
    for (size_t i = 0; i < (sizeof(modes) / sizeof(modes[0])); ++i) {
        assert_opn_rejected(policy_uri, (opcua_int32_t)(sizeof(policy_uri) - 1u), modes[i], 0,
                            MU_STATUS_BAD_SECURITYMODEREJECTED);
    }
}

void test_opn_entropy_failure_returns_bad_security_checks_failed(void) {
    /* OPC-10000-4 section 5.6.2.3 requires OpenSecureChannel security check
       failures to reject the request; section 7.38.2 defines
       Bad_SecurityChecksFailed as 0x80130000. */
    assert_opn_entropy_failure_returns_security_checks_failed();
}

void test_opn_rejects_invalid_request_type(void) {
    /* OPC-10000-4 §5.6.2.3 Table 12: requestType shall be ISSUE (0) or
       RENEW (1). Any other value is rejected with Bad_RequestTypeInvalid
       (0x803C0000). Uses SecurityPolicy None so no crypto adapter is needed. */
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
    assert_opn_rejected(policy_uri, (opcua_int32_t)(sizeof(policy_uri) - 1u), MU_MESSAGE_SECURITY_MODE_NONE, 2,
                        MU_STATUS_BAD_REQUESTTYPEINVALID);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_secure_channel_open_none);
    RUN_TEST(test_secure_channel_open_rejects_when_entropy_unavailable);
    RUN_TEST(test_secure_channel_close);
#ifdef MUC_OPCUA_SECURITY
    RUN_TEST(test_secure_channel_close_and_init_zeroize_derived_session_keys);
#endif
    RUN_TEST(test_opn_rejects_unacceptable_security_policy_without_crypto_adapter);
    RUN_TEST(test_opn_rejects_signing_modes_for_security_policy_none);
    RUN_TEST(test_opn_entropy_failure_returns_bad_security_checks_failed);
    RUN_TEST(test_opn_rejects_invalid_request_type);
#if defined(MUC_OPCUA_MULTIPLE_CONNECTIONS) && MU_INTERN_MAX_CONNECTIONS > 1
    RUN_TEST(test_open_secure_channel_ids_are_unique_across_connections);
#endif
    return UNITY_END();
}
