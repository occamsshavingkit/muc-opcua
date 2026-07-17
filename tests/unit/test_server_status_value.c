/* tests/unit/test_server_status_value.c
 *
 * Spec 084 (CU 3802/3808): reading the ServerStatus (i=2256) .Value attribute must
 * return a live ServerStatusDataType ExtensionObject (OPC-10000-5 §12.10), not
 * Bad_NotReadable. Before the fix, node 2256 in the runtime-bound dynamic space had
 * a NULL value source, so this test's first assertion (node must be resolvable AND
 * readable) fails.
 */
#include "../../src/address_space/base_nodes.h"
#include "../../src/core/server_internal.h"
#include "../../src/services/read/common.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/diagnostics.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_BASE_NODES

#define TEST_FIXED_TIME ((opcua_datetime_t)132223104000000000LL) /* arbitrary nonzero */
#define TEST_START_TIME ((opcua_datetime_t)132223100000000000LL)

static opcua_datetime_t stub_get_time(void *context) {
    (void)context;
    return TEST_FIXED_TIME;
}

static opcua_uint64_t stub_get_tick_ms(void *context) {
    (void)context;
    return 0;
}

void test_server_status_value_is_readable_extension_object(void) {
    mu_time_adapter_t time_adapter = {NULL, stub_get_time, stub_get_tick_ms};

    mu_base_runtime_nodes_t rn;
    memset(&rn, 0, sizeof(rn));
    mu_base_runtime_init(&rn, &time_adapter, TEST_START_TIME
#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS
                         ,
                         NULL
#endif
    );

    const mu_node_t *node = NULL;
    for (size_t i = 0; i < rn.space.node_count; ++i) {
        if (rn.space.nodes[i].node_id.identifier.numeric == 2256u) {
            node = &rn.space.nodes[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "ServerStatus (2256) must be resolvable in the runtime dynamic space");
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLE, node->node_class);
    TEST_ASSERT_NOT_NULL_MESSAGE(node->value, "ServerStatus (2256) must have a non-NULL value source");

    mu_variant_t v;
    memset(&v, 0, sizeof(v));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_attribute(NULL, node, MU_ATTRIBUTEID_VALUE, &v));
    TEST_ASSERT_EQUAL(MU_TYPE_EXTENSIONOBJECT, v.type);
    TEST_ASSERT_FALSE(v.is_array);
    TEST_ASSERT_EQUAL_UINT32(864u, v.ext_encoding_id);

    /* Encode the scalar variant and decode the ExtensionObject envelope: TypeId
       must be the numeric ServerStatusDataType_Encoding_DefaultBinary (864), and
       the body must be non-empty (StartTime + CurrentTime + State alone are 20
       bytes, well before BuildInfo/shutdown fields). */
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_variant(&w, &v));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    opcua_byte_t enc_mask = 0;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(&r, &enc_mask));
    TEST_ASSERT_EQUAL_UINT8(MU_TYPE_EXTENSIONOBJECT, enc_mask); /* 22, no array bit */

    mu_nodeid_t type_id;
    size_t body_len = 0;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&r, &type_id, &body_len));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, type_id.identifier_type);
    TEST_ASSERT_EQUAL_UINT32(864u, type_id.identifier.numeric);
    TEST_ASSERT_TRUE_MESSAGE(body_len >= 20u, "ServerStatusDataType body must be non-empty");

    /* Body field order: StartTime(Int64), CurrentTime(Int64), State(Int32). Confirm
       the live time-adapter values round-trip. */
    opcua_int64_t start_time = 0;
    opcua_int64_t current_time = 0;
    opcua_int32_t state = -1;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int64(&r, &start_time));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int64(&r, &current_time));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &state));
    TEST_ASSERT_EQUAL_INT64(TEST_START_TIME, start_time);
    TEST_ASSERT_EQUAL_INT64(TEST_FIXED_TIME, current_time);
    TEST_ASSERT_EQUAL_INT32(0, state); /* ServerState.Running */
}

#else

void test_server_status_value_require_base_nodes(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_BASE_NODES is disabled in this build");
}

#endif /* MUC_OPCUA_BASE_NODES */

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_BASE_NODES
    RUN_TEST(test_server_status_value_is_readable_extension_object);
#else
    RUN_TEST(test_server_status_value_require_base_nodes);
#endif
    return UNITY_END();
}
