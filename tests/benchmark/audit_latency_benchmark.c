#define _POSIX_C_SOURCE 200809L

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUDIT_LATENCY_SCENARIO "valid-minimal-discovery-session-read"
#define DEFAULT_ITERATIONS 1000u
#define DEFAULT_WARMUP 100u
#define RESPONSE_CAPACITY 8192u

typedef struct {
    const char *scenario;
    uint32_t iterations;
    uint32_t warmup;
    const char *output_path;
} benchmark_options_t;

typedef struct {
    opcua_byte_t open_secure_channel[256];
    size_t open_secure_channel_len;
    opcua_byte_t find_servers[256];
    size_t find_servers_len;
    opcua_byte_t get_endpoints[256];
    size_t get_endpoints_len;
    opcua_byte_t create_session[512];
    size_t create_session_len;
    opcua_byte_t activate_session[256];
    size_t activate_session_len;
    opcua_byte_t read[256];
    size_t read_len;
} benchmark_requests_t;

static const mu_reference_t object_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
static const mu_reference_t variable_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
static const mu_value_source_t variable_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {MU_TYPE_INT32, {.i32 = 42}, false, 0}},
};
static const mu_node_t nodes[] = {
    {{0, MU_NODEID_NUMERIC, {85}},
     MU_NODECLASS_OBJECT,
     {7, (const opcua_byte_t *)"Objects"},
     {7, (const opcua_byte_t *)"Objects"},
     object_refs,
     1,
     NULL},
    {{1, MU_NODEID_NUMERIC, {1000}},
     MU_NODECLASS_VARIABLE,
     {6, (const opcua_byte_t *)"MyVar1"},
     {6, (const opcua_byte_t *)"MyVar1"},
     variable_refs,
     1,
     &variable_value},
};
static const mu_address_space_t address_space = {nodes, 2};

static void print_usage(FILE *stream, const char *program) {
    (void)fprintf(stream, "Usage: %s [--scenario %s] [--iterations N] [--warmup N] [--output PATH]\n", program,
            AUDIT_LATENCY_SCENARIO);
}

static int parse_u32_arg(const char *name, const char *value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0') {
        (void)fprintf(stderr, "%s requires a non-empty integer value\n", name);
        return 1;
    }

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > (unsigned long)UINT32_MAX) {
        (void)fprintf(stderr, "%s has invalid integer value: %s\n", name, value);
        return 1;
    }

    *out = (uint32_t)parsed;
    return 0;
}

static int parse_options(int argc, char **argv, benchmark_options_t *options) {
    options->scenario = AUDIT_LATENCY_SCENARIO;
    options->iterations = DEFAULT_ITERATIONS;
    options->warmup = DEFAULT_WARMUP;
    options->output_path = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            print_usage(stdout, argv[0]);
            exit(0);
        }

        if (i + 1 >= argc) {
            (void)fprintf(stderr, "%s requires a value\n", arg);
            return 1;
        }

        if (strcmp(arg, "--scenario") == 0) {
            options->scenario = argv[++i];
        } else if (strcmp(arg, "--iterations") == 0) {
            if (parse_u32_arg(arg, argv[++i], &options->iterations) != 0) {
                return 1;
            }
        } else if (strcmp(arg, "--warmup") == 0) {
            if (parse_u32_arg(arg, argv[++i], &options->warmup) != 0) {
                return 1;
            }
        } else if (strcmp(arg, "--output") == 0) {
            options->output_path = argv[++i];
        } else {
            (void)fprintf(stderr, "unknown option: %s\n", arg);
            print_usage(stderr, argv[0]);
            return 1;
        }
    }

    if (strcmp(options->scenario, AUDIT_LATENCY_SCENARIO) != 0) {
        (void)fprintf(stderr, "unsupported scenario: %s\n", options->scenario);
        return 1;
    }
    if (options->iterations == 0u) {
        (void)fprintf(stderr, "--iterations must be greater than zero\n");
        return 1;
    }

    return 0;
}

static void write_string_literal(mu_binary_writer_t *writer, const char *text) {
    mu_string_t value = {(opcua_int32_t)strlen(text), (const opcua_byte_t *)text};
    mu_binary_write_string(writer, &value);
}

static void write_raw_bytes(mu_binary_writer_t *writer, const opcua_byte_t *bytes, size_t length) {
    if (writer->status != MU_STATUS_GOOD) {
        return;
    }
    if (writer->position > writer->length || length > writer->length - writer->position) {
        writer->status = MU_STATUS_BAD_ENCODINGERROR;
        return;
    }
    (void)memcpy(writer->buffer + writer->position, bytes, length);
    writer->position += length;
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t auth_token, opcua_uint32_t request_handle) {
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};

    mu_binary_write_nodeid(writer, &auth);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, request_handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_str);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &null_id, 0);
}

static int finish_request(const char *name, const mu_binary_writer_t *writer, size_t *length) {
    if (writer->status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "failed to build %s request: 0x%08" PRIx32 "\n", name, (uint32_t)writer->status);
        return 1;
    }
    *length = writer->position;
    return 0;
}

static int build_requests(benchmark_requests_t *requests) {
    mu_binary_writer_t writer;
    mu_string_t null_str = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};

    mu_binary_writer_init(&writer, requests->open_secure_channel, sizeof(requests->open_secure_channel));
    write_request_header(&writer, 0, 1);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 1);
    mu_binary_write_bytestring(&writer, &null_bytes);
    mu_binary_write_uint32(&writer, 3600000);
    if (finish_request("OpenSecureChannel", &writer, &requests->open_secure_channel_len) != 0) {
        return 1;
    }

    mu_binary_writer_init(&writer, requests->find_servers, sizeof(requests->find_servers));
    write_request_header(&writer, 0, 2);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_int32(&writer, 0);
    mu_binary_write_int32(&writer, 0);
    if (finish_request("FindServers", &writer, &requests->find_servers_len) != 0) {
        return 1;
    }

    mu_binary_writer_init(&writer, requests->get_endpoints, sizeof(requests->get_endpoints));
    write_request_header(&writer, 0, 3);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_int32(&writer, 0);
    mu_binary_write_int32(&writer, 0);
    if (finish_request("GetEndpoints", &writer, &requests->get_endpoints_len) != 0) {
        return 1;
    }

    mu_binary_writer_init(&writer, requests->create_session, sizeof(requests->create_session));
    write_request_header(&writer, 0, 4);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_byte(&writer, 0);
    mu_binary_write_uint32(&writer, 1);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_int32(&writer, 0);
    mu_binary_write_string(&writer, &null_str);
    write_string_literal(&writer, "opc.tcp://host:4840");
    write_string_literal(&writer, "audit-latency");
    mu_binary_write_bytestring(&writer, &null_bytes);
    mu_binary_write_bytestring(&writer, &null_bytes);
    mu_binary_write_double(&writer, 1200000.0);
    mu_binary_write_uint32(&writer, 0);
    if (finish_request("CreateSession", &writer, &requests->create_session_len) != 0) {
        return 1;
    }

    mu_binary_writer_init(&writer, requests->activate_session, sizeof(requests->activate_session));
    write_request_header(&writer, TEST_FAKE_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_bytestring(&writer, &null_bytes);
    mu_binary_write_int32(&writer, 0);
    mu_binary_write_int32(&writer, 0);
    {
        opcua_byte_t token_body[32];
        mu_binary_writer_t token_writer;
        mu_nodeid_t anonymous = {0, MU_NODEID_NUMERIC, {MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY}};
        size_t token_body_len;

        mu_binary_writer_init(&token_writer, token_body, sizeof(token_body));
        write_string_literal(&token_writer, "anonymous");
        if (finish_request("AnonymousIdentityToken", &token_writer, &token_body_len) != 0) {
            return 1;
        }
        mu_binary_write_extension_object_header(&writer, &anonymous, token_body_len);
        write_raw_bytes(&writer, token_body, token_body_len);
    }
    mu_binary_write_string(&writer, &null_str);
    mu_binary_write_bytestring(&writer, &null_bytes);
    if (finish_request("ActivateSession", &writer, &requests->activate_session_len) != 0) {
        return 1;
    }

    mu_binary_writer_init(&writer, requests->read, sizeof(requests->read));
    write_request_header(&writer, TEST_FAKE_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_double(&writer, 0.0);
    mu_binary_write_uint32(&writer, 3);
    mu_binary_write_int32(&writer, 1);
    {
        mu_nodeid_t variable = {1, MU_NODEID_NUMERIC, {1000}};
        mu_binary_write_nodeid(&writer, &variable);
        mu_binary_write_uint32(&writer, 13);
        mu_binary_write_string(&writer, &null_str);
        mu_binary_write_uint16(&writer, 0);
        mu_binary_write_string(&writer, &null_str);
    }
    return finish_request("Read", &writer, &requests->read_len);
}

static opcua_statuscode_t init_benchmark_server(mu_server_t **server) {
    static _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
    static opcua_byte_t receive_buffer[MU_MIN_CHUNK_SIZE];
    static opcua_byte_t send_buffer[MU_MIN_CHUNK_SIZE];
    mu_server_config_t config;
    opcua_statuscode_t status;

    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:audit:latency";
    config.product_uri = "urn:audit:latency";
    config.application_name = "audit latency benchmark";
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);
    config.max_chunk_count = 1;
    config.max_message_size = MU_MIN_CHUNK_SIZE;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.address_space = &address_space;
    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);

    status = mu_server_init(server_storage, sizeof(server_storage), &config, server);
    if (status == MU_STATUS_GOOD && *server != NULL) {
        /* Direct dispatch uses this fallback channel when no mock transport slot is active. */
        mu_secure_channel_init(&(*server)->secure_channel);
    }
    return status;
}

static int read_response_prefix(const char *name, const opcua_byte_t *response, size_t response_len,
                                opcua_uint32_t expected_type, opcua_uint32_t expected_handle,
                                mu_binary_reader_t *body) {
    mu_binary_reader_t reader;
    mu_nodeid_t type;
    opcua_int64_t timestamp;
    opcua_uint32_t handle;
    opcua_statuscode_t service_result;
    opcua_byte_t diagnostics;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header;
    opcua_byte_t additional_header_encoding;

    mu_binary_reader_init(&reader, response, response_len);
    if (mu_binary_read_nodeid(&reader, &type) != MU_STATUS_GOOD || type.identifier_type != MU_NODEID_NUMERIC ||
        type.identifier.numeric != expected_type) {
        (void)fprintf(stderr, "%s returned unexpected response type\n", name);
        return 1;
    }

    mu_binary_read_int64(&reader, &timestamp);
    mu_binary_read_uint32(&reader, &handle);
    mu_binary_read_statuscode(&reader, &service_result);
    mu_binary_read_byte(&reader, &diagnostics);
    mu_binary_read_int32(&reader, &string_table_count);
    mu_binary_read_nodeid(&reader, &additional_header);
    mu_binary_read_byte(&reader, &additional_header_encoding);
    (void)timestamp;
    (void)diagnostics;
    (void)string_table_count;
    (void)additional_header;
    (void)additional_header_encoding;

    if (reader.status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "%s response header decode failed: 0x%08" PRIx32 "\n", name, (uint32_t)reader.status);
        return 1;
    }
    if (handle != expected_handle) {
        (void)fprintf(stderr, "%s returned requestHandle %" PRIu32 ", expected %" PRIu32 "\n", name, (uint32_t)handle,
                (uint32_t)expected_handle);
        return 1;
    }
    if (service_result != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "%s returned serviceResult 0x%08" PRIx32 "\n", name, (uint32_t)service_result);
        return 1;
    }

    *body = reader;
    return 0;
}

static int validate_count(const char *name, mu_binary_reader_t *body, opcua_int32_t expected,
                          opcua_boolean_t allow_greater_than_zero) {
    opcua_int32_t count;
    mu_binary_read_int32(body, &count);
    if (body->status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "%s count decode failed: 0x%08" PRIx32 "\n", name, (uint32_t)body->status);
        return 1;
    }
    if (allow_greater_than_zero) {
        if (count <= 0) {
            (void)fprintf(stderr, "%s returned no entries\n", name);
            return 1;
        }
    } else if (count != expected) {
        (void)fprintf(stderr, "%s returned count %" PRId32 ", expected %" PRId32 "\n", name, (int32_t)count,
                (int32_t)expected);
        return 1;
    }
    return 0;
}

static int validate_create_session(mu_binary_reader_t *body) {
    mu_nodeid_t session_id;
    mu_nodeid_t auth_token;

    mu_binary_read_nodeid(body, &session_id);
    mu_binary_read_nodeid(body, &auth_token);
    if (body->status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "CreateSession session token decode failed: 0x%08" PRIx32 "\n", (uint32_t)body->status);
        return 1;
    }
    if (auth_token.identifier_type != MU_NODEID_NUMERIC ||
        auth_token.identifier.numeric != TEST_FAKE_FIRST_AUTH_TOKEN) {
        (void)fprintf(stderr, "CreateSession returned unexpected authentication token\n");
        return 1;
    }
    (void)session_id;
    return 0;
}

static int validate_read(mu_binary_reader_t *body) {
    opcua_int32_t result_count;
    opcua_byte_t value_mask;
    opcua_byte_t value_type;
    opcua_int32_t value;

    mu_binary_read_int32(body, &result_count);
    mu_binary_read_byte(body, &value_mask);
    mu_binary_read_byte(body, &value_type);
    mu_binary_read_int32(body, &value);
    if (body->status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "Read result decode failed: 0x%08" PRIx32 "\n", (uint32_t)body->status);
        return 1;
    }
    if (result_count != 1 || value_mask != 0x01u || value_type != MU_TYPE_INT32 || value != 42) {
        (void)fprintf(stderr, "Read returned unexpected DataValue\n");
        return 1;
    }
    return 0;
}

static int dispatch_expect(mu_server_t *server, const char *name, opcua_uint32_t request_id,
                           const opcua_byte_t *request, size_t request_len, opcua_uint32_t expected_response_id,
                           opcua_uint32_t expected_handle, mu_binary_reader_t *body) {
    static opcua_byte_t response[RESPONSE_CAPACITY];
    size_t response_len = sizeof(response);
    opcua_statuscode_t status = mu_service_dispatch(server, request_id, request, request_len, response, &response_len);

    if (status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "%s dispatch failed: 0x%08" PRIx32 "\n", name, (uint32_t)status);
        return 1;
    }
    return read_response_prefix(name, response, response_len, expected_response_id, expected_handle, body);
}

static int run_flow(mu_server_t *server, const benchmark_requests_t *requests) {
    mu_binary_reader_t body;

    if (dispatch_expect(server, "OpenSecureChannel", MU_ID_OPENSECURECHANNELREQUEST, requests->open_secure_channel,
                        requests->open_secure_channel_len, MU_ID_OPENSECURECHANNELRESPONSE, 1, &body) != 0) {
        return 1;
    }
    if (dispatch_expect(server, "FindServers", MU_ID_FINDSERVERSREQUEST, requests->find_servers,
                        requests->find_servers_len, MU_ID_FINDSERVERSRESPONSE, 2, &body) != 0 ||
        validate_count("FindServers", &body, 1, false) != 0) {
        return 1;
    }
    if (dispatch_expect(server, "GetEndpoints", MU_ID_GETENDPOINTSREQUEST, requests->get_endpoints,
                        requests->get_endpoints_len, MU_ID_GETENDPOINTSRESPONSE, 3, &body) != 0 ||
        validate_count("GetEndpoints", &body, 0, true) != 0) {
        return 1;
    }
    if (dispatch_expect(server, "CreateSession", MU_ID_CREATESESSIONREQUEST, requests->create_session,
                        requests->create_session_len, MU_ID_CREATESESSIONRESPONSE, 4, &body) != 0 ||
        validate_create_session(&body) != 0) {
        return 1;
    }
    if (dispatch_expect(server, "ActivateSession", MU_ID_ACTIVATESESSIONREQUEST, requests->activate_session,
                        requests->activate_session_len, MU_ID_ACTIVATESESSIONRESPONSE, 5, &body) != 0) {
        return 1;
    }
    if (dispatch_expect(server, "Read", MU_ID_READREQUEST, requests->read, requests->read_len, MU_ID_READRESPONSE, 6,
                        &body) != 0 ||
        validate_read(&body) != 0) {
        return 1;
    }

    return 0;
}

static int init_and_run_flow(const benchmark_requests_t *requests) {
    mu_server_t *server = NULL;
    opcua_statuscode_t status = init_benchmark_server(&server);

    if (status != MU_STATUS_GOOD) {
        (void)fprintf(stderr, "server init failed: 0x%08" PRIx32 "\n", (uint32_t)status);
        return 1;
    }
    return run_flow(server, requests);
}

static uint64_t monotonic_ns(void) {
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0;
    }
    return ((uint64_t)timestamp.tv_sec * 1000000000ull) + (uint64_t)timestamp.tv_nsec;
}

static int write_output_file(const char *path, const char *json) {
    FILE *file;

    if (path == NULL || strcmp(path, "-") == 0) {
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        (void)fprintf(stderr, "failed to open output path %s: %s\n", path, strerror(errno));
        return 1;
    }
    if (fputs(json, file) == EOF) {
        (void)fprintf(stderr, "failed to write output path %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        (void)fprintf(stderr, "failed to close output path %s: %s\n", path, strerror(errno));
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    benchmark_options_t options;
    benchmark_requests_t requests;
    uint64_t total_ns = 0;
    char json[1024];
    int json_len;

    if (parse_options(argc, argv, &options) != 0) {
        return 2;
    }
    if (build_requests(&requests) != 0) {
        return 1;
    }

    for (uint32_t i = 0; i < options.warmup; ++i) {
        if (init_and_run_flow(&requests) != 0) {
            return 1;
        }
    }

    for (uint32_t i = 0; i < options.iterations; ++i) {
        uint64_t start_ns;
        uint64_t end_ns;
        mu_server_t *server = NULL;
        opcua_statuscode_t status = init_benchmark_server(&server);

        if (status != MU_STATUS_GOOD) {
            fprintf(stderr, "server init failed: 0x%08" PRIx32 "\n", (uint32_t)status);
            return 1;
        }

        start_ns = monotonic_ns();
        if (start_ns == 0u || run_flow(server, &requests) != 0) {
            return 1;
        }
        end_ns = monotonic_ns();
        if (end_ns == 0u || end_ns < start_ns) {
            (void)fprintf(stderr, "monotonic clock failed during benchmark\n");
            return 1;
        }
        total_ns += end_ns - start_ns;
    }

    json_len = snprintf(json, sizeof(json),
                        "{\n"
                        "  \"scenario\": \"%s\",\n"
                        "  \"iterations\": %" PRIu32 ",\n"
                        "  \"warmup\": %" PRIu32 ",\n"
                        "  \"latency_ns_total\": %" PRIu64 ",\n"
                        "  \"elapsed_ns\": %" PRIu64 ",\n"
                        "  \"mean_ns\": %" PRIu64 ",\n"
                        "  \"unit\": \"ns\",\n"
                        "  \"command_build_note\": \"host in-process service dispatch; "
                        "OpenSecureChannel, FindServers, GetEndpoints, CreateSession, "
                        "ActivateSession, Read; no network or sleeps\"\n"
                        "}\n",
                        options.scenario, options.iterations, options.warmup, total_ns, total_ns,
                        total_ns / options.iterations);
    if (json_len < 0 || (size_t)json_len >= sizeof(json)) {
        (void)fprintf(stderr, "failed to format benchmark JSON\n");
        return 1;
    }

    (void)fputs(json, stdout);
    return write_output_file(options.output_path, json);
}
