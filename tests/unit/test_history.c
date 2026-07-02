#include "muc_opcua/config.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/history.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_SERVICE_HISTORY

#include "../../src/core/server_internal.h"
#include "../../src/services/history.h"
#include "../../src/services/service_header.h"
#include "muc_opcua/opcua_ids.h"

opcua_statuscode_t handle_history_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length);
opcua_statuscode_t handle_history_update(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);

static bool pointer_points_into_buffer(const opcua_byte_t *ptr, const opcua_byte_t *buf, size_t len) {
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)buf;

    return p >= start && (p - start) < len;
}

static void write_history_read_request_with_continuation_point(mu_binary_writer_t *w, opcua_byte_t *buf, size_t buf_len,
                                                               const opcua_byte_t *continuation_point,
                                                               size_t continuation_point_len) {
    mu_binary_writer_init(w, buf, buf_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(
                                          w, &(mu_nodeid_t){.namespace_index = 0,
                                                            .identifier_type = MU_NODEID_NUMERIC,
                                                            .identifier.numeric =
                                                                MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(w, 0x01));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(w, 22));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(w, false));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(w, 10000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(w, 20000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(w, 100));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(w, true));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(w, 2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(w, false));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(w, &(mu_nodeid_t){.namespace_index = 2,
                                                                               .identifier_type = MU_NODEID_NUMERIC,
                                                                               .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(w, &(mu_string_t){-1, NULL}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(w, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(w, &(mu_string_t){-1, NULL}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_bytestring(w, &(mu_bytestring_t){.length = (opcua_int32_t)continuation_point_len,
                                                                       .data = continuation_point}));
}

void test_history_read_decode(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    /* Encode a HistoryReadRequest */

    // historyReadDetails ExtensionObject
    mu_nodeid_t ext_id = {.identifier_type = MU_NODEID_NUMERIC,
                          .namespace_index = 0,
                          .identifier.numeric = MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &ext_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01)); // EncodingMask = ByteString

    // Length of ReadRawModifiedDetails (calculate or fake it and use a sub-writer)
    // isReadModified (1) + startTime (8) + endTime (8) + numValuesPerNode (4) + returnBounds (1) = 22 bytes
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 22));

    // ReadRawModifiedDetails body
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, false)); // isReadModified
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 10000));   // startTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 20000));   // endTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 100));    // numValuesPerNode
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, true));  // returnBounds

    // timestampsToReturn
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 2)); // Both

    // releaseContinuationPoints
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, false));

    // nodesToRead array (1 element)
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1));

    mu_nodeid_t node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 2, .identifier.numeric = 1001};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &node_id));

    mu_string_t index_range = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&w, &index_range));

    // dataEncoding (QualifiedName)
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(&w, 0));
    mu_string_t encoding_name = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&w, &encoding_name));

    // continuationPoint (ByteString)
    mu_bytestring_t cp = {-1, NULL};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&w, &cp));

    /* Decode it */
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_history_read_request_t req;
    mu_history_read_value_id_t nodes[2];
    memset(&req, 0, sizeof(req));
    memset(nodes, 0, sizeof(nodes));

    opcua_statuscode_t status = mu_history_read_request_decode(&r, &req, nodes, 2);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    TEST_ASSERT_EQUAL(false, req.details.is_read_modified);
    TEST_ASSERT_EQUAL(10000, req.details.start_time);
    TEST_ASSERT_EQUAL(20000, req.details.end_time);
    TEST_ASSERT_EQUAL(100, req.details.num_values_per_node);
    TEST_ASSERT_EQUAL(true, req.details.return_bounds);
    TEST_ASSERT_EQUAL(2, req.timestamps_to_return);
    TEST_ASSERT_EQUAL(false, req.release_continuation_points);
    TEST_ASSERT_EQUAL(1, req.num_nodes_to_read);
    TEST_ASSERT_EQUAL(1001, req.nodes_to_read[0].node_id.identifier.numeric);
#endif
}

void test_history_read_details_extension_object_truncated_body_returns_decoding_error(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    const opcua_int32_t encoded_details_body_len = 22;
    const opcua_int32_t declared_details_body_len = encoded_details_body_len + 1;

    mu_binary_writer_init(&w, buf, sizeof(buf));

    /* OPC-10000-4 section 5.11.3.2: HistoryRead carries historyReadDetails.
       OPC-10000-6 section 5.2.2.15: ExtensionObject body length bounds the body. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(
                                          &w, &(mu_nodeid_t){.namespace_index = 0,
                                                             .identifier_type = MU_NODEID_NUMERIC,
                                                             .identifier.numeric =
                                                                 MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, declared_details_body_len));

    /* The ReadRawModifiedDetails body below is one byte shorter than declared. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, false));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 10000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 20000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 100));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, true));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, false));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 0));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_history_read_request_t req;
    mu_history_read_value_id_t nodes[1];
    memset(&req, 0, sizeof(req));
    memset(nodes, 0, sizeof(nodes));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_history_read_request_decode(&r, &req, nodes, 1));
#endif
}

void test_history_read_decode_continuation_point_is_owned_from_request_buffer(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[256];
    const opcua_byte_t continuation_point[] = {'O', 'W', 'N', '1'};
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    /* OPC-10000-4 section 5.11.3.2 defines HistoryRead's per-node
       continuationPoint parameter. OPC-10000-4 section 7.9 defines a
       ContinuationPoint as a Server-defined opaque value used to restart
       HistoryRead later, so decoded bytes must outlive transient request data. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(
                                          &w, &(mu_nodeid_t){.namespace_index = 0,
                                                             .identifier_type = MU_NODEID_NUMERIC,
                                                             .identifier.numeric =
                                                                 MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 22));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, false));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 10000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 20000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 100));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, true));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, false));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &(mu_nodeid_t){.namespace_index = 2,
                                                                                .identifier_type = MU_NODEID_NUMERIC,
                                                                                .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&w, &(mu_string_t){-1, NULL}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(&w, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&w, &(mu_string_t){-1, NULL}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(
                                          &w, &(mu_bytestring_t){.length = (opcua_int32_t)sizeof(continuation_point),
                                                                 .data = continuation_point}));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_history_read_request_t req;
    mu_history_read_value_id_t nodes[1];
    memset(&req, 0, sizeof(req));
    memset(nodes, 0, sizeof(nodes));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_history_read_request_decode(&r, &req, nodes, 1));
    TEST_ASSERT_EQUAL(1, req.num_nodes_to_read);
    TEST_ASSERT_EQUAL((opcua_int32_t)sizeof(continuation_point), req.nodes_to_read[0].continuation_point.length);
    TEST_ASSERT_NOT_NULL(req.nodes_to_read[0].continuation_point.data);
    TEST_ASSERT_EQUAL(false, pointer_points_into_buffer(req.nodes_to_read[0].continuation_point.data, buf, w.position));

    memset(buf, 0xA5, w.position);
    TEST_ASSERT_EQUAL_INT8_ARRAY(continuation_point, req.nodes_to_read[0].continuation_point.data,
                                 sizeof(continuation_point));
#endif
}

void test_history_read_decode_continuation_point_stable_after_later_request_reuses_buffer(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[256];
    const opcua_byte_t first_continuation_point[] = {'F', 'I', 'R', 'S', 'T', '1'};
    const opcua_byte_t later_continuation_point[] = {'L', 'A', 'T', 'E', 'R', '2'};
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_history_read_request_t first_req;
    mu_history_read_request_t later_req;
    mu_history_read_value_id_t first_nodes[1];
    mu_history_read_value_id_t later_nodes[1];

    write_history_read_request_with_continuation_point(&w, buf, sizeof(buf), first_continuation_point,
                                                       sizeof(first_continuation_point));
    mu_binary_reader_init(&r, buf, w.position);
    memset(&first_req, 0, sizeof(first_req));
    memset(first_nodes, 0, sizeof(first_nodes));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_history_read_request_decode(&r, &first_req, first_nodes, 1));
    TEST_ASSERT_EQUAL(1, first_req.num_nodes_to_read);
    TEST_ASSERT_EQUAL_INT32((opcua_int32_t)sizeof(first_continuation_point),
                            first_req.nodes_to_read[0].continuation_point.length);
    TEST_ASSERT_EQUAL_INT8_ARRAY(first_continuation_point, first_req.nodes_to_read[0].continuation_point.data,
                                 sizeof(first_continuation_point));

    write_history_read_request_with_continuation_point(&w, buf, sizeof(buf), later_continuation_point,
                                                       sizeof(later_continuation_point));
    mu_binary_reader_init(&r, buf, w.position);
    memset(&later_req, 0, sizeof(later_req));
    memset(later_nodes, 0, sizeof(later_nodes));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_history_read_request_decode(&r, &later_req, later_nodes, 1));

    /* OPC-10000-4 section 5.11.3.2 defines HistoryRead's per-node
       continuationPoint parameter. OPC-10000-4 section 7.9 defines the
       ContinuationPoint type used to continue a later request, so stored
       continuation-point bytes must not alias transient request storage
       that a later HistoryRead request can reuse. */
    TEST_ASSERT_EQUAL_INT32((opcua_int32_t)sizeof(first_continuation_point),
                            first_req.nodes_to_read[0].continuation_point.length);
    TEST_ASSERT_EQUAL_INT8_ARRAY(first_continuation_point, first_req.nodes_to_read[0].continuation_point.data,
                                 sizeof(first_continuation_point));
    TEST_ASSERT_EQUAL_INT8_ARRAY(later_continuation_point, later_req.nodes_to_read[0].continuation_point.data,
                                 sizeof(later_continuation_point));
#endif
}

void test_history_read_encode(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    mu_datavalue_t dv = {0};
    dv.has_value = true;
    dv.value.type = MU_TYPE_INT32;
    dv.value.value.i32 = 42;
    dv.has_source_timestamp = true;
    dv.source_timestamp = 123456789;

    mu_history_read_result_t result;
    result.status_code = MU_STATUS_GOOD;
    result.continuation_point.length = -1;
    result.continuation_point.data = NULL;
    result.history_data.num_data_values = 1;
    result.history_data.data_values = &dv;

    mu_history_read_response_t resp;
    resp.num_results = 1;
    resp.results = &result;

    opcua_statuscode_t status = mu_history_read_response_encode(&w, &resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    // Verify encoded bytes
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    opcua_int32_t num_results;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &num_results));
    TEST_ASSERT_EQUAL(1, num_results);

    opcua_statuscode_t sc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &sc));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sc);

    mu_bytestring_t cp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&r, &cp));
    TEST_ASSERT_EQUAL(-1, cp.length);

    mu_nodeid_t ext_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &ext_id));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, ext_id.identifier_type);
    TEST_ASSERT_EQUAL(MU_ID_HISTORYDATA_ENCODING_DEFAULTBINARY, ext_id.identifier.numeric);

    opcua_byte_t mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, &mask));
    TEST_ASSERT_EQUAL(0x01, mask); // ByteString

    opcua_int32_t length;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &length));

    opcua_int32_t num_dv;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &num_dv));
    TEST_ASSERT_EQUAL(1, num_dv);

    mu_datavalue_t dec_dv;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_datavalue(&r, &dec_dv));
    TEST_ASSERT_EQUAL(true, dec_dv.has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, dec_dv.value.type);
    TEST_ASSERT_EQUAL(42, dec_dv.value.value.i32);
    TEST_ASSERT_EQUAL(true, dec_dv.has_source_timestamp);
    TEST_ASSERT_EQUAL(123456789, dec_dv.source_timestamp);

    opcua_int32_t diag_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_len));
    TEST_ASSERT_EQUAL(-1, diag_len);

    TEST_ASSERT_EQUAL(w.position, r.position); // Ensure we read everything
#endif
}

static opcua_statuscode_t mock_read_raw_modified(void *context, const mu_nodeid_t *node_id,
                                                 opcua_boolean_t is_read_modified, opcua_datetime_t start_time,
                                                 opcua_datetime_t end_time, opcua_uint32_t num_values_per_node,
                                                 opcua_boolean_t return_bounds, const opcua_byte_t *cp_in,
                                                 size_t cp_in_length, opcua_byte_t *cp_out, size_t *cp_out_length,
                                                 mu_historical_data_point_t *data_points, size_t max_data_points,
                                                 size_t *actual_data_points) {
    (void)context;
    (void)is_read_modified;
    (void)end_time;
    (void)num_values_per_node;
    (void)return_bounds;
    (void)cp_in;
    (void)cp_in_length;
    (void)cp_out;

    if (cp_out_length) {
        *cp_out_length = 0;
    }

    if (node_id->identifier.numeric == 1001 && max_data_points >= 1) {
        data_points[0].source_timestamp = start_time + 10;
        data_points[0].server_timestamp = start_time + 20;
        data_points[0].status = MU_STATUS_GOOD;
        data_points[0].value.type = MU_TYPE_INT32;
        data_points[0].value.value.i32 = 99;
        *actual_data_points = 1;
        return MU_STATUS_GOOD;
    }

    if (node_id->identifier.numeric == 1002 && max_data_points >= 1) {
        if (cp_in_length == 0) {
            data_points[0].source_timestamp = start_time + 10;
            data_points[0].server_timestamp = start_time + 20;
            data_points[0].status = MU_STATUS_GOOD;
            data_points[0].value.type = MU_TYPE_INT32;
            data_points[0].value.value.i32 = 99;
            *actual_data_points = 1;
            if (cp_out && cp_out_length) {
                memcpy(cp_out, "CP01", 4);
                *cp_out_length = 4;
            }
            return MU_STATUS_GOOD;
        } else if (cp_in_length == 4 && memcmp(cp_in, "CP01", 4) == 0) {
            data_points[0].source_timestamp = start_time + 30;
            data_points[0].server_timestamp = start_time + 40;
            data_points[0].status = MU_STATUS_GOOD;
            data_points[0].value.type = MU_TYPE_INT32;
            data_points[0].value.value.i32 = 100;
            *actual_data_points = 1;
            return MU_STATUS_GOOD;
        }
    }

    return MU_STATUS_BAD_NODEIDUNKNOWN;
}

void test_history_read_dispatch(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    mu_history_adapter_t adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.read_raw_modified = mock_read_raw_modified;

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.history_adapter = adapter;

    struct mu_server server;
    memset(&server, 0, sizeof(server));
    server.config = config;

    opcua_byte_t req_buf[256];
    mu_binary_writer_t req_w;
    mu_binary_writer_init(&req_w, req_buf, sizeof(req_buf));

    // Encode a HistoryRead request header
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0}));  // auth token
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 100));                       // timestamp
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 1));                        // handle
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // return diagnostics
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // audit entry id
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // timeout
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0})); // additionalHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0));                         // encoding mask

    // HistoryReadRequest
    // historyReadDetails (ExtensionObject)
    TEST_ASSERT_EQUAL(
        MU_STATUS_GOOD,
        mu_binary_write_nodeid(
            &req_w, &(mu_nodeid_t){.namespace_index = 0,
                                   .identifier_type = MU_NODEID_NUMERIC,
                                   .identifier.numeric = MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0x01));     // ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 22));      // Length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false)); // isReadModified
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 1000));    // startTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 2000));    // endTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 10));     // numValuesPerNode
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false)); // returnBounds

    // timestampsToReturn
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 2)); // Both
    // releaseContinuationPoints
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false));

    // nodesToRead array (1 element)
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 2,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // indexRange
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(&req_w, 0));                        // namespaceIndex
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // name
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&req_w, &(mu_bytestring_t){-1, NULL})); // CP

    opcua_byte_t res_buf[512];
    mu_binary_writer_t res_w;
    mu_binary_writer_init(&res_w, res_buf, sizeof(res_buf));

    mu_binary_reader_t req_r;
    mu_binary_reader_init(&req_r, req_buf, req_w.position);

    size_t response_length = 0;
    opcua_statuscode_t status = handle_history_read(&server, &req_r, &res_w, &response_length);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_GREATER_THAN(0, response_length);

    // Verify response
    mu_binary_reader_t res_r;
    mu_binary_reader_init(&res_r, res_buf, response_length);

    // Read NodeId for Response
    mu_nodeid_t res_node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &res_node_id));
    TEST_ASSERT_EQUAL(MU_ID_HISTORYREADRESPONSE, res_node_id.identifier.numeric);

    // Read ResponseHeader
    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&res_r, &timestamp));

    opcua_uint32_t handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&res_r, &handle));
    TEST_ASSERT_EQUAL(1, handle);

    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);

    // serviceDiagnostics (null)
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &diag_mask));
    TEST_ASSERT_EQUAL(0, diag_mask);

    // stringTable (null/empty array)
    opcua_int32_t string_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &string_table_len));
    TEST_ASSERT_EQUAL(-1, string_table_len);

    // additionalHeader (null ExtensionObject)
    mu_nodeid_t add_hdr_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &add_hdr_id));
    TEST_ASSERT_EQUAL(0, add_hdr_id.identifier.numeric);
    opcua_byte_t add_hdr_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &add_hdr_mask));
    TEST_ASSERT_EQUAL(0, add_hdr_mask);

    // Read num_results
    opcua_int32_t num_res;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_res));
    TEST_ASSERT_EQUAL(1, num_res);

    opcua_statuscode_t sc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &sc));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sc);

    mu_bytestring_t cp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&res_r, &cp));
    TEST_ASSERT_EQUAL(-1, cp.length);

    mu_nodeid_t ext_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &ext_id));
    TEST_ASSERT_EQUAL(MU_ID_HISTORYDATA_ENCODING_DEFAULTBINARY, ext_id.identifier.numeric);

    opcua_byte_t mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &mask));
    TEST_ASSERT_EQUAL(0x01, mask);

    opcua_int32_t len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &len));

    opcua_int32_t num_dv;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_dv));
    TEST_ASSERT_EQUAL(1, num_dv);

    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_datavalue(&res_r, &dv));
    TEST_ASSERT_EQUAL(true, dv.has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, dv.value.type);
    TEST_ASSERT_EQUAL(99, dv.value.value.i32);
    TEST_ASSERT_EQUAL(true, dv.has_source_timestamp);
    TEST_ASSERT_EQUAL(1010, dv.source_timestamp);
#endif
}

void test_history_read_pagination(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    mu_history_adapter_t adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.read_raw_modified = mock_read_raw_modified;

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.history_adapter = adapter;

    struct mu_server server;
    memset(&server, 0, sizeof(server));
    server.config = config;

    // --- FIRST CALL (Requesting first page without CP) ---
    opcua_byte_t req_buf[256];
    mu_binary_writer_t req_w;
    mu_binary_writer_init(&req_w, req_buf, sizeof(req_buf));

    // RequestHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0}));  // auth token
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 100));                       // timestamp
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 1));                        // handle
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // return diagnostics
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // audit entry id
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // timeout
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0})); // additionalHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0));                         // encoding mask

    // HistoryReadDetails
    TEST_ASSERT_EQUAL(
        MU_STATUS_GOOD,
        mu_binary_write_nodeid(
            &req_w, &(mu_nodeid_t){.namespace_index = 0,
                                   .identifier_type = MU_NODEID_NUMERIC,
                                   .identifier.numeric = MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0x01));     // ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 22));      // Length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false)); // isReadModified
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 1000));    // startTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 2000));    // endTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 10));     // numValuesPerNode
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false)); // returnBounds

    // timestampsToReturn
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 2)); // Both
    // releaseContinuationPoints
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false));

    // nodesToRead array (1 element for node 1002)
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 2,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 1002}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // indexRange
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(&req_w, 0));                        // namespaceIndex
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // name
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&req_w, &(mu_bytestring_t){-1, NULL})); // CP (empty)

    opcua_byte_t res_buf[512];
    mu_binary_writer_t res_w;
    mu_binary_writer_init(&res_w, res_buf, sizeof(res_buf));

    mu_binary_reader_t req_r;
    mu_binary_reader_init(&req_r, req_buf, req_w.position);

    size_t response_length = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, handle_history_read(&server, &req_r, &res_w, &response_length));

    // Verify first response CP is "CP01"
    mu_binary_reader_t res_r;
    mu_binary_reader_init(&res_r, res_buf, response_length);

    // Skip response header
    mu_nodeid_t res_node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &res_node_id));
    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&res_r, &timestamp));
    opcua_uint32_t handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&res_r, &handle));
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &service_result));
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &diag_mask));
    opcua_int32_t string_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &string_table_len));
    mu_nodeid_t add_hdr_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &add_hdr_id));
    opcua_byte_t add_hdr_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &add_hdr_mask));

    // Results array
    opcua_int32_t num_res;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_res));
    TEST_ASSERT_EQUAL(1, num_res);

    opcua_statuscode_t sc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &sc));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sc);

    mu_bytestring_t cp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&res_r, &cp));
    TEST_ASSERT_EQUAL(4, cp.length);
    TEST_ASSERT_EQUAL_INT8_ARRAY("CP01", cp.data, 4);

    // --- SECOND CALL (Passing "CP01" back) ---
    mu_binary_writer_init(&req_w, req_buf, sizeof(req_buf));

    // RequestHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0}));  // auth token
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 100));                       // timestamp
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 1));                        // handle
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // return diagnostics
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // audit entry id
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // timeout
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0})); // additionalHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0));                         // encoding mask

    // HistoryReadDetails
    TEST_ASSERT_EQUAL(
        MU_STATUS_GOOD,
        mu_binary_write_nodeid(
            &req_w, &(mu_nodeid_t){.namespace_index = 0,
                                   .identifier_type = MU_NODEID_NUMERIC,
                                   .identifier.numeric = MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0x01));     // ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 22));      // Length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false)); // isReadModified
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 1000));    // startTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 2000));    // endTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 10));     // numValuesPerNode
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false)); // returnBounds

    // timestampsToReturn
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 2)); // Both
    // releaseContinuationPoints
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, false));

    // nodesToRead array (1 element for node 1002)
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 2,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 1002}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // indexRange
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint16(&req_w, 0));                        // namespaceIndex
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // name

    // Pass CP "CP01" back
    mu_bytestring_t cp_to_pass = {4, (const opcua_byte_t *)"CP01"};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_bytestring(&req_w, &cp_to_pass)); // CP

    mu_binary_writer_init(&res_w, res_buf, sizeof(res_buf));
    mu_binary_reader_init(&req_r, req_buf, req_w.position);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, handle_history_read(&server, &req_r, &res_w, &response_length));

    // Verify second response
    mu_binary_reader_init(&res_r, res_buf, response_length);

    // Skip response header
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &res_node_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&res_r, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&res_r, &handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &diag_mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &string_table_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &add_hdr_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &add_hdr_mask));

    // Results array
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_res));
    TEST_ASSERT_EQUAL(1, num_res);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &sc));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sc);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&res_r, &cp));
    TEST_ASSERT_EQUAL(-1, cp.length); // CP should be empty now

    // Read details
    opcua_byte_t mask;
    opcua_int32_t len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &res_node_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &len));

    opcua_int32_t num_dv;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_dv));
    TEST_ASSERT_EQUAL(1, num_dv);

    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_datavalue(&res_r, &dv));
    TEST_ASSERT_EQUAL(true, dv.has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, dv.value.type);
    TEST_ASSERT_EQUAL(100, dv.value.value.i32); // Next value
    TEST_ASSERT_EQUAL(true, dv.has_source_timestamp);
    TEST_ASSERT_EQUAL(1030, dv.source_timestamp);
#endif
}

static opcua_statuscode_t mock_update_data(void *context, const mu_nodeid_t *node_id,
                                           opcua_uint32_t perform_insert_replace,
                                           const mu_historical_data_point_t *data_points, size_t data_points_count,
                                           opcua_statuscode_t *results) {
    (void)context;
    if (node_id->identifier.numeric == 1001) {
        TEST_ASSERT_EQUAL(2, perform_insert_replace); // Replace
        TEST_ASSERT_EQUAL(1, data_points_count);
        TEST_ASSERT_EQUAL(99, data_points[0].value.value.i32);
        results[0] = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }
    return MU_STATUS_BAD_NODEIDUNKNOWN;
}

static opcua_statuscode_t mock_delete_raw_modified(void *context, const mu_nodeid_t *node_id,
                                                   opcua_boolean_t is_delete_modified, opcua_datetime_t start_time,
                                                   opcua_datetime_t end_time) {
    (void)context;
    if (node_id->identifier.numeric == 1001) {
        TEST_ASSERT_EQUAL(true, is_delete_modified);
        TEST_ASSERT_EQUAL(1000, start_time);
        TEST_ASSERT_EQUAL(2000, end_time);
        return MU_STATUS_GOOD;
    }
    return MU_STATUS_BAD_NODEIDUNKNOWN;
}

void test_history_update_decode(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    // Encode a HistoryUpdateRequest
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 2)); // 2 update details

    // --- Detail 1: UpdateDataDetails ---
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(
                          &w, &(mu_nodeid_t){.namespace_index = 0,
                                             .identifier_type = MU_NODEID_NUMERIC,
                                             .identifier.numeric = MU_ID_UPDATEDATADETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01)); // encoding mask = ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 38));  // length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &(mu_nodeid_t){.namespace_index = 2,
                                                                                .identifier_type = MU_NODEID_NUMERIC,
                                                                                .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 2)); // performInsertReplace = Replace
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1));  // 1 value
    // DataValue (hasValue, hasStatus, hasSourceTimestamp, hasServerTimestamp) -> 0x0F
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x0F));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, MU_TYPE_INT32));        // Variant type
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 99));                  // Variant value
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_statuscode(&w, MU_STATUS_GOOD)); // status
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 1000));                // source timestamp
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 2000));                // server timestamp

    // --- Detail 2: DeleteRawModifiedDetails ---
    TEST_ASSERT_EQUAL(
        MU_STATUS_GOOD,
        mu_binary_write_nodeid(
            &w, &(mu_nodeid_t){.namespace_index = 0,
                               .identifier_type = MU_NODEID_NUMERIC,
                               .identifier.numeric = MU_ID_DELETERAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01)); // encoding mask = ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 21));  // length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &(mu_nodeid_t){.namespace_index = 2,
                                                                                .identifier_type = MU_NODEID_NUMERIC,
                                                                                .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&w, true)); // isDeleteModified
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 1000));   // startTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&w, 2000));   // endTime

    // Decode
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    mu_history_update_item_t items[5];
    mu_history_update_request_t req;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_history_update_request_decode(&r, &req, items, 5));
    TEST_ASSERT_EQUAL(2, req.num_items);

    // Verify detail 1
    TEST_ASSERT_EQUAL(MU_HISTORY_UPDATE_TYPE_DATA, req.items[0].type);
    TEST_ASSERT_EQUAL(1001, req.items[0].body.data.node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(2, req.items[0].body.data.perform_insert_replace);
    TEST_ASSERT_EQUAL(1, req.items[0].body.data.num_values);
    TEST_ASSERT_EQUAL(99, req.items[0].body.data.values[0].value.value.i32);

    // Verify detail 2
    TEST_ASSERT_EQUAL(MU_HISTORY_UPDATE_TYPE_DELETE, req.items[1].type);
    TEST_ASSERT_EQUAL(1001, req.items[1].body.delete_raw.node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(true, req.items[1].body.delete_raw.is_delete_modified);
    TEST_ASSERT_EQUAL(1000, req.items[1].body.delete_raw.start_time);
    TEST_ASSERT_EQUAL(2000, req.items[1].body.delete_raw.end_time);
#endif
}

/* Feature 025 (F11): a negative ExtensionObject body length must be rejected as a
   decoding error, not used in (size_t) skip math. OPC-10000-6 §5.2.2.15. */
void test_history_update_decode_rejects_negative_length(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    opcua_byte_t buf[64];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1)); /* 1 update detail */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(
                          &w, &(mu_nodeid_t){.namespace_index = 0,
                                             .identifier_type = MU_NODEID_NUMERIC,
                                             .identifier.numeric = MU_ID_UPDATEDATADETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01)); /* encoding mask = ByteString */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, -1));  /* negative body length */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_history_update_item_t items[2];
    mu_history_update_request_t req;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_history_update_request_decode(&r, &req, items, 2));
#endif
}

void test_history_update_dispatch(void) {
#ifdef MUC_OPCUA_SERVICE_HISTORY
    mu_history_adapter_t adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.update_data = mock_update_data;
    adapter.delete_raw_modified = mock_delete_raw_modified;

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.history_adapter = adapter;

    struct mu_server server;
    memset(&server, 0, sizeof(server));
    server.config = config;

    opcua_byte_t req_buf[256];
    mu_binary_writer_t req_w;
    mu_binary_writer_init(&req_w, req_buf, sizeof(req_buf));

    // RequestHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0}));  // auth token
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 100));                       // timestamp
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 1));                        // handle
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // return diagnostics
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&req_w, &(mu_string_t){-1, NULL})); // audit entry id
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 0));                        // timeout
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 0})); // additionalHeader
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0));                         // encoding mask

    // HistoryUpdateRequest with 2 details
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 2));

    // 1. UpdateDataDetails
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(
                                          &req_w, &(mu_nodeid_t){.namespace_index = 0,
                                                                 .identifier_type = MU_NODEID_NUMERIC,
                                                                 .identifier.numeric =
                                                                     MU_ID_UPDATEDATADETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0x01)); // encoding mask = ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 38));  // length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 2,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&req_w, 2)); // performInsertReplace = Replace
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 1));  // 1 value
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0x0F));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, MU_TYPE_INT32));        // Variant type
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 99));                  // Variant value
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_statuscode(&req_w, MU_STATUS_GOOD)); // status
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 1000));                // source timestamp
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 2000));                // server timestamp

    // 2. DeleteRawModifiedDetails
    TEST_ASSERT_EQUAL(
        MU_STATUS_GOOD,
        mu_binary_write_nodeid(
            &req_w, &(mu_nodeid_t){.namespace_index = 0,
                                   .identifier_type = MU_NODEID_NUMERIC,
                                   .identifier.numeric = MU_ID_DELETERAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&req_w, 0x01)); // encoding mask = ByteString
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&req_w, 21));  // length
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_binary_write_nodeid(&req_w, &(mu_nodeid_t){.namespace_index = 2,
                                                                    .identifier_type = MU_NODEID_NUMERIC,
                                                                    .identifier.numeric = 1001}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_boolean(&req_w, true)); // isDeleteModified
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 1000));   // startTime
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(&req_w, 2000));   // endTime

    opcua_byte_t res_buf[512];
    mu_binary_writer_t res_w;
    mu_binary_writer_init(&res_w, res_buf, sizeof(res_buf));

    mu_binary_reader_t req_r;
    mu_binary_reader_init(&req_r, req_buf, req_w.position);

    size_t response_length = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, handle_history_update(&server, &req_r, &res_w, &response_length));
    TEST_ASSERT_GREATER_THAN(0, response_length);

    // Verify response
    mu_binary_reader_t res_r;
    mu_binary_reader_init(&res_r, res_buf, response_length);

    // Skip response header
    mu_nodeid_t res_node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &res_node_id));
    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&res_r, &timestamp));
    opcua_uint32_t handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&res_r, &handle));
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &diag_mask));
    opcua_int32_t string_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &string_table_len));
    mu_nodeid_t add_hdr_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&res_r, &add_hdr_id));
    opcua_byte_t add_hdr_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&res_r, &add_hdr_mask));

    // Results array (2 items)
    opcua_int32_t num_res;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_res));
    TEST_ASSERT_EQUAL(2, num_res);

    // Result 1
    opcua_statuscode_t sc1;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &sc1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sc1);
    opcua_int32_t num_op_res1;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_op_res1));
    TEST_ASSERT_EQUAL(1, num_op_res1);
    opcua_statuscode_t op_sc1;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &op_sc1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, op_sc1);
    opcua_int32_t detail_diag1;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &detail_diag1));
    TEST_ASSERT_EQUAL(-1, detail_diag1);

    // Result 2
    opcua_statuscode_t sc2;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&res_r, &sc2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sc2);
    opcua_int32_t num_op_res2;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &num_op_res2));
    TEST_ASSERT_EQUAL(-1, num_op_res2); // empty/null
    opcua_int32_t detail_diag2;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &detail_diag2));
    TEST_ASSERT_EQUAL(-1, detail_diag2);

    // Outer DiagnosticInfo array
    opcua_int32_t outer_diag;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&res_r, &outer_diag));
    TEST_ASSERT_EQUAL(-1, outer_diag);
#endif
}

#endif // MUC_OPCUA_SERVICE_HISTORY

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_SERVICE_HISTORY
    RUN_TEST(test_history_read_decode);
    RUN_TEST(test_history_read_details_extension_object_truncated_body_returns_decoding_error);
    RUN_TEST(test_history_read_decode_continuation_point_is_owned_from_request_buffer);
    RUN_TEST(test_history_read_decode_continuation_point_stable_after_later_request_reuses_buffer);
    RUN_TEST(test_history_read_encode);
    RUN_TEST(test_history_read_dispatch);
    RUN_TEST(test_history_read_pagination);
    RUN_TEST(test_history_update_decode);
    RUN_TEST(test_history_update_decode_rejects_negative_length);
    RUN_TEST(test_history_update_dispatch);
#endif
    return UNITY_END();
}
