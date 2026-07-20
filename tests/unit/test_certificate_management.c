/* tests/unit/test_certificate_management.c
 *
 * spec 096 (CU 2105): Global Certificate Management Server Facet unit tests.
 * Validates that the CertificateGroupType(12555) and CertificateType(12556)
 * ObjectType InstanceDeclarations are present in the address space.
 *
 * This is a type-system-only slice; no Methods are tested in this iteration.
 * The test initializes a minimal server (no adapter) and verifies that both
 * type nodes exist and are browsable.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/certificate_management.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT && MUC_OPCUA_CU_METHOD_SERVER && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION

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

static opcua_statuscode_t init_test_server(void) {
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.application_uri = "urn:test:certmgmt";
    config.application_name = "certmgmt-test";
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
    config.certificate_management_adapter = NULL;
    return mu_server_init(s_server_storage, sizeof(s_server_storage), &config, &s_server);
}

/* Look up a node in the static address space by numeric NodeId. Returns
 * the node pointer or NULL when not found. */
static const mu_node_t *find_node(opcua_uint32_t numeric_id) {
    const mu_address_space_t *bs = mu_base_address_space();
    mu_nodeid_t id = {0u, MU_NODEID_NUMERIC, {numeric_id}};
    return mu_address_space_find_node(bs, (mu_address_space_index_t *)0, &id);
}

void test_server_init_accepts_null_adapter(void) {
    opcua_statuscode_t s = init_test_server();
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_NOT_NULL(s_server);
}

void test_certificate_group_type_node_exists(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server());
    const mu_node_t *node = find_node(MU_ID_CERTIFICATE_GROUP_TYPE);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(MU_NODECLASS_OBJECTTYPE, node->node_class);
}

void test_certificate_type_node_exists(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server());
    const mu_node_t *node = find_node(MU_ID_CERTIFICATE_TYPE);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(MU_NODECLASS_OBJECTTYPE, node->node_class);
}

void test_certificate_group_type_has_subtype_of_base_object_type(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server());
    const mu_node_t *node = find_node(MU_ID_CERTIFICATE_GROUP_TYPE);
    TEST_ASSERT_NOT_NULL(node);

    /* The HasSubtype reference is an inverse reference on the child type,
     * with is_forward=false and target BaseObjectType(58). */
    opcua_boolean_t found = false;
    for (size_t i = 0; i < node->reference_count; ++i) {
        const mu_reference_t *ref = &node->references[i];
        if (!ref->is_forward && ref->reference_type_id.identifier.numeric == 45u &&
            ref->target_id.identifier.numeric == 58u) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void test_certificate_type_has_subtype_of_base_object_type(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, init_test_server());
    const mu_node_t *node = find_node(MU_ID_CERTIFICATE_TYPE);
    TEST_ASSERT_NOT_NULL(node);

    opcua_boolean_t found = false;
    for (size_t i = 0; i < node->reference_count; ++i) {
        const mu_reference_t *ref = &node->references[i];
        if (!ref->is_forward && ref->reference_type_id.identifier.numeric == 45u &&
            ref->target_id.identifier.numeric == 58u) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

#else

void test_certificate_management_requires_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT is disabled in this build");
}

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT */

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT && MUC_OPCUA_CU_METHOD_SERVER && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    RUN_TEST(test_server_init_accepts_null_adapter);
    RUN_TEST(test_certificate_group_type_node_exists);
    RUN_TEST(test_certificate_type_node_exists);
    RUN_TEST(test_certificate_group_type_has_subtype_of_base_object_type);
    RUN_TEST(test_certificate_type_has_subtype_of_base_object_type);
#else
    RUN_TEST(test_certificate_management_requires_build);
#endif
    return UNITY_END();
}
