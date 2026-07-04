/* tests/unit/test_aggregate.c */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/service_header.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD

static size_t aggregate_filter_body_length(opcua_uint32_t aggregate_type) {
    size_t nodeid_length = 7u;
    if (aggregate_type <= 255u) {
        nodeid_length = 2u;
    } else if (aggregate_type <= 65535u) {
        nodeid_length = 4u;
    }
    return 8u + nodeid_length + 8u + 5u;
}

static opcua_datetime_t fake_time(void *c) {
    (void)c;
    return 0;
}

static opcua_uint64_t g_tick_ms = 0;
static opcua_uint64_t fake_tick_ms(void *c) {
    (void)c;
    return g_tick_ms;
}

/* Define some mock variables for our address space */
static const mu_reference_t s_empty_refs[1] = {{0}};
static const mu_value_source_t s_numeric_value = {MU_VALUESOURCE_STATIC,
                                                  {.static_value = {MU_TYPE_FLOAT, {.f = 10.0f}}}};
static const mu_value_source_t s_string_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_STRING, {.str = {5, (const opcua_byte_t *)"hello"}}}}};

static mu_node_t s_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                               MU_NODECLASS_OBJECT,
                               {7, (const opcua_byte_t *)"Objects"},
                               {7, (const opcua_byte_t *)"Objects"},
                               s_empty_refs,
                               0,
                               NULL},
                              {{1, MU_NODEID_NUMERIC, {1000}},
                               MU_NODECLASS_VARIABLE,
                               {10, (const opcua_byte_t *)"NumericVar"},
                               {10, (const opcua_byte_t *)"NumericVar"},
                               s_empty_refs,
                               0,
                               &s_numeric_value},
                              {{1, MU_NODEID_NUMERIC, {1001}},
                               MU_NODECLASS_VARIABLE,
                               {9, (const opcua_byte_t *)"StringVar"},
                               {9, (const opcua_byte_t *)"StringVar"},
                               s_empty_refs,
                               0,
                               &s_string_value}};
static const mu_address_space_t s_address_space = {s_nodes, 3};

static void activated_server(mu_server_t *server) {
    (void)memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    server->config.time_adapter.get_time = fake_time;
    server->config.address_space = &s_address_space;
    server->config.time_adapter.get_tick_ms = fake_tick_ms;
    g_tick_ms = 0;
    mu_subscriptions_init(&server->subs);
    mu_session_init(&server->sessions[0]);
    opcua_uint64_t rev;
    opcua_uint32_t sid, tok;
    mu_session_create(&server->sessions[0], 0, &rev, &sid, &tok);
    mu_session_activate(&server->sessions[0], tok, 321);
    server->active_session = &server->sessions[0];
}

/* Helper to write a request header */
static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t auth_id = {0, MU_NODEID_NUMERIC, {12345}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(w, &auth_id);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_str);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0);
}

/* Helper to write a response header */
static void skip_response_header(mu_binary_reader_t *r, opcua_uint32_t *handle, opcua_statuscode_t *result) {
    opcua_int64_t ts;
    opcua_byte_t diag, enc;
    opcua_int32_t strtbl;
    mu_nodeid_t addl;
    mu_binary_read_int64(r, &ts);
    mu_binary_read_uint32(r, handle);
    mu_binary_read_statuscode(r, result);
    mu_binary_read_byte(r, &diag);
    mu_binary_read_int32(r, &strtbl);
    mu_binary_read_nodeid(r, &addl);
    mu_binary_read_byte(r, &enc);
}

static opcua_uint32_t create_subscription(mu_server_t *server) {
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 21);
    mu_binary_write_double(&w, 1000.0);
    mu_binary_write_uint32(&w, 30);
    mu_binary_write_uint32(&w, 10);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0);

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(server, MU_ID_CREATESUBSCRIPTIONREQUEST, req, w.position, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_CREATESUBSCRIPTIONRESPONSE, type.identifier.numeric);

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&r, &sub_id);
    return sub_id;
}

static opcua_statuscode_t create_aggregate_item_result(mu_server_t *server, opcua_uint32_t sub_id,
                                                       opcua_uint32_t aggregate_type) {
    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_int32(&w, 1);

    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &node_id);
    mu_binary_write_uint32(&w, 13);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint16(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, 42);
    mu_binary_write_double(&w, 100.0);

    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY}};
    mu_binary_write_extension_object_header(&w, &filter_type, aggregate_filter_body_length(aggregate_type));

    mu_binary_write_int64(&w, 0);
    mu_nodeid_t agg_type = {0, MU_NODEID_NUMERIC, {aggregate_type}};
    mu_binary_write_nodeid(&w, &agg_type);
    mu_binary_write_double(&w, 5000.0);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_boolean(&w, false);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_boolean(&w, false);

    mu_binary_write_uint32(&w, 1);
    mu_binary_write_boolean(&w, true);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(server, MU_ID_CREATEMONITOREDITEMSREQUEST, req, w.position, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    opcua_int32_t results_count;
    mu_binary_read_int32(&r, &results_count);
    TEST_ASSERT_EQUAL(1, results_count);

    opcua_statuscode_t item_result;
    mu_binary_read_statuscode(&r, &item_result);
    return item_result;
}

void test_aggregate_standard_nodeids_match_opcua_nodeset(void) {
    /* OPC-10000-4 §7.22.4 AggregateFilter; OPC-10000-6 §5.2.2.9 NodeId;
       OPC-10000-6 §5.2.2.15 ExtensionObject; OPC-10000-13 §4.2.2.4,
       §4.2.2.9, and §4.2.2.10 AggregateFunction objects. Numeric values are
       from OPC Foundation UA NodeSet NodeIds.csv. */
    TEST_ASSERT_EQUAL_UINT32(730u, MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY);
    TEST_ASSERT_EQUAL_UINT32(2342u, MU_ID_AGGREGATETYPE_AVERAGE);
    TEST_ASSERT_EQUAL_UINT32(2346u, MU_ID_AGGREGATETYPE_MINIMUM);
    TEST_ASSERT_EQUAL_UINT32(2347u, MU_ID_AGGREGATETYPE_MAXIMUM);
}

void test_aggregate_filter_rejects_stale_operation_limits_nodeids(void) {
    /* OPC-10000-13 aggregate functions are ns=0;i=2342/2346/2347. The stale
       11565/11569/11570 values are OperationLimits variables, not aggregate
       functions, and must be rejected by OPC-10000-4 §5.13.2.4 filter rules. */
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED,
                            create_aggregate_item_result(&server, sub_id, 11565u));
    TEST_ASSERT_FALSE(server.subs.monitored_items[0].in_use);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED,
                            create_aggregate_item_result(&server, sub_id, 11569u));
    TEST_ASSERT_FALSE(server.subs.monitored_items[0].in_use);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED,
                            create_aggregate_item_result(&server, sub_id, 11570u));
    TEST_ASSERT_FALSE(server.subs.monitored_items[0].in_use);
}

void test_aggregate_filter_decodes_correctly(void) {
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    /* Construct CreateMonitoredItemsRequest with AggregateFilter */
    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0); /* timestampsToReturn */
    mu_binary_write_int32(&w, 1);  /* itemsToCreate array length */

    /* MonitoredItemCreateRequest */
    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &node_id);
    mu_binary_write_uint32(&w, 13); /* attributeId = Value */
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str); /* indexRange */
    mu_binary_write_uint16(&w, 0);         /* dataEncoding namespace */
    mu_binary_write_string(&w, &null_str); /* dataEncoding name */
    mu_binary_write_uint32(&w, 2);         /* monitoringMode = Reporting */
    mu_binary_write_uint32(&w, 42);        /* clientHandle */
    mu_binary_write_double(&w, 100.0);     /* requestedSamplingInterval */

    /* ExtensionObject filter: AggregateFilter */
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY}};
    mu_binary_write_extension_object_header(&w, &filter_type,
                                            aggregate_filter_body_length(MU_ID_AGGREGATETYPE_AVERAGE));

    /* AggregateFilter Body */
    mu_binary_write_int64(&w, 0); /* startTime */
    mu_nodeid_t agg_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATETYPE_AVERAGE}};
    mu_binary_write_nodeid(&w, &agg_type); /* aggregateType */
    mu_binary_write_double(&w, 5000.0);    /* processingInterval */
    /* AggregateConfiguration */
    mu_binary_write_boolean(&w, true);  /* useServerDefaults */
    mu_binary_write_boolean(&w, false); /* treatUncertainAsBad */
    mu_binary_write_byte(&w, 0);        /* percentDataBad */
    mu_binary_write_byte(&w, 0);        /* percentDataGood */
    mu_binary_write_boolean(&w, false); /* useSlopedExtrapolation */

    /* queueSize, discardOldest */
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_boolean(&w, true);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, req, w.position,
                                                          resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_CREATEMONITOREDITEMSRESPONSE, type.identifier.numeric);

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    opcua_int32_t results_count;
    mu_binary_read_int32(&r, &results_count);
    TEST_ASSERT_EQUAL(1, results_count);

    opcua_statuscode_t item_result;
    mu_binary_read_statuscode(&r, &item_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, item_result);

    /* Verify internal state */
    mu_monitored_item_t *item = &server.subs.monitored_items[0];
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_TRUE(item->has_aggregate);
    TEST_ASSERT_EQUAL(MU_ID_AGGREGATETYPE_AVERAGE, item->aggregate_state.aggregate_type);
    TEST_ASSERT_TRUE(item->aggregate_state.processing_interval == 5000.0);
}

void test_aggregate_filter_fails_on_non_numeric(void) {
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_int32(&w, 1);

    /* MonitoredItemCreateRequest pointing to String node */
    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1001}};
    mu_binary_write_nodeid(&w, &node_id);
    mu_binary_write_uint32(&w, 13);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint16(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, 42);
    mu_binary_write_double(&w, 100.0);

    /* ExtensionObject filter: AggregateFilter */
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY}};
    mu_binary_write_extension_object_header(&w, &filter_type,
                                            aggregate_filter_body_length(MU_ID_AGGREGATETYPE_AVERAGE));

    /* AggregateFilter Body */
    mu_binary_write_int64(&w, 0);
    mu_nodeid_t agg_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATETYPE_AVERAGE}};
    mu_binary_write_nodeid(&w, &agg_type);
    mu_binary_write_double(&w, 5000.0);
    /* AggregateConfiguration */
    mu_binary_write_boolean(&w, true);
    mu_binary_write_boolean(&w, false);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_boolean(&w, false);

    mu_binary_write_uint32(&w, 1);
    mu_binary_write_boolean(&w, true);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, req, w.position,
                                                          resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);

    opcua_int32_t results_count;
    mu_binary_read_int32(&r, &results_count);
    TEST_ASSERT_EQUAL(1, results_count);

    opcua_statuscode_t item_result;
    mu_binary_read_statuscode(&r, &item_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_FILTERNOTALLOWED, item_result);
}

void test_aggregate_filter_fails_on_unsupported_aggregate_type(void) {
    /* OPC-10000-4 §7.22.4: AggregateFilter selects the AggregateFunction used
       for returned values. OPC-10000-13 §4.2.2.5 and §5.4.3.6 define
       TimeAverage as a standard AggregateFunction. OPC-10000-13 §4.2.2.4,
       §4.2.2.9, and §4.2.2.10 scope this build to Average, Minimum, and
       Maximum, so TimeAverage is rejected with the per-item
       Bad_MonitoredItemFilterUnsupported result. */
    const opcua_uint32_t time_average_aggregate_type = 2343u;
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_int32(&w, 1);

    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &node_id);
    mu_binary_write_uint32(&w, 13);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint16(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, 42);
    mu_binary_write_double(&w, 100.0);

    /* ExtensionObject filter: AggregateFilter */
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY}};
    mu_binary_write_extension_object_header(&w, &filter_type,
                                            aggregate_filter_body_length(time_average_aggregate_type));

    /* AggregateFilter Body with unsupported standard TimeAverage type */
    mu_binary_write_int64(&w, 0);
    mu_nodeid_t agg_type = {0, MU_NODEID_NUMERIC, {time_average_aggregate_type}};
    mu_binary_write_nodeid(&w, &agg_type);
    mu_binary_write_double(&w, 5000.0);
    /* AggregateConfiguration */
    mu_binary_write_boolean(&w, true);
    mu_binary_write_boolean(&w, false);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_boolean(&w, false);

    mu_binary_write_uint32(&w, 1);
    mu_binary_write_boolean(&w, true);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, req, w.position,
                                                          resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);

    opcua_int32_t results_count;
    mu_binary_read_int32(&r, &results_count);
    TEST_ASSERT_EQUAL(1, results_count);

    opcua_statuscode_t item_result;
    mu_binary_read_statuscode(&r, &item_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED, item_result);
    TEST_ASSERT_EQUAL_size_t(0u, server.subs.active_monitored_items_count);
    TEST_ASSERT_FALSE(server.subs.monitored_items[0].in_use);
}

void test_aggregate_filter_fails_on_truncated_extension_object_body(void) {
    /* OPC-10000-4 §7.22.4 defines AggregateFilter as a MonitoringFilter.
       OPC-10000-6 §5.2.2.15 requires the ExtensionObject ByteString length to
       bound the complete encoded body; a truncated body is a decoding error. */
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_int32(&w, 1);

    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &node_id);
    mu_binary_write_uint32(&w, 13);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint16(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, 42);
    mu_binary_write_double(&w, 100.0);

    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY}};
    mu_binary_write_extension_object_header(&w, &filter_type,
                                            aggregate_filter_body_length(MU_ID_AGGREGATETYPE_AVERAGE));

    /* Malformed AggregateFilter body: only startTime is present. */
    mu_binary_write_int64(&w, 0);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST,
                                                                             req, w.position, resp, &resp_len));
    TEST_ASSERT_EQUAL_size_t(0u, server.subs.active_monitored_items_count);
    TEST_ASSERT_FALSE(server.subs.monitored_items[0].in_use);
}

void test_aggregate_filter_fails_on_invalid_interval(void) {
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_int32(&w, 1);

    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &node_id);
    mu_binary_write_uint32(&w, 13);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint16(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, 42);
    mu_binary_write_double(&w, 100.0);

    /* ExtensionObject filter: AggregateFilter */
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY}};
    mu_binary_write_extension_object_header(&w, &filter_type,
                                            aggregate_filter_body_length(MU_ID_AGGREGATETYPE_AVERAGE));

    /* AggregateFilter Body with 0.0 processing interval */
    mu_binary_write_int64(&w, 0);
    mu_nodeid_t agg_type = {0, MU_NODEID_NUMERIC, {MU_ID_AGGREGATETYPE_AVERAGE}};
    mu_binary_write_nodeid(&w, &agg_type);
    mu_binary_write_double(&w, 0.0);
    /* AggregateConfiguration */
    mu_binary_write_boolean(&w, true);
    mu_binary_write_boolean(&w, false);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_byte(&w, 0);
    mu_binary_write_boolean(&w, false);

    mu_binary_write_uint32(&w, 1);
    mu_binary_write_boolean(&w, true);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, req, w.position,
                                                          resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);

    opcua_int32_t results_count;
    mu_binary_read_int32(&r, &results_count);

    opcua_statuscode_t item_result;
    mu_binary_read_statuscode(&r, &item_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_FILTERNOTALLOWED, item_result);
}

void test_aggregate_filter_average_scoped_support(void) {
    /* OPC-10000-4 §7.22.4: AggregateFilter selects the AggregateFunction used
       for returned values. OPC-10000-13 §5.4.3.5: this scoped Average support
       publishes sum(Good raw values) / Good raw sample count for the interval. */
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, create_aggregate_item_result(&server, sub_id, MU_ID_AGGREGATETYPE_AVERAGE));

    mu_monitored_item_t *item = &server.subs.monitored_items[0];
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_TRUE(item->has_aggregate);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_AGGREGATETYPE_AVERAGE, item->aggregate_state.aggregate_type);

    item->aggregate_state.processing_interval = 100.0;
    item->aggregate_state.last_calculation = 0;
    item->aggregate_state.sample_count = 0;
    (void)memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
    item->sampling_interval_ms = 10;
    item->next_sample_ms = 10;
    item->queue_head = 0;
    item->queue_tail = 0;
    item->queue_count = 0;
    item->pending = false;

    /* Mock value updates and ticks */
    mu_value_source_t val1 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 10.0f}}}};
    mu_value_source_t val2 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 20.0f}}}};
    mu_value_source_t val3 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 30.0f}}}};

    /* Tick 1: 10ms - sample 1 */
    g_tick_ms = 10;
    ((mu_node_t *)&s_nodes[1])->value = &val1;
    mu_subscriptions_tick(&server, g_tick_ms);
    TEST_ASSERT_EQUAL(1, server.subs.monitored_items[0].aggregate_state.sample_count);

    /* Tick 2: 20ms - sample 2 */
    g_tick_ms = 20;
    ((mu_node_t *)&s_nodes[1])->value = &val2;
    mu_subscriptions_tick(&server, g_tick_ms);
    TEST_ASSERT_EQUAL(2, server.subs.monitored_items[0].aggregate_state.sample_count);

    /* Tick 3: 30ms - sample 3 */
    g_tick_ms = 30;
    ((mu_node_t *)&s_nodes[1])->value = &val3;
    mu_subscriptions_tick(&server, g_tick_ms);
    TEST_ASSERT_EQUAL(3, server.subs.monitored_items[0].aggregate_state.sample_count);

    /* Tick 4: 100ms - aggregate interval expires */
    g_tick_ms = 100;
    mu_subscriptions_tick(&server, g_tick_ms);

    /* Verify that average (20.0) is enqueued in monitored item queue */
    mu_monitored_item_t *res_item = &server.subs.monitored_items[0];
    TEST_ASSERT_EQUAL(1, res_item->aggregate_state.sample_count); /* 1 sample taken at 100ms for next interval */
    TEST_ASSERT_EQUAL(1, res_item->queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_DOUBLE, res_item->queue[0].value.type);
    TEST_ASSERT_TRUE(res_item->queue[0].value.value.d == 20.0);
}

void test_aggregate_filter_minimum_scoped_support(void) {
    /* OPC-10000-4 §7.22.4: AggregateFilter selects the AggregateFunction used
       for returned values. OPC-10000-13 §5.4.3.10: this scoped Minimum support
       publishes the minimum Good raw value sampled within the interval. */
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, create_aggregate_item_result(&server, sub_id, MU_ID_AGGREGATETYPE_MINIMUM));

    mu_monitored_item_t *item = &server.subs.monitored_items[0];
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_TRUE(item->has_aggregate);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_AGGREGATETYPE_MINIMUM, item->aggregate_state.aggregate_type);

    item->aggregate_state.processing_interval = 100.0;
    item->aggregate_state.last_calculation = 0;
    item->aggregate_state.sample_count = 0;
    memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
    item->sampling_interval_ms = 10;
    item->next_sample_ms = 10;
    item->queue_head = 0;
    item->queue_tail = 0;
    item->queue_count = 0;
    item->pending = false;

    mu_value_source_t val1 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 15.0f}}}};
    mu_value_source_t val2 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 5.0f}}}};
    mu_value_source_t val3 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 30.0f}}}};

    g_tick_ms = 10;
    ((mu_node_t *)&s_nodes[1])->value = &val1;
    mu_subscriptions_tick(&server, g_tick_ms);

    g_tick_ms = 20;
    ((mu_node_t *)&s_nodes[1])->value = &val2;
    mu_subscriptions_tick(&server, g_tick_ms);

    g_tick_ms = 30;
    ((mu_node_t *)&s_nodes[1])->value = &val3;
    mu_subscriptions_tick(&server, g_tick_ms);

    g_tick_ms = 100;
    mu_subscriptions_tick(&server, g_tick_ms);

    mu_monitored_item_t *res_item = &server.subs.monitored_items[0];
    TEST_ASSERT_EQUAL(1, res_item->queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, res_item->queue[0].value.type);
    TEST_ASSERT_TRUE(res_item->queue[0].value.value.f == 5.0f);
}

void test_aggregate_filter_maximum_scoped_support(void) {
    /* OPC-10000-4 §7.22.4: AggregateFilter selects the AggregateFunction used
       for returned values. OPC-10000-13 §5.4.3.11: this scoped Maximum support
       publishes the maximum Good raw value sampled within the interval. */
    mu_server_t server;
    activated_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, create_aggregate_item_result(&server, sub_id, MU_ID_AGGREGATETYPE_MAXIMUM));

    mu_monitored_item_t *item = &server.subs.monitored_items[0];
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_TRUE(item->has_aggregate);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_AGGREGATETYPE_MAXIMUM, item->aggregate_state.aggregate_type);

    item->aggregate_state.processing_interval = 100.0;
    item->aggregate_state.last_calculation = 0;
    item->aggregate_state.sample_count = 0;
    memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
    item->sampling_interval_ms = 10;
    item->next_sample_ms = 10;
    item->queue_head = 0;
    item->queue_tail = 0;
    item->queue_count = 0;
    item->pending = false;

    mu_value_source_t val1 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 15.0f}}}};
    mu_value_source_t val2 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 5.0f}}}};
    mu_value_source_t val3 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 30.0f}}}};

    g_tick_ms = 10;
    ((mu_node_t *)&s_nodes[1])->value = &val1;
    mu_subscriptions_tick(&server, g_tick_ms);

    g_tick_ms = 20;
    ((mu_node_t *)&s_nodes[1])->value = &val2;
    mu_subscriptions_tick(&server, g_tick_ms);

    g_tick_ms = 30;
    ((mu_node_t *)&s_nodes[1])->value = &val3;
    mu_subscriptions_tick(&server, g_tick_ms);

    g_tick_ms = 100;
    mu_subscriptions_tick(&server, g_tick_ms);

    mu_monitored_item_t *res_item = &server.subs.monitored_items[0];
    TEST_ASSERT_EQUAL(1, res_item->queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, res_item->queue[0].value.type);
    TEST_ASSERT_TRUE(res_item->queue[0].value.value.f == 30.0f);
}

void test_aggregate_min_max_calculations(void) {
    mu_server_t server;
    activated_server(&server);

    /* Test Min Aggregate */
    mu_monitored_item_t item_min;
    (void)memset(&item_min, 0, sizeof(item_min));
    item_min.in_use = true;
    item_min.has_aggregate = true;
    item_min.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_MINIMUM;
    item_min.aggregate_state.processing_interval = 100.0;
    item_min.aggregate_state.last_calculation = 0;
    item_min.aggregate_state.sample_count = 0;
    item_min.sampling_interval_ms = 10;
    item_min.next_sample_ms = 10;
    item_min.resolved_node = &s_nodes[1];
    item_min.monitoring_mode = MU_MONITORING_MODE_REPORTING;

    /* Test Max Aggregate */
    mu_monitored_item_t item_max;
    (void)memset(&item_max, 0, sizeof(item_max));
    item_max.in_use = true;
    item_max.has_aggregate = true;
    item_max.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_MAXIMUM;
    item_max.aggregate_state.processing_interval = 100.0;
    item_max.aggregate_state.last_calculation = 0;
    item_max.aggregate_state.sample_count = 0;
    item_max.sampling_interval_ms = 10;
    item_max.next_sample_ms = 10;
    item_max.resolved_node = &s_nodes[1];
    item_max.monitoring_mode = MU_MONITORING_MODE_REPORTING;

    server.subs.monitored_items[0] = item_min;
    server.subs.monitored_items[1] = item_max;
    server.subs.active_monitored_items_count = 2;

    mu_value_source_t val1 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 15.0f}}}};
    mu_value_source_t val2 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 5.0f}}}};
    mu_value_source_t val3 = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 30.0f}}}};

    /* Tick 1: 10ms - sample 1 */
    g_tick_ms = 10;
    ((mu_node_t *)&s_nodes[1])->value = &val1;
    mu_subscriptions_tick(&server, g_tick_ms);

    /* Tick 2: 20ms - sample 2 */
    g_tick_ms = 20;
    ((mu_node_t *)&s_nodes[1])->value = &val2;
    mu_subscriptions_tick(&server, g_tick_ms);

    /* Tick 3: 30ms - sample 3 */
    g_tick_ms = 30;
    ((mu_node_t *)&s_nodes[1])->value = &val3;
    mu_subscriptions_tick(&server, g_tick_ms);

    /* Tick 4: 100ms - aggregate interval expires */
    g_tick_ms = 100;
    mu_subscriptions_tick(&server, g_tick_ms);

    /* Verify min is 5.0 */
    TEST_ASSERT_EQUAL(1, server.subs.monitored_items[0].queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, server.subs.monitored_items[0].queue[0].value.type);
    TEST_ASSERT_TRUE(server.subs.monitored_items[0].queue[0].value.value.f == 5.0f);

    /* Verify max is 30.0 */
    TEST_ASSERT_EQUAL(1, server.subs.monitored_items[1].queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, server.subs.monitored_items[1].queue[0].value.type);
    TEST_ASSERT_TRUE(server.subs.monitored_items[1].queue[0].value.value.f == 30.0f);
}

void test_aggregate_no_samples_publishes_last_known(void) {
    mu_server_t server;
    activated_server(&server);

    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;
    item.aggregate_state.processing_interval = 100.0;
    item.aggregate_state.last_calculation = 0;
    item.aggregate_state.sample_count = 0;
    item.sampling_interval_ms = 10;
    item.next_sample_ms = 10;
    item.resolved_node = &s_nodes[1];
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.has_value = true;
    item.last_value.type = MU_TYPE_DOUBLE;
    item.last_value.value.d = 42.0;

    server.subs.monitored_items[0] = item;
    server.subs.active_monitored_items_count = 1;

    /* Tick to 100ms without any samples being processed */
    g_tick_ms = 100;
    mu_subscriptions_tick(&server, g_tick_ms);

    /* Verify that last known value (42.0) is published as average */
    TEST_ASSERT_EQUAL(1, server.subs.monitored_items[0].queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_DOUBLE, server.subs.monitored_items[0].queue[0].value.type);
    TEST_ASSERT_TRUE(server.subs.monitored_items[0].queue[0].value.value.d == 42.0);
}

void test_aggregate_zero_heap_containment(void) {
    /* Verify memory size constraints (relaxed to 80 on 64-bit hosts due to pointer size) */
    TEST_ASSERT_TRUE(sizeof(mu_aggregate_state_t) <= (sizeof(void *) == 8 ? 80 : 64));
}

#else

void test_aggregate_requires_standard_subscriptions(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS_STANDARD is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_aggregate_standard_nodeids_match_opcua_nodeset);
    RUN_TEST(test_aggregate_filter_rejects_stale_operation_limits_nodeids);
    RUN_TEST(test_aggregate_filter_decodes_correctly);
    RUN_TEST(test_aggregate_filter_fails_on_non_numeric);
    RUN_TEST(test_aggregate_filter_fails_on_unsupported_aggregate_type);
    RUN_TEST(test_aggregate_filter_fails_on_truncated_extension_object_body);
    RUN_TEST(test_aggregate_filter_fails_on_invalid_interval);
    RUN_TEST(test_aggregate_filter_average_scoped_support);
    RUN_TEST(test_aggregate_filter_minimum_scoped_support);
    RUN_TEST(test_aggregate_filter_maximum_scoped_support);
    RUN_TEST(test_aggregate_min_max_calculations);
    RUN_TEST(test_aggregate_no_samples_publishes_last_known);
    RUN_TEST(test_aggregate_zero_heap_containment);
#else
    RUN_TEST(test_aggregate_requires_standard_subscriptions);
#endif
    return UNITY_END();
}
