/* tests/unit/test_browse_service.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/browse.h"

opcua_statuscode_t handle_browse_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length);
opcua_statuscode_t handle_browse(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                 size_t *response_length);
opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);

static void write_test_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t auth_token = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
    mu_string_t null_string = {-1, NULL};
    mu_nodeid_t null_nodeid = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};

    mu_binary_write_nodeid(writer, &auth_token);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, request_handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &null_nodeid, 0);
}

static void skip_response_header(mu_binary_reader_t *reader) {
    mu_nodeid_t response_type;
    opcua_int64_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_byte_t service_diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    mu_binary_read_nodeid(reader, &response_type);
    TEST_ASSERT_EQUAL(MU_ID_BROWSENEXTRESPONSE, response_type.identifier.numeric);
    mu_binary_read_int64(reader, &timestamp);
    mu_binary_read_uint32(reader, &request_handle);
    mu_binary_read_statuscode(reader, &service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);
    mu_binary_read_byte(reader, &service_diagnostics_mask);
    TEST_ASSERT_EQUAL(0, service_diagnostics_mask);
    mu_binary_read_int32(reader, &string_table_count);
    TEST_ASSERT_EQUAL(-1, string_table_count);
    mu_binary_read_nodeid(reader, &additional_header_type);
    mu_binary_read_byte(reader, &additional_header_encoding);
    TEST_ASSERT_EQUAL(0, additional_header_encoding);
}

static void skip_translate_browse_paths_response_header(mu_binary_reader_t *reader) {
    mu_nodeid_t response_type;
    opcua_int64_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_byte_t service_diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    mu_binary_read_nodeid(reader, &response_type);
    TEST_ASSERT_EQUAL(MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, response_type.identifier.numeric);
    mu_binary_read_int64(reader, &timestamp);
    mu_binary_read_uint32(reader, &request_handle);
    mu_binary_read_statuscode(reader, &service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);
    mu_binary_read_byte(reader, &service_diagnostics_mask);
    TEST_ASSERT_EQUAL(0, service_diagnostics_mask);
    mu_binary_read_int32(reader, &string_table_count);
    TEST_ASSERT_EQUAL(-1, string_table_count);
    mu_binary_read_nodeid(reader, &additional_header_type);
    mu_binary_read_byte(reader, &additional_header_encoding);
    TEST_ASSERT_EQUAL(0, additional_header_encoding);
}

/* T013 / SCN-001 / CASE-002 / opc_cu_2317 / OPC-10000-4 §5.9.4:
   Objects(85) -HierarchicalReferences/Organizes-> MyVar1(ns=1;i=1000). */
static const mu_reference_t translate_obj_refs[] = {
    {{.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 35},
     {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1000},
     true}};
static const mu_reference_t translate_var_refs[] = {
    {{.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 35},
     {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85},
     false}};
static const mu_value_source_t translate_var_value = {MU_VALUESOURCE_STATIC,
                                                      {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
static const mu_node_t translate_nodes[] = {
    {{.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85},
     MU_NODECLASS_OBJECT,
     {7, (const opcua_byte_t *)"Objects"},
     {7, (const opcua_byte_t *)"Objects"},
     translate_obj_refs,
     1,
     NULL},
    {{.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1000},
     MU_NODECLASS_VARIABLE,
     {6, (const opcua_byte_t *)"MyVar1"},
     {6, (const opcua_byte_t *)"MyVar1"},
     translate_var_refs,
     1,
     &translate_var_value}};
static const mu_address_space_t translate_space = {translate_nodes, 2};

static opcua_statuscode_t translate_stub_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t translate_stub_accept(void *context, void **client_handle) {
    (void)context;
    *client_handle = NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t translate_stub_read(void *context, void *client_handle, opcua_byte_t *buffer, size_t capacity,
                                              size_t *bytes_read) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    (void)capacity;
    *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t translate_stub_write(void *context, void *client_handle, const opcua_byte_t *buffer,
                                               size_t length, size_t *bytes_written) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    *bytes_written = length;
    return MU_STATUS_GOOD;
}

static void translate_stub_close(void *context, void *client_handle) {
    (void)context;
    (void)client_handle;
}

static void translate_stub_shutdown(void *context) {
    (void)context;
}

static opcua_datetime_t translate_stub_time(void *context) {
    (void)context;
    return 0;
}

static opcua_uint64_t translate_stub_tick_ms(void *context) {
    (void)context;
    return 0;
}

static opcua_statuscode_t translate_stub_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    (void)memset(buffer, 0, length);
    return MU_STATUS_GOOD;
}

static mu_server_t *init_translate_test_server(void *storage, size_t storage_size) {
    static opcua_byte_t receive_buffer[MU_MIN_CHUNK_SIZE];
    static opcua_byte_t send_buffer[MU_MIN_CHUNK_SIZE];
    mu_server_config_t config;
    mu_server_t *server = NULL;

    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://unit:4840";
    config.application_uri = "urn:unit";
    config.product_uri = "urn:unit";
    config.application_name = "unit";
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);
    config.max_chunk_count = 1;
    config.max_message_size = MU_MIN_CHUNK_SIZE;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.tcp_adapter.listen = translate_stub_listen;
    config.tcp_adapter.accept = translate_stub_accept;
    config.tcp_adapter.read = translate_stub_read;
    config.tcp_adapter.write = translate_stub_write;
    config.tcp_adapter.close_connection = translate_stub_close;
    config.tcp_adapter.shutdown = translate_stub_shutdown;
    config.time_adapter.get_time = translate_stub_time;
    config.time_adapter.get_tick_ms = translate_stub_tick_ms;
    config.entropy_adapter.generate_random = translate_stub_random;
    config.address_space = &translate_space;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, storage_size, &config, &server));
    return server;
}

static void write_translate_browse_path(mu_binary_writer_t *writer, const char *target_name) {
    mu_nodeid_t starting_node = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85};
    mu_nodeid_t hierarchical_references = {
        .identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 33};
    mu_string_t target = {(opcua_int32_t)strlen(target_name), (const opcua_byte_t *)target_name};

    mu_binary_write_nodeid(writer, &starting_node);
    mu_binary_write_int32(writer, 1);
    mu_binary_write_nodeid(writer, &hierarchical_references);
    mu_binary_write_boolean(writer, false);
    mu_binary_write_boolean(writer, true);
    mu_binary_write_uint16(writer, 0);
    mu_binary_write_string(writer, &target);
}

void test_translate_browse_paths_scn_001_case_002_resolves_objects_to_variable(void) {
    _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[512];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    mu_binary_reader_t response_reader;
    size_t response_length = 0;
    mu_server_t *server = init_translate_test_server(server_storage, sizeof(server_storage));

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 14);
    mu_binary_write_int32(&request_writer, 1);
    write_translate_browse_path(&request_writer, "MyVar1");

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      handle_translate_browse_paths(server, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(response_writer.position, response_length);

    mu_binary_reader_init(&response_reader, response_buffer, response_length);
    skip_translate_browse_paths_response_header(&response_reader);

    opcua_int32_t result_count;
    mu_binary_read_int32(&response_reader, &result_count);
    TEST_ASSERT_EQUAL(1, result_count);

    opcua_statuscode_t status;
    mu_binary_read_statuscode(&response_reader, &status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

    opcua_int32_t target_count;
    mu_binary_read_int32(&response_reader, &target_count);
    TEST_ASSERT_EQUAL(1, target_count);

    mu_nodeid_t target;
    mu_binary_read_nodeid(&response_reader, &target);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, target.identifier_type);
    TEST_ASSERT_EQUAL(1, target.namespace_index);
    TEST_ASSERT_EQUAL(1000, target.identifier.numeric);

    opcua_uint32_t remaining_path_index;
    mu_binary_read_uint32(&response_reader, &remaining_path_index);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFFu, remaining_path_index);

    opcua_int32_t diagnostic_info_count;
    mu_binary_read_int32(&response_reader, &diagnostic_info_count);
    TEST_ASSERT_EQUAL(0, diagnostic_info_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, response_reader.status);
    TEST_ASSERT_EQUAL(response_length, response_reader.position);
}

void test_translate_browse_paths_scn_001_case_002_wrong_target_name_returns_no_match(void) {
    _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[512];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    mu_binary_reader_t response_reader;
    size_t response_length = 0;
    mu_server_t *server = init_translate_test_server(server_storage, sizeof(server_storage));

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 15);
    mu_binary_write_int32(&request_writer, 1);
    write_translate_browse_path(&request_writer, "Nope");

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      handle_translate_browse_paths(server, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(response_writer.position, response_length);

    mu_binary_reader_init(&response_reader, response_buffer, response_length);
    skip_translate_browse_paths_response_header(&response_reader);

    opcua_int32_t result_count;
    mu_binary_read_int32(&response_reader, &result_count);
    TEST_ASSERT_EQUAL(1, result_count);

    opcua_statuscode_t status;
    mu_binary_read_statuscode(&response_reader, &status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOMATCH, status);

    opcua_int32_t target_count;
    mu_binary_read_int32(&response_reader, &target_count);
    TEST_ASSERT_EQUAL(0, target_count);

    opcua_int32_t diagnostic_info_count;
    mu_binary_read_int32(&response_reader, &diagnostic_info_count);
    TEST_ASSERT_EQUAL(0, diagnostic_info_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, response_reader.status);
    TEST_ASSERT_EQUAL(response_length, response_reader.position);
}

void test_translate_browse_paths_malformed_negative_browse_path_count_rejects_without_body(void) {
    _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[64];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    size_t response_length = 0;
    mu_server_t *server = init_translate_test_server(server_storage, sizeof(server_storage));

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 18);
    mu_binary_write_int32(&request_writer, -2);

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            handle_translate_browse_paths(server, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(0, response_length);
    TEST_ASSERT_EQUAL(0, response_writer.position);
}

void test_translate_browse_paths_malformed_negative_relative_path_element_count_rejects_without_body(void) {
    _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[64];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    size_t response_length = 0;
    mu_nodeid_t starting_node = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85};
    mu_server_t *server = init_translate_test_server(server_storage, sizeof(server_storage));

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 19);
    mu_binary_write_int32(&request_writer, 1);
    mu_binary_write_nodeid(&request_writer, &starting_node);
    mu_binary_write_int32(&request_writer, -2);

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            handle_translate_browse_paths(server, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(0, response_length);
    TEST_ASSERT_EQUAL(0, response_writer.position);
}

void test_translate_browse_paths_excessive_relative_path_elements_rejects_without_body(void) {
    _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[64];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    size_t response_length = 0;
    mu_nodeid_t starting_node = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85};
    mu_server_t *server = init_translate_test_server(server_storage, sizeof(server_storage));

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 20);
    mu_binary_write_int32(&request_writer, 1);
    mu_binary_write_nodeid(&request_writer, &starting_node);
    mu_binary_write_int32(&request_writer, 9);

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS,
                            handle_translate_browse_paths(server, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(0, response_length);
    TEST_ASSERT_EQUAL(0, response_writer.position);
}

void test_browse_service_static_references(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    /* ViewDescription */
    mu_nodeid_t empty_nodeid = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
    mu_binary_write_nodeid(&writer, &empty_nodeid); /* empty nodeid */
    mu_binary_write_int64(&writer, 0);              /* timestamp */
    mu_binary_write_uint32(&writer, 0);             /* view version */

    /* RequestedMaxReferencesPerNode */
    mu_binary_write_uint32(&writer, 10);

    /* NoOfNodesToBrowse */
    mu_binary_write_int32(&writer, 1);

    /* NodesToBrowse[0] */
    mu_nodeid_t root_folder = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84};
    mu_nodeid_t hier_ref = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 33};
    mu_binary_write_nodeid(&writer, &root_folder); /* RootFolder */
    mu_binary_write_uint32(&writer, MU_BROWSE_DIRECTION_FORWARD);
    mu_binary_write_nodeid(&writer, &hier_ref); /* HierarchicalReferences */
    mu_binary_write_boolean(&writer, true);
    mu_binary_write_uint32(&writer, 0xFFFFFFFF); /* All classes */
    mu_binary_write_uint32(&writer, 0x3F);       /* Result mask */

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);

    mu_browse_request_t req;
    mu_browse_description_t desc[2];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_request_decode(&reader, &req, desc, 2));
    TEST_ASSERT_EQUAL(10, req.requested_max_references_per_node);
    TEST_ASSERT_EQUAL(1, req.num_nodes_to_browse);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, req.nodes_to_browse[0].node_id.identifier_type);
    TEST_ASSERT_EQUAL(84, req.nodes_to_browse[0].node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_BROWSE_DIRECTION_FORWARD, req.nodes_to_browse[0].browse_direction);
    TEST_ASSERT_TRUE(req.nodes_to_browse[0].include_subtypes);
}

void test_browse_service_response_encode(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    mu_reference_description_t ref = {
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 35},
        .is_forward = true,
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85},
        .browse_name_namespace_index = 0,
        .browse_name = {.length = 7, .data = (opcua_byte_t *)"Objects"},
        .display_name = {.length = 7, .data = (opcua_byte_t *)"Objects"},
        .node_class = 1, /* Object */
        .type_definition = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 61}};

    mu_browse_result_t res = {.status_code = MU_STATUS_GOOD, .references = &ref, .num_references = 1};

    mu_browse_response_t resp = {.results = &res, .num_results = 1};

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_response_encode(&writer, &resp));

    /* Independent binary fixture for BrowseResponse encoding
     * OPC-10000-4 §5.9.2.2 / OPC-10000-6 §5.2.2:
     *   BrowseResult: StatusCode GOOD, ContinuationPoint null (-1),
     *   1 ReferenceDescription (Organizes ns=0;i=35, forward,
     *   ObjectsFolder ns=0;i=85, BrowseName="Objects" ns=0,
     *   DisplayName="Objects", NodeClass Object=1,
     *   TypeDefinition BaseObjectType ns=0;i=61) */
    const opcua_byte_t expected[] = {
        0x01, 0x00, 0x00, 0x00,                /* Int32: num_results=1 */
        0x00, 0x00, 0x00, 0x00,                /* StatusCode: GOOD */
        0xFF, 0xFF, 0xFF, 0xFF,                /* ContinuationPoint: null (-1) */
        0x01, 0x00, 0x00, 0x00,                /* Int32: num_references=1 */
        0x00, 0x23,                            /* NodeId TwoByte: ns=0, id=35 (Organizes) */
        0x01,                                  /* Boolean: is_forward=true */
        0x00, 0x55,                            /* NodeId TwoByte: ns=0, id=85 (ObjectsFolder) */
        0x00, 0x00,                            /* UInt16: browse_name_namespace_index=0 */
        0x07, 0x00, 0x00, 0x00,                /* Int32: string length=7 */
        'O',  'b',  'j',  'e',  'c', 't', 's', /* "Objects" */
        0x02,                                  /* LocalizedText mask: text only */
        0x07, 0x00, 0x00, 0x00,                /* Int32: string length=7 */
        'O',  'b',  'j',  'e',  'c', 't', 's', /* "Objects" */
        0x01, 0x00, 0x00, 0x00,                /* UInt32: node_class=Object(1) */
        0x00, 0x3D,                            /* NodeId TwoByte: ns=0, id=61 (BaseObjectType) */
        0x00, 0x00, 0x00, 0x00,                /* Int32: diagnosticInfos=0 (empty) */
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), writer.position);
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));
}

void test_browse_service_rejects_invalid_browse_direction(void) {
    mu_browse_description_t desc = {
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84},
        .browse_direction = 3,
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    /* OPC-10000-4 section 5.9.2.4 lists Bad_BrowseDirectionInvalid for Browse
       results, and section 7.38.2 defines it for invalid BrowseDirection values. */
    mu_browse_request_t req = {
        .requested_max_references_per_node = 0, .nodes_to_browse = &desc, .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[4];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(NULL, NULL, &req, &result, 1, ref_pool, 4));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_BROWSEDIRECTIONINVALID, result.status_code);
    TEST_ASSERT_EQUAL(0, result.num_references);
}

void test_browse_next_unknown_continuation_points_return_invalid_results(void) {
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[512];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    mu_binary_reader_t response_reader;
    size_t response_length = 0;
    opcua_byte_t cp0_storage[4] = {1, 2, 3, 4};
    opcua_byte_t cp1_storage[2] = {5, 6};
    mu_bytestring_t continuation_points[2] = {{4, cp0_storage}, {2, cp1_storage}};

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 12);
    mu_binary_write_boolean(&request_writer, false);
    mu_binary_write_int32(&request_writer, 2);
    mu_binary_write_bytestring(&request_writer, &continuation_points[0]);
    mu_binary_write_bytestring(&request_writer, &continuation_points[1]);

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, handle_browse_next(NULL, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(response_writer.position, response_length);

    mu_binary_reader_init(&response_reader, response_buffer, response_length);
    skip_response_header(&response_reader);

    opcua_int32_t result_count;
    mu_binary_read_int32(&response_reader, &result_count);
    TEST_ASSERT_EQUAL(2, result_count);

    for (int i = 0; i < 2; ++i) {
        opcua_statuscode_t status;
        mu_bytestring_t continuation_point;
        opcua_int32_t reference_count;

        mu_binary_read_statuscode(&response_reader, &status);
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_CONTINUATIONPOINTINVALID, status);
        mu_binary_read_bytestring(&response_reader, &continuation_point);
        TEST_ASSERT_EQUAL(-1, continuation_point.length);
        mu_binary_read_int32(&response_reader, &reference_count);
        TEST_ASSERT_EQUAL(0, reference_count);
    }

    opcua_int32_t diagnostic_info_count;
    mu_binary_read_int32(&response_reader, &diagnostic_info_count);
    TEST_ASSERT_EQUAL(0, diagnostic_info_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, response_reader.status);
    TEST_ASSERT_EQUAL(response_length, response_reader.position);
}

void test_browse_next_over_continuation_point_capacity_rejects_without_body(void) {
    opcua_byte_t request_buffer[512];
    opcua_byte_t response_buffer[64];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    size_t response_length = 0;
    mu_bytestring_t empty_continuation_point = {0, NULL};

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 13);
    mu_binary_write_boolean(&request_writer, false);
    mu_binary_write_int32(&request_writer, 33);
    for (int i = 0; i < 33; ++i) {
        mu_binary_write_bytestring(&request_writer, &empty_continuation_point);
    }

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS,
                            handle_browse_next(NULL, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(0, response_length);
    TEST_ASSERT_EQUAL(0, response_writer.position);
}

void test_browse_malformed_negative_nodes_to_browse_rejects_without_body(void) {
    _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[64];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    size_t response_length = 0;
    mu_nodeid_t null_view = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
    mu_server_t *server = init_translate_test_server(server_storage, sizeof(server_storage));

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 16);
    mu_binary_write_nodeid(&request_writer, &null_view);
    mu_binary_write_int64(&request_writer, 0);
    mu_binary_write_uint32(&request_writer, 0);
    mu_binary_write_uint32(&request_writer, 0);
    mu_binary_write_int32(&request_writer, -2);

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            handle_browse(server, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(0, response_length);
    TEST_ASSERT_EQUAL(0, response_writer.position);
}

void test_browse_next_malformed_negative_continuation_point_count_rejects_without_body(void) {
    opcua_byte_t request_buffer[256];
    opcua_byte_t response_buffer[64];
    mu_binary_writer_t request_writer;
    mu_binary_writer_t response_writer;
    mu_binary_reader_t request_reader;
    size_t response_length = 0;

    mu_binary_writer_init(&request_writer, request_buffer, sizeof(request_buffer));
    write_test_request_header(&request_writer, 17);
    mu_binary_write_boolean(&request_writer, false);
    mu_binary_write_int32(&request_writer, -2);

    mu_binary_reader_init(&request_reader, request_buffer, request_writer.position);
    mu_binary_writer_init(&response_writer, response_buffer, sizeof(response_buffer));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            handle_browse_next(NULL, &request_reader, &response_writer, &response_length));
    TEST_ASSERT_EQUAL(0, response_length);
    TEST_ASSERT_EQUAL(0, response_writer.position);
}

/* T015: OPC-10000-4 §5.9.2.2 Table 34 — a non-null ViewDescription.viewId must
   restrict results to references defined for that View. This micro profile does
   not implement Views, so a non-null viewId is rejected with Bad_ViewIdUnknown
   instead of being silently ignored. A null viewId (the common case) browses
   normally. */
void test_browse_service_rejects_non_null_view_id(void) {
    mu_browse_description_t desc = {
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84},
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    mu_browse_request_t req = {.view_id = {.identifier_type = MU_NODEID_NUMERIC,
                                           .namespace_index = 0,
                                           .identifier.numeric = 12345}, /* non-null View */
                               .timestamp = 0,
                               .view_version = 0,
                               .requested_max_references_per_node = 0,
                               .nodes_to_browse = &desc,
                               .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[4];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(NULL, NULL, &req, &result, 1, ref_pool, 4));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_VIEWIDUNKNOWN, result.status_code);
    TEST_ASSERT_EQUAL(0, result.num_references);
}

void test_browse_service_accepts_null_view_id(void) {
    mu_browse_description_t desc = {
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84},
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    /* null viewId: ns=0, numeric 0 -> browse normally (no Bad_ViewIdUnknown) */
    mu_browse_request_t req = {
        .view_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .timestamp = 0,
        .view_version = 0,
        .requested_max_references_per_node = 0,
        .nodes_to_browse = &desc,
        .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[4];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(NULL, NULL, &req, &result, 1, ref_pool, 4));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_BAD_VIEWIDUNKNOWN, result.status_code);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_service_static_references);
    RUN_TEST(test_browse_service_response_encode);
    RUN_TEST(test_browse_service_rejects_invalid_browse_direction);
    RUN_TEST(test_browse_next_unknown_continuation_points_return_invalid_results);
    RUN_TEST(test_browse_next_over_continuation_point_capacity_rejects_without_body);
    RUN_TEST(test_browse_malformed_negative_nodes_to_browse_rejects_without_body);
    RUN_TEST(test_browse_next_malformed_negative_continuation_point_count_rejects_without_body);
    RUN_TEST(test_translate_browse_paths_scn_001_case_002_resolves_objects_to_variable);
    RUN_TEST(test_translate_browse_paths_scn_001_case_002_wrong_target_name_returns_no_match);
    RUN_TEST(test_translate_browse_paths_malformed_negative_browse_path_count_rejects_without_body);
    RUN_TEST(test_translate_browse_paths_malformed_negative_relative_path_element_count_rejects_without_body);
    RUN_TEST(test_translate_browse_paths_excessive_relative_path_elements_rejects_without_body);
    RUN_TEST(test_browse_service_rejects_non_null_view_id);
    RUN_TEST(test_browse_service_accepts_null_view_id);
    return UNITY_END();
}
