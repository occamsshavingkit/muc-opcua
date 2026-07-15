/* tests/unit/test_write_decoder.c */
#include "../../src/services/write.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_SERVICE_WRITE
static void init_sentinel_write_state(mu_write_request_t *req, mu_write_value_t *nodes, size_t node_count) {
    req->num_nodes_to_write = 77;
    req->nodes_to_write = &nodes[1];

    for (size_t i = 0; i < node_count; ++i) {
        nodes[i].node_id.namespace_index = (opcua_uint16_t)(40 + i);
        nodes[i].node_id.identifier_type = MU_NODEID_NUMERIC;
        nodes[i].node_id.identifier.numeric = (opcua_uint32_t)(9000 + i);
        nodes[i].attribute_id = (opcua_uint32_t)(100 + i);
        nodes[i].index_range.length = -1;
        nodes[i].index_range.data = (const opcua_byte_t *)0x1;
        nodes[i].value.has_value = true;
        nodes[i].value.value.type = MU_TYPE_INT32;
        nodes[i].value.value.value.i32 = (opcua_int32_t)(-100 - (int)i);
    }
}

static void assert_sentinel_write_state_unchanged(const mu_write_request_t *req, const mu_write_value_t *nodes,
                                                  size_t node_count) {
    TEST_ASSERT_EQUAL_size_t(77, req->num_nodes_to_write);
    TEST_ASSERT_EQUAL_PTR(&nodes[1], req->nodes_to_write);

    for (size_t i = 0; i < node_count; ++i) {
        TEST_ASSERT_EQUAL_UINT16((opcua_uint16_t)(40 + i), nodes[i].node_id.namespace_index);
        TEST_ASSERT_EQUAL_INT(MU_NODEID_NUMERIC, nodes[i].node_id.identifier_type);
        TEST_ASSERT_EQUAL_UINT32((opcua_uint32_t)(9000 + i), nodes[i].node_id.identifier.numeric);
        TEST_ASSERT_EQUAL_UINT32((opcua_uint32_t)(100 + i), nodes[i].attribute_id);
        TEST_ASSERT_EQUAL_INT32(-1, nodes[i].index_range.length);
        TEST_ASSERT_EQUAL_PTR((const opcua_byte_t *)0x1, nodes[i].index_range.data);
        TEST_ASSERT_TRUE(nodes[i].value.has_value);
        TEST_ASSERT_EQUAL_INT(MU_TYPE_INT32, nodes[i].value.value.type);
        TEST_ASSERT_EQUAL_INT32((opcua_int32_t)(-100 - (int)i), nodes[i].value.value.value.i32);
    }
}

static opcua_statuscode_t decode_write_fixture(const opcua_byte_t *buf, size_t len, mu_write_request_t *req,
                                               mu_write_value_t *nodes, size_t max_nodes) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    return mu_write_request_decode(&r, req, nodes, max_nodes);
}

static size_t build_malformed_array_request(opcua_byte_t *buf, size_t len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, len);
    /* SCN-004 / OPC-10000-6 section 5.2.1: array counts below -1 are malformed. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, -2));
    return w.position;
}

static size_t build_excessive_count_request(opcua_byte_t *buf, size_t len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 3));
    return w.position;
}

static size_t build_truncated_body_request(opcua_byte_t *buf, size_t len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x02));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(&w, 2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1001));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 13));
    return w.position;
}

static size_t build_invalid_nodeid_request(opcua_byte_t *buf, size_t len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x0F));
    return w.position;
}
#endif

void test_write_request_decode_happy_path(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    /* WriteRequest body:
     * - Array size (Int32): 1
     * - WriteValue[0]:
     *   - NodeId: Numeric (2, 1001)
     *   - AttributeId: UInt32 (13, Value attribute)
     *   - IndexRange: String (null)
     *   - Value: DataValue
     *     - Encoding mask: 0x01 (HasValue)
     *     - Variant: Int32 (value: 42)
     */
    /* Write Array Size */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1));

    /* WriteValue[0] NodeId */
    mu_nodeid_t nid;
    nid.namespace_index = 2;
    nid.identifier_type = MU_NODEID_NUMERIC;
    nid.identifier.numeric = 1001;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &nid));

    /* AttributeId */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 13));

    /* IndexRange (null string) */
    mu_string_t idx_range = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&w, &idx_range));

    /* Value (DataValue: has_value=true, variant=Int32 42) */
    mu_datavalue_t val;
    (void)memset(&val, 0, sizeof(val));
    val.has_value = true;
    val.value.type = MU_TYPE_INT32;
    val.value.value.i32 = 42;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_datavalue(&w, &val));

    /* Now decode it */
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_write_request_t req;
    mu_write_value_t nodes[2];
    (void)memset(&req, 0, sizeof(req));
    (void)memset(nodes, 0, sizeof(nodes));

    opcua_statuscode_t status = mu_write_request_decode(&r, &req, nodes, 2);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(1, req.num_nodes_to_write);
    TEST_ASSERT_EQUAL(2, req.nodes_to_write[0].node_id.namespace_index);
    TEST_ASSERT_EQUAL(1001, req.nodes_to_write[0].node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(13, req.nodes_to_write[0].attribute_id);
    TEST_ASSERT_EQUAL_INT(MU_TYPE_INT32, req.nodes_to_write[0].value.value.type);
    TEST_ASSERT_EQUAL(42, req.nodes_to_write[0].value.value.value.i32);
#endif
}

void test_write_request_decode_errors(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    /* Too many operations */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 5));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_write_request_t req;
    mu_write_value_t nodes[2];
    opcua_statuscode_t status = mu_write_request_decode(&r, &req, nodes, 2);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TOOMANYOPERATIONS, status);
#endif
}

void test_write_request_decode_rejects_malformed_array_without_state_mutation(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[16];
    mu_write_request_t req;
    mu_write_value_t nodes[2];
    size_t len = build_malformed_array_request(buf, sizeof(buf));

    init_sentinel_write_state(&req, nodes, 2);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, decode_write_fixture(buf, len, &req, nodes, 2));
    assert_sentinel_write_state_unchanged(&req, nodes, 2);
#endif
}

void test_write_request_decode_rejects_excessive_count_without_state_mutation(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[16];
    mu_write_request_t req;
    mu_write_value_t nodes[2];
    size_t len = build_excessive_count_request(buf, sizeof(buf));

    init_sentinel_write_state(&req, nodes, 2);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TOOMANYOPERATIONS, decode_write_fixture(buf, len, &req, nodes, 2));
    assert_sentinel_write_state_unchanged(&req, nodes, 2);
#endif
}

void test_write_request_decode_rejects_truncated_body_without_state_mutation(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[32];
    mu_write_request_t req;
    mu_write_value_t nodes[2];
    size_t len = build_truncated_body_request(buf, sizeof(buf));

    init_sentinel_write_state(&req, nodes, 2);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, decode_write_fixture(buf, len, &req, nodes, 2));
    assert_sentinel_write_state_unchanged(&req, nodes, 2);
#endif
}

void test_write_request_decode_rejects_invalid_nodeid_without_state_mutation(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[16];
    mu_write_request_t req;
    mu_write_value_t nodes[2];
    size_t len = build_invalid_nodeid_request(buf, sizeof(buf));

    init_sentinel_write_state(&req, nodes, 2);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, decode_write_fixture(buf, len, &req, nodes, 2));
    assert_sentinel_write_state_unchanged(&req, nodes, 2);
#endif
}

void test_write_response_encode(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    opcua_statuscode_t results[] = {MU_STATUS_GOOD, MU_STATUS_BAD_NOTWRITABLE};
    mu_write_response_t resp;
    resp.num_results = 2;
    resp.results = results;

    opcua_statuscode_t status = mu_write_response_encode(&w, &resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    /* Verify encoding:
     * - Array size (Int32): 2
     * - Result[0]: Good (0x00000000)
     * - Result[1]: Bad_NotWritable (0x803B0000)
     */
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    opcua_int32_t len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &len));
    TEST_ASSERT_EQUAL(2, len);

    opcua_statuscode_t r1, r2;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &r1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r1);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &r2));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTWRITABLE, r2);

    /* Verify diagnosticInfos: null array encoded as Int32 -1
     * per OPC-10000-6 §5.2.5 and OPC-10000-4 §5.11.4.2 Table 53 */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &len));
    TEST_ASSERT_EQUAL(-1, len);

    TEST_ASSERT_EQUAL_size_t(w.position, r.position);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_request_decode_happy_path);
    RUN_TEST(test_write_request_decode_errors);
    RUN_TEST(test_write_request_decode_rejects_malformed_array_without_state_mutation);
    RUN_TEST(test_write_request_decode_rejects_excessive_count_without_state_mutation);
    RUN_TEST(test_write_request_decode_rejects_truncated_body_without_state_mutation);
    RUN_TEST(test_write_request_decode_rejects_invalid_nodeid_without_state_mutation);
    RUN_TEST(test_write_response_encode);
    return UNITY_END();
}
