/* tests/unit/test_key_credential.c
 *
 * spec 094 (CU 2113): KeyCredential Service Server Facet unit tests.
 * Validates that the four method handlers (GetEncryptingKey, CreateCredential,
 * UpdateCredential, DeleteCredential) correctly dispatch to the integrator's
 * mu_key_credential_adapter_t and return the right StatusCode when the
 * adapter is missing (Bad_NotSupported) or the backing store rejects the
 * operation (Bad_NoEntryExists / Bad_NoData).
 *
 * The handlers are exercised through the registered_methods[] table that
 * mu_key_credential_register() populates, mirroring how the real Call service
 * dispatch would invoke them.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/key_credential.h"
#include "muc_opcua/services/method.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE

#define MAX_MOCK_CREDENTIALS 4

typedef struct {
    char resource_uri[32];
    char key_id[32];
    opcua_byte_t blob[64];
    size_t blob_len;
    int in_use;
} mock_credential_t;

static mock_credential_t s_store[MAX_MOCK_CREDENTIALS];
static opcua_byte_t s_default_public_key[] = {0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
                                              0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
                                              0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00}; /* mock DER */
static const opcua_byte_t s_default_key_id[] = "test-key-id-1";

static void mock_store_reset(void) {
    (void)memset(s_store, 0, sizeof(s_store));
}

static mock_credential_t *mock_store_find(const mu_string_t *resource_uri, const mu_string_t *key_id) {
    for (size_t i = 0; i < MAX_MOCK_CREDENTIALS; ++i) {
        if (!s_store[i].in_use) {
            continue;
        }
        if (key_id != NULL) {
            if ((size_t)key_id->length != strlen(s_store[i].key_id) ||
                memcmp(key_id->data, s_store[i].key_id, (size_t)key_id->length) != 0) {
                continue;
            }
        }
        if ((size_t)resource_uri->length == strlen(s_store[i].resource_uri) &&
            memcmp(resource_uri->data, s_store[i].resource_uri, (size_t)resource_uri->length) == 0) {
            return &s_store[i];
        }
    }
    return NULL;
}

static opcua_statuscode_t mock_get_encrypting_key(void *ctx, const mu_string_t *resource_uri,
                                                  mu_bytestring_t *out_public_key, mu_string_t *out_key_id) {
    (void)ctx;
    (void)resource_uri;
    out_public_key->data = s_default_public_key;
    out_public_key->length = (opcua_int32_t)sizeof(s_default_public_key);
    out_key_id->data = s_default_key_id;
    out_key_id->length = (opcua_int32_t)(sizeof(s_default_key_id) - 1u);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_create_credential(void *ctx, const mu_string_t *resource_uri,
                                                 const mu_bytestring_t *credential, const mu_string_t *key_id) {
    (void)ctx;
    if (resource_uri == NULL || credential == NULL || key_id == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (credential->length < 0 || (size_t)credential->length > sizeof(s_store[0].blob)) {
        return MU_STATUS_BAD_RESOURCEUNAVAILABLE;
    }
    if (mock_store_find(resource_uri, key_id) != NULL) {
        return MU_STATUS_BAD_ENTRYEXISTS;
    }
    for (size_t i = 0; i < MAX_MOCK_CREDENTIALS; ++i) {
        if (!s_store[i].in_use) {
            size_t uri_len = (size_t)resource_uri->length;
            size_t key_len = (size_t)key_id->length;
            if (uri_len >= sizeof(s_store[i].resource_uri) || key_len >= sizeof(s_store[i].key_id)) {
                return MU_STATUS_BAD_RESOURCEUNAVAILABLE;
            }
            (void)memcpy(s_store[i].resource_uri, resource_uri->data, uri_len);
            s_store[i].resource_uri[uri_len] = '\0';
            (void)memcpy(s_store[i].key_id, key_id->data, key_len);
            s_store[i].key_id[key_len] = '\0';
            (void)memcpy(s_store[i].blob, credential->data, (size_t)credential->length);
            s_store[i].blob_len = (size_t)credential->length;
            s_store[i].in_use = 1;
            return MU_STATUS_GOOD;
        }
    }
    return MU_STATUS_BAD_RESOURCEUNAVAILABLE;
}

static opcua_statuscode_t mock_update_credential(void *ctx, const mu_string_t *resource_uri,
                                                 const mu_bytestring_t *credential, const mu_string_t *key_id) {
    (void)ctx;
    mock_credential_t *entry = mock_store_find(resource_uri, key_id);
    if (entry == NULL) {
        return MU_STATUS_BAD_NOENTRYEXISTS;
    }
    if (credential->length < 0 || (size_t)credential->length > sizeof(entry->blob)) {
        return MU_STATUS_BAD_RESOURCEUNAVAILABLE;
    }
    (void)memcpy(entry->blob, credential->data, (size_t)credential->length);
    entry->blob_len = (size_t)credential->length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_delete_credential(void *ctx, const mu_string_t *resource_uri,
                                                 const mu_string_t *key_id) {
    (void)ctx;
    mock_credential_t *entry = mock_store_find(resource_uri, key_id);
    if (entry == NULL) {
        return MU_STATUS_BAD_NOENTRYEXISTS;
    }
    entry->in_use = 0;
    return MU_STATUS_GOOD;
}

static const mu_key_credential_adapter_t s_mock_adapter = {mock_get_encrypting_key, mock_create_credential,
                                                           mock_update_credential, mock_delete_credential, NULL};

static opcua_byte_t s_send_buffer[8192];
static opcua_byte_t s_recv_buffer[8192];

static opcua_statuscode_t stub_entropy(void *ctx, opcua_byte_t *buf, size_t len) {
    (void)ctx;
    if (buf != NULL) {
        (void)memset(buf, 0xAA, len);
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_listen(void *ctx, const char *url) {
    (void)ctx;
    (void)url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_accept(void *ctx, void **handle) {
    (void)ctx;
    *handle = NULL;
    return MU_STATUS_GOOD;
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

static mu_server_t *s_server;
static _Alignas(8) opcua_byte_t s_server_storage[MU_SERVER_STORAGE_BYTES];

static opcua_statuscode_t init_test_server(const mu_key_credential_adapter_t *adapter) {
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.application_uri = "urn:test:keycred";
    config.application_name = "kc-test";
    config.receive_buffer = s_recv_buffer;
    config.receive_buffer_size = sizeof(s_recv_buffer);
    config.send_buffer = s_send_buffer;
    config.send_buffer_size = sizeof(s_send_buffer);
    config.max_sessions = 1u;
    config.max_secure_channels = 1u;
    config.max_chunk_count = 1u;
    config.max_message_size = (opcua_uint32_t)sizeof(s_recv_buffer);
    config.tcp_adapter.context = NULL;
    config.tcp_adapter.listen = stub_listen;
    config.tcp_adapter.accept = stub_accept;
    config.tcp_adapter.read = stub_read;
    config.tcp_adapter.write = stub_write;
    config.tcp_adapter.close_connection = stub_close;
    config.tcp_adapter.shutdown = stub_shutdown;
    config.time_adapter.context = NULL;
    config.time_adapter.get_time = stub_get_time;
    config.time_adapter.get_tick_ms = stub_get_tick_ms;
    config.entropy_adapter.context = NULL;
    config.entropy_adapter.generate_random = stub_entropy;
    config.key_credential_adapter = adapter;
    return mu_server_init(s_server_storage, sizeof(s_server_storage), &config, &s_server);
}

/* Look up a registered method by NodeId, returning its index. Returns
 * SIZE_MAX when not found. */
static size_t find_registered(mu_server_t *server, opcua_uint32_t numeric_id) {
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (server->registered_methods[i].method_id.namespace_index == 0u &&
            server->registered_methods[i].method_id.identifier_type == MU_NODEID_NUMERIC &&
            server->registered_methods[i].method_id.identifier.numeric == numeric_id) {
            return i;
        }
    }
    return (size_t)-1;
}

/* Invoke the registered callback for the given method NodeId. */
static opcua_statuscode_t invoke_registered(mu_server_t *server, opcua_uint32_t method_numeric,
                                            opcua_uint32_t object_numeric, const mu_variant_t *inputs,
                                            size_t input_count, mu_variant_t *outputs, size_t *output_count) {
    size_t idx = find_registered(server, method_numeric);
    if (idx == (size_t)-1) {
        return MU_STATUS_BAD_METHODINVALID;
    }
    mu_nodeid_t object_id = {0u, MU_NODEID_NUMERIC, {object_numeric}};
    mu_nodeid_t method_id = {0u, MU_NODEID_NUMERIC, {method_numeric}};
    return server->registered_methods[idx].callback(server, server->registered_methods[idx].context, &object_id,
                                                    &method_id, inputs, input_count, outputs, output_count);
}

static mu_variant_t make_string_variant(const char *s) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_STRING;
    v.value.str.length = (opcua_int32_t)strlen(s);
    v.value.str.data = (const opcua_byte_t *)s;
    return v;
}

static mu_variant_t make_bytestring_variant(const opcua_byte_t *data, size_t len) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_BYTESTRING;
    v.value.bytestr.length = (opcua_int32_t)len;
    v.value.bytestr.data = data;
    return v;
}

void test_key_credential_register_wires_four_methods(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(4, s_server->registered_method_count);
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_KEYCRED_GET_ENCRYPTING_KEY));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_KEYCRED_CREATE_CREDENTIAL));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_KEYCRED_UPDATE_CREDENTIAL));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_KEYCRED_DELETE_CREDENTIAL));
}

void test_get_encrypting_key_returns_valid_key(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t inputs[1] = {make_string_variant("opc.tcp://broker.example:8883")};
    mu_variant_t outputs[2];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 2u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_GET_ENCRYPTING_KEY, 17533u, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL_size_t(2, out_count);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, outputs[0].type);
    TEST_ASSERT_EQUAL((opcua_int32_t)sizeof(s_default_public_key), outputs[0].value.bytestr.length);
    TEST_ASSERT_EQUAL_MEMORY(s_default_public_key, outputs[0].value.bytestr.data, sizeof(s_default_public_key));
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, outputs[1].type);
    TEST_ASSERT_EQUAL((opcua_int32_t)(sizeof(s_default_key_id) - 1u), outputs[1].value.str.length);
    TEST_ASSERT_EQUAL_MEMORY(s_default_key_id, outputs[1].value.str.data, sizeof(s_default_key_id) - 1u);
}

void test_get_encrypting_key_without_adapter_returns_bad_notsupported(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));

    mu_variant_t inputs[1] = {make_string_variant("opc.tcp://broker.example:8883")};
    mu_variant_t outputs[2];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 2u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_GET_ENCRYPTING_KEY, 17533u, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, s);
}

void test_create_credential_stores_blob(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    const opcua_byte_t blob[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    mu_variant_t inputs[3] = {
        make_string_variant("opc.tcp://broker.example:8883"),
        make_bytestring_variant(blob, sizeof(blob)),
        make_string_variant("cred-1"),
    };
    mu_variant_t outputs[1];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_CREATE_CREDENTIAL, 17496u, inputs, 3u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL_size_t(0, out_count);
    TEST_ASSERT_EQUAL(1, s_store[0].in_use);
    TEST_ASSERT_EQUAL_STRING("opc.tcp://broker.example:8883", s_store[0].resource_uri);
    TEST_ASSERT_EQUAL_STRING("cred-1", s_store[0].key_id);
    TEST_ASSERT_EQUAL(sizeof(blob), s_store[0].blob_len);
    TEST_ASSERT_EQUAL_MEMORY(blob, s_store[0].blob, sizeof(blob));
}

void test_create_credential_without_adapter_returns_bad_notsupported(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));

    const opcua_byte_t blob[] = {0x01};
    mu_variant_t inputs[3] = {
        make_string_variant("opc.tcp://broker.example:8883"),
        make_bytestring_variant(blob, sizeof(blob)),
        make_string_variant("cred-1"),
    };
    mu_variant_t outputs[1];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_CREATE_CREDENTIAL, 17496u, inputs, 3u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, s);
}

void test_update_credential_replaces_blob(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Seed the store with CreateCredential first. */
    const opcua_byte_t seed[] = {0xAA};
    mu_variant_t create_inputs[3] = {make_string_variant("opc.tcp://broker:8883"),
                                     make_bytestring_variant(seed, sizeof(seed)), make_string_variant("k1")};
    mu_variant_t create_outs[1];
    size_t create_out_count = 0u;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, invoke_registered(s_server, MU_ID_KEYCRED_CREATE_CREDENTIAL, 17496u,
                                                        create_inputs, 3u, create_outs, &create_out_count));

    /* Now update. */
    const opcua_byte_t fresh[] = {0xBB, 0xCC, 0xDD};
    mu_variant_t update_inputs[3] = {make_string_variant("opc.tcp://broker:8883"),
                                     make_bytestring_variant(fresh, sizeof(fresh)), make_string_variant("k1")};
    mu_variant_t update_outs[1];
    size_t update_out_count = 0u;
    opcua_statuscode_t s = invoke_registered(s_server, MU_ID_KEYCRED_UPDATE_CREDENTIAL, 17496u, update_inputs, 3u,
                                             update_outs, &update_out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(sizeof(fresh), s_store[0].blob_len);
    TEST_ASSERT_EQUAL_MEMORY(fresh, s_store[0].blob, sizeof(fresh));
}

void test_update_credential_missing_returns_bad_noentryexists(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    const opcua_byte_t blob[] = {0x01};
    mu_variant_t inputs[3] = {make_string_variant("opc.tcp://broker:8883"), make_bytestring_variant(blob, sizeof(blob)),
                              make_string_variant("no-such-key")};
    mu_variant_t outs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_UPDATE_CREDENTIAL, 17496u, inputs, 3u, outs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOENTRYEXISTS, s);
}

void test_delete_credential_removes_entry(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Seed the store. */
    const opcua_byte_t seed[] = {0x11, 0x22};
    mu_variant_t create_inputs[3] = {make_string_variant("opc.tcp://broker:9999"),
                                     make_bytestring_variant(seed, sizeof(seed)), make_string_variant("del-key")};
    mu_variant_t create_outs[1];
    size_t create_out_count = 0u;
    (void)invoke_registered(s_server, MU_ID_KEYCRED_CREATE_CREDENTIAL, 17496u, create_inputs, 3u, create_outs,
                            &create_out_count);

    /* Delete it. */
    mu_variant_t delete_inputs[2] = {make_string_variant("opc.tcp://broker:9999"), make_string_variant("del-key")};
    mu_variant_t delete_outs[1];
    size_t delete_out_count = 0u;
    opcua_statuscode_t s = invoke_registered(s_server, MU_ID_KEYCRED_DELETE_CREDENTIAL, 17496u, delete_inputs, 2u,
                                             delete_outs, &delete_out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(0, s_store[0].in_use);
}

void test_delete_credential_missing_returns_bad_noentryexists(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t inputs[2] = {make_string_variant("opc.tcp://broker:9999"), make_string_variant("no-such-key")};
    mu_variant_t outs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_DELETE_CREDENTIAL, 17496u, inputs, 2u, outs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOENTRYEXISTS, s);
}

void test_get_encrypting_key_wrong_arg_type_returns_bad_invalidargument(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Pass a UInt32 where a String is expected. */
    mu_variant_t bad_input;
    (void)memset(&bad_input, 0, sizeof(bad_input));
    bad_input.type = MU_TYPE_UINT32;
    bad_input.value.ui32 = 42u;
    mu_variant_t outputs[2];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 2u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_GET_ENCRYPTING_KEY, 17533u, &bad_input, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INVALIDARGUMENT, s);
}

void test_create_credential_missing_args_returns_bad_argumentsmissing(void) {
    mock_store_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Only 1 input supplied (3 expected). */
    mu_variant_t inputs[1] = {make_string_variant("opc.tcp://broker:8883")};
    mu_variant_t outs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_KEYCRED_CREATE_CREDENTIAL, 17496u, inputs, 1u, outs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ARGUMENTSMISSING, s);
}

#else

void test_key_credential_requires_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE is disabled in this build");
}

#endif /* MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE */

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE
    RUN_TEST(test_key_credential_register_wires_four_methods);
    RUN_TEST(test_get_encrypting_key_returns_valid_key);
    RUN_TEST(test_get_encrypting_key_without_adapter_returns_bad_notsupported);
    RUN_TEST(test_create_credential_stores_blob);
    RUN_TEST(test_create_credential_without_adapter_returns_bad_notsupported);
    RUN_TEST(test_update_credential_replaces_blob);
    RUN_TEST(test_update_credential_missing_returns_bad_noentryexists);
    RUN_TEST(test_delete_credential_removes_entry);
    RUN_TEST(test_delete_credential_missing_returns_bad_noentryexists);
    RUN_TEST(test_get_encrypting_key_wrong_arg_type_returns_bad_invalidargument);
    RUN_TEST(test_create_credential_missing_args_returns_bad_argumentsmissing);
#else
    RUN_TEST(test_key_credential_requires_build);
#endif
    return UNITY_END();
}
