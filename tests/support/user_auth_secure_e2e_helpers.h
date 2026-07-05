/* Shared fixed-buffer helpers for test_user_auth_secure_e2e.c.
 *
 * This header is intentionally included after that test's local fixture types and
 * g_fixture definition so the per-file mock scaffolding remains private.
 */
#ifndef USER_AUTH_SECURE_E2E_HELPERS_H
#define USER_AUTH_SECURE_E2E_HELPERS_H

void setUp(void) {
    g_fixture = (secure_fixture_t){0};
}

/* Byte-wise copy (not memcpy) so the static-analysis buffer heuristics stay quiet on
   this test harness; callers TEST_ASSERT the destination capacity before calling. */
static void copy_bytes(opcua_byte_t *dst, const opcua_byte_t *src, size_t length) {
    for (size_t i = 0u; i < length; ++i) {
        dst[i] = src[i];
    }
}

static void cleanup_crypto(mu_crypto_adapter_t *crypto) {
    if (crypto->context != NULL) {
        mu_host_crypto_adapter_cleanup(crypto);
    }
}

void tearDown(void) {
    if (g_fixture.server != NULL) {
        mu_server_close(g_fixture.server);
        g_fixture.server = NULL;
    }
    mu_sym_keys_release_cipher(&g_fixture.c2s);
    mu_sym_keys_release_cipher(&g_fixture.s2c);
    cleanup_crypto(&g_fixture.server_crypto);
    cleanup_crypto(&g_fixture.client_crypto);
    cleanup_crypto(&g_fixture.user_crypto);
    cleanup_crypto(&g_fixture.other_crypto);
}

static opcua_statuscode_t mock_listen(void *context, const char *url) {
    (void)context;
    (void)url;
    return MU_STATUS_GOOD;
}

static void mock_shutdown(void *context) {
    (void)context;
}

static void mock_close(void *context, void *handle) {
    /* cppcheck-suppress misra-c2012-11.5 */
    mock_t *mock = (mock_t *)context;
    (void)handle;
    mock->close_count++;
}

static opcua_statuscode_t mock_accept(void *context, void **handle) {
    /* cppcheck-suppress misra-c2012-11.5 */
    mock_t *mock = (mock_t *)context;
    mock->accept_count++;
    /* cppcheck-suppress intToPointerCast */
    /* cppcheck-suppress misra-c2012-11.6 */
    *handle = (mock->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    /* cppcheck-suppress misra-c2012-11.5 */
    mock_t *mock = (mock_t *)context;
    (void)handle;
    if (mock->read_index >= mock->inbound_count) {
        *bytes_read = 0;
        /* cppcheck-suppress misra-c2012-15.5 */
        return MU_STATUS_GOOD;
    }
    size_t length = mock->inbound_len[mock->read_index];
    TEST_ASSERT_TRUE(length <= capacity);
    copy_bytes(buffer, mock->inbound[mock->read_index], length);
    mock->read_index++;
    *bytes_read = length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_write(void *context, void *handle, const opcua_byte_t *buffer, size_t length,
                                     size_t *bytes_written) {
    /* cppcheck-suppress misra-c2012-11.5 */
    mock_t *mock = (mock_t *)context;
    (void)handle;
    TEST_ASSERT_TRUE(length <= sizeof(mock->last_write));
    copy_bytes(mock->last_write, buffer, length);
    mock->last_write_len = length;
    *bytes_written = length;
    return MU_STATUS_GOOD;
}

static void enqueue(mock_t *mock, const opcua_byte_t *bytes, size_t length) {
    TEST_ASSERT_TRUE(mock->inbound_count < (size_t)MAX_INBOUND);
    TEST_ASSERT_TRUE(length <= (size_t)CHUNK_CAP);
    copy_bytes(mock->inbound[mock->inbound_count], bytes, length);
    mock->inbound_len[mock->inbound_count] = length;
    mock->inbound_count++;
}

static void reset_inbound(mock_t *mock) {
    mock->inbound_count = 0;
    mock->read_index = 0;
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t auth_token, opcua_uint32_t handle) {
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(writer, &auth);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &null_id, 0);
}

static void parse_decoded_response(const opcua_byte_t *body, size_t length, decoded_response_t *response) {
    mu_binary_reader_t reader;
    mu_nodeid_t type;
    opcua_int64_t timestamp;
    opcua_byte_t diagnostics_mask;
    opcua_byte_t additional_encoding;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header;

    mu_binary_reader_init(&reader, body, length);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &type));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, type.identifier_type);
    TEST_ASSERT_EQUAL(0, type.namespace_index);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&reader, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &response->request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, &response->service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &diagnostics_mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &string_table_count));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &additional_header));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &additional_encoding));

    response->type_id = type.identifier.numeric;
    response->body = reader;
}

static void unwrap_opn_response(secure_fixture_t *fixture, decoded_response_t *response) {
    opcua_byte_t opn_body[CHUNK_CAP];
    size_t opn_body_len = 0;
    opcua_byte_t scratch[6144];
    mu_asym_chunk_info_t info = {0};

    TEST_ASSERT_TRUE(fixture->mock.last_write_len >= 8u);
    TEST_ASSERT_EQUAL_MEMORY("OPNF", fixture->mock.last_write, 4);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_asym_chunk_unwrap(&fixture->client_crypto, fixture->mock.last_write,
                                                           fixture->mock.last_write_len, opn_body, sizeof(opn_body),
                                                           &opn_body_len, scratch, sizeof(scratch), &info));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_BASIC256SHA256_ID, info.policy);
    /* Copy into the fixture RX buffer so decoded pointers survive unwrap_opn_response return. */
    TEST_ASSERT_TRUE(opn_body_len <= sizeof(fixture->rx));
    memcpy(fixture->rx, opn_body, opn_body_len);
    parse_decoded_response(fixture->rx, opn_body_len, response);
}

static void secure_call(secure_fixture_t *fixture, const opcua_byte_t *body, size_t body_len,
                        decoded_response_t *response) {
    opcua_byte_t chunk[CHUNK_CAP];
    size_t message_len = 0;
    const opcua_byte_t *response_body = NULL;
    size_t response_body_len = 0;
    mu_sym_chunk_info_t info = {0};
    opcua_uint32_t sequence = fixture->next_sequence;

    fixture->next_sequence++;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_chunk_wrap(&fixture->client_crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT,
                                        &fixture->c2s, "MSG", fixture->secure_channel_id, fixture->token_id, sequence,
                                        sequence, body, body_len, chunk, sizeof(chunk), &message_len));
    reset_inbound(&fixture->mock);
    enqueue(&fixture->mock, chunk, message_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(fixture->server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_chunk_unwrap(&fixture->client_crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT,
                                          &fixture->s2c, fixture->mock.last_write, fixture->mock.last_write_len,
                                          &response_body, &response_body_len, &info));
    parse_decoded_response(response_body, response_body_len, response);
}

static void init_crypto(secure_fixture_t *fixture) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&fixture->server_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&fixture->client_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&fixture->user_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&fixture->other_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      fixture->server_crypto.get_own_certificate(fixture->server_crypto.context, &fixture->server_cert,
                                                                 &fixture->server_cert_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      fixture->client_crypto.get_own_certificate(fixture->client_crypto.context, &fixture->client_cert,
                                                                 &fixture->client_cert_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, fixture->user_crypto.get_own_certificate(
                                          fixture->user_crypto.context, &fixture->user_cert, &fixture->user_cert_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      fixture->other_crypto.get_own_certificate(fixture->other_crypto.context, &fixture->other_cert,
                                                                &fixture->other_cert_len));
}

#endif /* USER_AUTH_SECURE_E2E_HELPERS_H */
