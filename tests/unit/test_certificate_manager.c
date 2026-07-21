/* tests/unit/test_certificate_manager.c
 *
 * spec 097 (CU 1631): Certificate Manager Pull Model unit tests.
 * Validates the four Pull Model method handlers dispatch correctly.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/certificate_manager.h"
#include "muc_opcua/services/method.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL

static opcua_byte_t s_recv_buffer[8192];
static opcua_byte_t s_send_buffer[8192];
static _Alignas(8) opcua_byte_t s_server_storage[MU_SERVER_STORAGE_BYTES];
static mu_server_t *s_server;

/* TCP adapter stubs (matching test_key_credential.c signatures) */
static opcua_statuscode_t stub_listen(void *ctx, const char *endpoint_url, opcua_uint16_t port, opcua_int32_t backlog) {
    (void)ctx;
    (void)endpoint_url;
    (void)port;
    (void)backlog;
    return 0;
}
static opcua_statuscode_t stub_accept(void *ctx, void **out_handle) {
    (void)ctx;
    (void)out_handle;
    return 0;
}
static opcua_statuscode_t stub_read(void *ctx, void *handle, opcua_byte_t *buf, size_t cap, size_t *out_len) {
    (void)ctx;
    (void)handle;
    (void)buf;
    (void)cap;
    *out_len = 0u;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t stub_write(void *ctx, void *handle, const opcua_byte_t *buf, size_t len, size_t *written) {
    (void)ctx;
    (void)handle;
    (void)buf;
    (void)len;
    *written = 0u;
    return MU_STATUS_GOOD;
}
static void stub_close(void *ctx, void *handle) {
    (void)ctx;
    (void)handle;
}
static void stub_shutdown(void *ctx) {
    (void)ctx;
}
static opcua_datetime_t stub_get_time(void *ctx) {
    (void)ctx;
    return 0;
}
static opcua_uint64_t stub_get_tick_ms(void *ctx) {
    (void)ctx;
    return 0u;
}
static opcua_statuscode_t stub_entropy(void *ctx, opcua_byte_t *buf, size_t len) {
    (void)ctx;
    if (buf != NULL)
        memset(buf, 0xAA, len);
    return MU_STATUS_GOOD;
}

static const opcua_byte_t s_test_csr[] = {0x30, 0x01, 0x02};
static const opcua_byte_t s_test_cert[] = {0x30, 0x01, 0x03};
static const opcua_byte_t s_test_key[] = {0x04, 0x01, 0x02};
static const opcua_byte_t s_test_issuers[] = {0x05, 0x02, 0x01};
static opcua_byte_t s_rejected_buf[64];
static size_t s_rejected_len;

/* Mock adapter callbacks */
static opcua_statuscode_t mock_start_signing(void *ctx, const mu_bytestring_t *csr, mu_nodeid_t group_id,
                                             uint32_t *out_request_id) {
    (void)ctx;
    (void)group_id;
    if (csr == NULL || csr->data == NULL || csr->length == 0)
        return MU_STATUS_BAD_INVALIDARGUMENT;
    *out_request_id = 42u;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_finish(void *ctx, uint32_t request_id, mu_bytestring_t *out_cert,
                                      mu_bytestring_t *out_key, mu_bytestring_t *out_issuers) {
    (void)ctx;
    if (request_id != 42u)
        return MU_STATUS_BAD_NOTFOUND;
    out_cert->data = s_test_cert;
    out_cert->length = (int)sizeof(s_test_cert);
    out_key->data = s_test_key;
    out_key->length = (int)sizeof(s_test_key);
    out_issuers->data = s_test_issuers;
    out_issuers->length = (int)sizeof(s_test_issuers);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_rejected(void *ctx, uint8_t *out_buffer, size_t *out_len) {
    (void)ctx;
    if (s_rejected_len > 0 && out_buffer != NULL && out_len != NULL && *out_len >= s_rejected_len)
        memcpy(out_buffer, s_rejected_buf, s_rejected_len);
    *out_len = s_rejected_len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_start_key_pair(void *ctx, const mu_bytestring_t *key_spec, mu_nodeid_t group_id,
                                              uint32_t *out_request_id) {
    (void)ctx;
    (void)group_id;
    if (key_spec == NULL || key_spec->data == NULL || key_spec->length == 0)
        return MU_STATUS_BAD_INVALIDARGUMENT;
    *out_request_id = 99u;
    return MU_STATUS_GOOD;
}

static const mu_certificate_manager_adapter_t s_mock_adapter = {
    .start_signing_request = mock_start_signing,
    .finish_request = mock_finish,
    .get_rejected_list = mock_rejected,
    .start_new_key_pair = mock_start_key_pair,
    .context = NULL,
};

static void reset_mock_rejected(void) {
    memset(s_rejected_buf, 0, sizeof(s_rejected_buf));
    s_rejected_len = 0;
}

static opcua_statuscode_t init_test_server(const mu_certificate_manager_adapter_t *adapter) {
    mu_server_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint_url = "opc.tcp://localhost:4840";
    cfg.application_uri = "urn:test:certmgr";
    cfg.application_name = "cm-test";
    cfg.receive_buffer = s_recv_buffer;
    cfg.receive_buffer_size = sizeof(s_recv_buffer);
    cfg.send_buffer = s_send_buffer;
    cfg.send_buffer_size = sizeof(s_send_buffer);
    cfg.max_sessions = 1u;
    cfg.max_secure_channels = 1u;
    cfg.max_chunk_count = 1u;
    cfg.max_message_size = (opcua_uint32_t)sizeof(s_recv_buffer);
    cfg.tcp_adapter.context = NULL;
    cfg.tcp_adapter.listen = stub_listen;
    cfg.tcp_adapter.accept = stub_accept;
    cfg.tcp_adapter.read = stub_read;
    cfg.tcp_adapter.write = stub_write;
    cfg.tcp_adapter.close_connection = stub_close;
    cfg.tcp_adapter.shutdown = stub_shutdown;
    cfg.time_adapter.context = NULL;
    cfg.time_adapter.get_time = stub_get_time;
    cfg.time_adapter.get_tick_ms = stub_get_tick_ms;
    cfg.entropy_adapter.context = NULL;
    cfg.entropy_adapter.generate_random = stub_entropy;
    cfg.certificate_manager_adapter = adapter;
    return mu_server_init(s_server_storage, sizeof(s_server_storage), &cfg, &s_server);
}

static size_t find_registered(mu_server_t *server, opcua_uint32_t numeric_id) {
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (server->registered_methods[i].method_id.namespace_index == 0u &&
            server->registered_methods[i].method_id.identifier_type == MU_NODEID_NUMERIC &&
            server->registered_methods[i].method_id.identifier.numeric == numeric_id)
            return i;
    }
    return (size_t)-1;
}

static opcua_statuscode_t invoke_registered(mu_server_t *server, opcua_uint32_t method_numeric,
                                            const mu_variant_t *inputs, size_t input_count, mu_variant_t *outputs,
                                            size_t *output_count) {
    size_t idx = find_registered(server, method_numeric);
    if (idx == (size_t)-1)
        return MU_STATUS_BAD_METHODINVALID;
    mu_nodeid_t object_id = {0u, MU_NODEID_NUMERIC, {15625u}};
    mu_nodeid_t method_id = {0u, MU_NODEID_NUMERIC, {method_numeric}};
    return server->registered_methods[idx].callback(server, server->registered_methods[idx].context, &object_id,
                                                    &method_id, inputs, input_count, outputs, output_count);
}

static mu_variant_t make_bytestring_variant(const opcua_byte_t *data, size_t len) {
    mu_variant_t v;
    memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_BYTESTRING;
    v.value.bytestr.length = (opcua_int32_t)len;
    v.value.bytestr.data = data;
    return v;
}

static mu_variant_t make_uint32_variant(uint32_t n) {
    mu_variant_t v;
    memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_UINT32;
    v.value.ui32 = n;
    return v;
}

/* --- US1: registration wiring --- */

void test_certificate_manager_register_wires_four_methods(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(4, s_server->registered_method_count);
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_CM_START_SIGNING_REQUEST));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_CM_FINISH_REQUEST));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_CM_GET_REJECTED_LIST));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_CM_START_NEW_KEY_PAIR_REQUEST));
}

/* --- US2: StartSigningRequest --- */

void test_start_signing_request_valid(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t inputs[] = {make_bytestring_variant(s_test_csr, sizeof(s_test_csr))};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_START_SIGNING_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, st);
    TEST_ASSERT_EQUAL(1, out_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, outputs[0].type);
    TEST_ASSERT_EQUAL(42u, outputs[0].value.ui32);
}

void test_start_signing_request_invalid(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t inputs[] = {make_bytestring_variant(NULL, 0)};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_START_SIGNING_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INVALIDARGUMENT, st);
}

void test_start_signing_request_no_adapter(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));
    mu_variant_t inputs[] = {make_bytestring_variant(s_test_csr, sizeof(s_test_csr))};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_START_SIGNING_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, st);
}

/* --- US2: FinishRequest --- */

void test_finish_request_found(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t inputs[] = {make_uint32_variant(42u)};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_FINISH_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, st);
    TEST_ASSERT_EQUAL(3, out_count);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, outputs[0].type);
}

void test_finish_request_not_found(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t inputs[] = {make_uint32_variant(999u)};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_FINISH_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTFOUND, st);
}

void test_finish_request_no_adapter(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));
    mu_variant_t inputs[] = {make_uint32_variant(1u)};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_FINISH_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, st);
}

/* --- US3: GetRejectedList --- */

void test_get_rejected_list_populated(void) {
    reset_mock_rejected();
    s_rejected_buf[0] = 0x01;
    s_rejected_buf[1] = 0x02;
    s_rejected_len = 2;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_GET_REJECTED_LIST, NULL, 0, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, st);
    TEST_ASSERT_EQUAL(1, out_count);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, outputs[0].type);
}

void test_get_rejected_list_empty(void) {
    reset_mock_rejected();
    s_rejected_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_GET_REJECTED_LIST, NULL, 0, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, st);
}

void test_get_rejected_list_no_adapter(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st = invoke_registered(s_server, MU_ID_CM_GET_REJECTED_LIST, NULL, 0, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, st);
}

/* --- US4: StartNewKeyPairRequest --- */

void test_start_new_key_pair_valid(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t inputs[] = {make_bytestring_variant(s_test_csr, sizeof(s_test_csr))};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st =
        invoke_registered(s_server, MU_ID_CM_START_NEW_KEY_PAIR_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, st);
    TEST_ASSERT_EQUAL(1, out_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, outputs[0].type);
    TEST_ASSERT_EQUAL(99u, outputs[0].value.ui32);
}

void test_start_new_key_pair_invalid(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    mu_variant_t inputs[] = {make_bytestring_variant(NULL, 0)};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st =
        invoke_registered(s_server, MU_ID_CM_START_NEW_KEY_PAIR_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INVALIDARGUMENT, st);
}

void test_start_new_key_pair_no_adapter(void) {
    reset_mock_rejected();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));
    mu_variant_t inputs[] = {make_bytestring_variant(s_test_csr, sizeof(s_test_csr))};
    mu_variant_t outputs[4];
    size_t out_count = 0;
    opcua_statuscode_t st =
        invoke_registered(s_server, MU_ID_CM_START_NEW_KEY_PAIR_REQUEST, inputs, 1, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, st);
}

void test_register_null_server(void) {
    opcua_statuscode_t st = mu_certificate_manager_register(NULL);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, st);
}

#else

void test_certificate_manager_skipped(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL is disabled in this build");
}

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL */

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL
    RUN_TEST(test_certificate_manager_register_wires_four_methods);
    RUN_TEST(test_start_signing_request_valid);
    RUN_TEST(test_start_signing_request_invalid);
    RUN_TEST(test_start_signing_request_no_adapter);
    RUN_TEST(test_finish_request_found);
    RUN_TEST(test_finish_request_not_found);
    RUN_TEST(test_finish_request_no_adapter);
    RUN_TEST(test_get_rejected_list_populated);
    RUN_TEST(test_get_rejected_list_empty);
    RUN_TEST(test_get_rejected_list_no_adapter);
    RUN_TEST(test_start_new_key_pair_valid);
    RUN_TEST(test_start_new_key_pair_invalid);
    RUN_TEST(test_start_new_key_pair_no_adapter);
    RUN_TEST(test_register_null_server);
#else
    RUN_TEST(test_certificate_manager_skipped);
#endif
    return UNITY_END();
}
