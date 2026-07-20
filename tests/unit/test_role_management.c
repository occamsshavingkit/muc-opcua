/* tests/unit/test_role_management.c
 *
 * spec 095 (CU 2080): User Role Management Server Facet unit tests.
 * Validates that the four method handlers (AddRole, RemoveRole, AddIdentity,
 * RemoveIdentity) correctly dispatch to the integrator's
 * mu_role_management_adapter_t and return the right StatusCode when the
 * adapter is missing (Bad_NotSupported).
 *
 * The handlers are exercised through the registered_methods[] table that
 * mu_role_management_register() populates, mirroring how the real Call service
 * dispatch would invoke them.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/method.h"
#include "muc_opcua/services/role_management.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_CU_USER_ROLE_MANAGEMENT

#define MAX_MOCK_ROLES 4

typedef struct {
    opcua_uint32_t namespace;
    opcua_uint32_t identifier;
    char name[32];
    int in_use;
} mock_role_t;

static mock_role_t s_roles[MAX_MOCK_ROLES];
static opcua_uint32_t s_next_role_id;

static void mock_reset(void) {
    (void)memset(s_roles, 0, sizeof(s_roles));
    s_next_role_id = 1u;
}

static const opcua_uint32_t s_test_role_id_ns = 0u;

static opcua_statuscode_t mock_add_role(void *ctx, const mu_nodeid_t *role_id, const mu_string_t *role_name) {
    (void)ctx;
    if (role_id == NULL || role_name == NULL || role_name->length <= 0) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < MAX_MOCK_ROLES; ++i) {
        if (s_roles[i].in_use) {
            size_t name_len = (size_t)role_name->length;
            if (name_len == strlen(s_roles[i].name) && memcmp(role_name->data, s_roles[i].name, name_len) == 0) {
                return MU_STATUS_BAD_ENTRYEXISTS;
            }
        }
    }
    for (size_t i = 0; i < MAX_MOCK_ROLES; ++i) {
        if (!s_roles[i].in_use) {
            opcua_uint32_t id = s_next_role_id++;
            s_roles[i].namespace = s_test_role_id_ns;
            s_roles[i].identifier = id;
            size_t name_len = (size_t)role_name->length;
            if (name_len >= sizeof(s_roles[i].name)) {
                return MU_STATUS_BAD_RESOURCEUNAVAILABLE;
            }
            (void)memcpy(s_roles[i].name, role_name->data, name_len);
            s_roles[i].name[name_len] = '\0';
            s_roles[i].in_use = 1;
            /* Write back the assigned NodeId through the mutable struct. */
            ((mu_nodeid_t *)role_id)->namespace_index = s_test_role_id_ns;
            ((mu_nodeid_t *)role_id)->identifier_type = MU_NODEID_NUMERIC;
            ((mu_nodeid_t *)role_id)->identifier.numeric = id;
            return MU_STATUS_GOOD;
        }
    }
    return MU_STATUS_BAD_RESOURCEUNAVAILABLE;
}

static opcua_statuscode_t mock_remove_role(void *ctx, const mu_nodeid_t *role_id) {
    (void)ctx;
    if (role_id == NULL || role_id->identifier_type != MU_NODEID_NUMERIC) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < MAX_MOCK_ROLES; ++i) {
        if (s_roles[i].in_use && s_roles[i].identifier == role_id->identifier.numeric &&
            s_roles[i].namespace == role_id->namespace_index) {
            s_roles[i].in_use = 0;
            return MU_STATUS_GOOD;
        }
    }
    return MU_STATUS_BAD_NOENTRYEXISTS;
}

static opcua_statuscode_t mock_add_identity(void *ctx, const mu_nodeid_t *role_id, const mu_string_t *identity) {
    (void)ctx;
    (void)identity;
    if (role_id == NULL || role_id->identifier_type != MU_NODEID_NUMERIC) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < MAX_MOCK_ROLES; ++i) {
        if (s_roles[i].in_use && s_roles[i].identifier == role_id->identifier.numeric &&
            s_roles[i].namespace == role_id->namespace_index) {
            return MU_STATUS_GOOD;
        }
    }
    return MU_STATUS_BAD_NOENTRYEXISTS;
}

static opcua_statuscode_t mock_remove_identity(void *ctx, const mu_nodeid_t *role_id, const mu_string_t *identity) {
    (void)ctx;
    (void)identity;
    if (role_id == NULL || role_id->identifier_type != MU_NODEID_NUMERIC) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < MAX_MOCK_ROLES; ++i) {
        if (s_roles[i].in_use && s_roles[i].identifier == role_id->identifier.numeric &&
            s_roles[i].namespace == role_id->namespace_index) {
            return MU_STATUS_GOOD;
        }
    }
    return MU_STATUS_BAD_NOENTRYEXISTS;
}

static const mu_role_management_adapter_t s_mock_adapter = {mock_add_role, mock_remove_role, mock_add_identity,
                                                            mock_remove_identity, NULL};

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

static opcua_statuscode_t init_test_server(const mu_role_management_adapter_t *adapter) {
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.application_uri = "urn:test:rolemgmt";
    config.application_name = "rm-test";
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
    config.role_management_adapter = adapter;
    return mu_server_init(s_server_storage, sizeof(s_server_storage), &config, &s_server);
}

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

static mu_variant_t make_nodeid_variant(opcua_uint16_t ns, opcua_uint32_t numeric) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_NODEID;
    v.value.nodeid.namespace_index = ns;
    v.value.nodeid.identifier_type = MU_NODEID_NUMERIC;
    v.value.nodeid.identifier.numeric = numeric;
    return v;
}

/* --- Register --- */

void test_role_management_register_wires_four_methods(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(4, s_server->registered_method_count);
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_ROLE_ADD_ROLE));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_ROLE_REMOVE_ROLE));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_ROLE_ADD_IDENTITY));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, find_registered(s_server, MU_ID_ROLE_REMOVE_IDENTITY));
}

/* --- AddRole --- */

void test_add_role_creates_role(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t inputs[2] = {make_string_variant("Operator"), make_string_variant("urn:test")};
    mu_variant_t outputs[1];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 1u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, inputs, 2u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL_size_t(1, out_count);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, outputs[0].type);
    TEST_ASSERT_EQUAL(1u, outputs[0].value.nodeid.identifier.numeric);
    TEST_ASSERT_EQUAL(1, s_roles[0].in_use);
    TEST_ASSERT_EQUAL_STRING("Operator", s_roles[0].name);
}

void test_add_role_without_adapter_returns_bad_notsupported(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));

    mu_variant_t inputs[2] = {make_string_variant("Operator"), make_string_variant("urn:test")};
    mu_variant_t outputs[1];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 1u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, inputs, 2u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, s);
}

/* --- RemoveRole --- */

void test_remove_role_deletes_role(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Seed with AddRole. */
    mu_variant_t add_inputs[2] = {make_string_variant("Operator"), make_string_variant("urn:test")};
    mu_variant_t add_outputs[1];
    size_t add_out = 1u;
    (void)invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, add_inputs, 2u, add_outputs, &add_out);
    TEST_ASSERT_EQUAL(1, s_roles[0].in_use);

    /* Remove it. */
    mu_variant_t remove_inputs[1] = {make_nodeid_variant(0u, 1u)};
    mu_variant_t remove_outputs[1];
    size_t remove_out = 0u;
    opcua_statuscode_t s = invoke_registered(s_server, MU_ID_ROLE_REMOVE_ROLE, MU_ID_ROLE_SET_TYPE, remove_inputs, 1u,
                                             remove_outputs, &remove_out);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(0, s_roles[0].in_use);
}

void test_remove_role_missing_returns_bad_noentryexists(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t inputs[1] = {make_nodeid_variant(0u, 99u)};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_REMOVE_ROLE, MU_ID_ROLE_SET_TYPE, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOENTRYEXISTS, s);
}

void test_remove_role_without_adapter_returns_bad_notsupported(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));

    mu_variant_t inputs[1] = {make_nodeid_variant(0u, 1u)};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_REMOVE_ROLE, MU_ID_ROLE_SET_TYPE, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, s);
}

/* --- AddIdentity --- */

void test_add_identity_succeeds(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Seed with AddRole. */
    mu_variant_t add_inputs[2] = {make_string_variant("Operator"), make_string_variant("urn:test")};
    mu_variant_t add_outputs[1];
    size_t add_out = 1u;
    (void)invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, add_inputs, 2u, add_outputs, &add_out);

    mu_variant_t inputs[1] = {make_string_variant("user1")};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    /* AddIdentity is called on a Role instance. */
    opcua_statuscode_t s = invoke_registered(s_server, MU_ID_ROLE_ADD_IDENTITY, 1u, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
}

void test_add_identity_missing_role_returns_bad_noentryexists(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t inputs[1] = {make_string_variant("user1")};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_ADD_IDENTITY, MU_ID_ROLE_TYPE, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOENTRYEXISTS, s);
}

void test_add_identity_without_adapter_returns_bad_notsupported(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));

    mu_variant_t inputs[1] = {make_string_variant("user1")};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_ADD_IDENTITY, MU_ID_ROLE_TYPE, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, s);
}

/* --- RemoveIdentity --- */

void test_remove_identity_succeeds(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    /* Seed with AddRole. */
    mu_variant_t add_inputs[2] = {make_string_variant("Operator"), make_string_variant("urn:test")};
    mu_variant_t add_outputs[1];
    size_t add_out = 1u;
    (void)invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, add_inputs, 2u, add_outputs, &add_out);

    mu_variant_t inputs[1] = {make_string_variant("user1")};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s = invoke_registered(s_server, MU_ID_ROLE_REMOVE_IDENTITY, 1u, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
}

void test_remove_identity_without_adapter_returns_bad_notsupported(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(NULL));

    mu_variant_t inputs[1] = {make_string_variant("user1")};
    mu_variant_t outputs[1];
    size_t out_count = 0u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_REMOVE_IDENTITY, MU_ID_ROLE_TYPE, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTSUPPORTED, s);
}

/* --- Argument validation --- */

void test_add_role_wrong_arg_type_returns_bad_invalidargument(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t bad_input;
    (void)memset(&bad_input, 0, sizeof(bad_input));
    bad_input.type = MU_TYPE_UINT32;
    bad_input.value.ui32 = 42u;
    mu_variant_t outputs[1];
    (void)memset(outputs, 0, sizeof(outputs));
    size_t out_count = 1u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, &bad_input, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INVALIDARGUMENT, s);
}

void test_add_role_missing_args_returns_bad_argumentsmissing(void) {
    mock_reset();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server(&s_mock_adapter));

    mu_variant_t inputs[1] = {make_string_variant("Operator")};
    mu_variant_t outputs[1];
    size_t out_count = 1u;

    opcua_statuscode_t s =
        invoke_registered(s_server, MU_ID_ROLE_ADD_ROLE, MU_ID_ROLE_SET_TYPE, inputs, 1u, outputs, &out_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ARGUMENTSMISSING, s);
}

#else

void test_role_management_requires_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_CU_USER_ROLE_MANAGEMENT is disabled in this build");
}

#endif /* MUC_OPCUA_CU_USER_ROLE_MANAGEMENT */

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_CU_USER_ROLE_MANAGEMENT
    RUN_TEST(test_role_management_register_wires_four_methods);
    RUN_TEST(test_add_role_creates_role);
    RUN_TEST(test_add_role_without_adapter_returns_bad_notsupported);
    RUN_TEST(test_remove_role_deletes_role);
    RUN_TEST(test_remove_role_missing_returns_bad_noentryexists);
    RUN_TEST(test_remove_role_without_adapter_returns_bad_notsupported);
    RUN_TEST(test_add_identity_succeeds);
    RUN_TEST(test_add_identity_missing_role_returns_bad_noentryexists);
    RUN_TEST(test_add_identity_without_adapter_returns_bad_notsupported);
    RUN_TEST(test_remove_identity_succeeds);
    RUN_TEST(test_remove_identity_without_adapter_returns_bad_notsupported);
    RUN_TEST(test_add_role_wrong_arg_type_returns_bad_invalidargument);
    RUN_TEST(test_add_role_missing_args_returns_bad_argumentsmissing);
#else
    RUN_TEST(test_role_management_requires_build);
#endif
    return UNITY_END();
}
