#define _POSIX_C_SOURCE 200809L

#include "core/message_chunk.h"
#include "core/server_internal.h"
#include "core/service_dispatch.h"
#include "core/uasc.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "services/secure_channel.h"
#include "services/session.h"
#ifdef MUC_OPCUA_SUBSCRIPTIONS
#include "services/subscription.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_BENCH_NODES 128u
#define MAX_BENCH_BATCH 32u
#define BENCH_NODE_BASE 1000u
#define BENCH_ATTRIBUTE_VALUE 13u
#define REQUEST_CAPACITY 8192u
#define RESPONSE_CAPACITY 8192u
#define DEFAULT_ITERATIONS 20000u
#define DEFAULT_WARMUP 2000u
#define MESSAGE_CHUNK_HEADER_SIZE 12u

typedef enum {
    SCENARIO_READ_VALUE = 0,
    SCENARIO_READ_BAD_NODE,
    SCENARIO_BINARY_PRIMITIVE,
    SCENARIO_MESSAGE_CHUNK_HEADER,
    SCENARIO_UASC_RESPONSE_FRAMING,
    SCENARIO_WRITE_VALUE,
    SCENARIO_WRITE_BAD_TYPE,
    SCENARIO_SUBSCRIPTION_IDLE_TICK,
    SCENARIO_SUBSCRIPTION_ACTIVE_TICK,
    SCENARIO_UNKNOWN
} scenario_t;

typedef struct {
    scenario_t scenario;
    const char *scenario_name;
    size_t nodes;
    size_t batch;
    uint32_t iterations;
    uint32_t warmup;
    uint32_t min_ms;
} bench_options_t;

static _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t receive_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t send_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t request_buffer[REQUEST_CAPACITY];
static opcua_byte_t response_buffer[RESPONSE_CAPACITY];
static opcua_byte_t message_chunk_header_input[MESSAGE_CHUNK_HEADER_SIZE] = {
    'M', 'S', 'G', 'F', 12u, 0u, 0u, 0u, 1u, 0u, 0u, 0u,
};
static opcua_byte_t message_chunk_header_output[MESSAGE_CHUNK_HEADER_SIZE];
static mu_node_t bench_nodes[MAX_BENCH_NODES];
static mu_address_space_t bench_address_space;
static opcua_int32_t bench_values[MAX_BENCH_NODES];
static volatile uint64_t calibration_sink;

opcua_statuscode_t mu_binary_read_array_length(mu_binary_reader_t *reader, opcua_int32_t *length);

static opcua_statuscode_t read_bench_value(void *context, const mu_nodeid_t *node_id, mu_variant_t *value) {
    (void)context;
    if (node_id == NULL || node_id->identifier_type != MU_NODEID_NUMERIC ||
        node_id->identifier.numeric < BENCH_NODE_BASE) {
        return MU_STATUS_BAD_NODEIDUNKNOWN;
    }

    opcua_uint32_t index = node_id->identifier.numeric - BENCH_NODE_BASE;
    if (index >= MAX_BENCH_NODES) {
        return MU_STATUS_BAD_NODEIDUNKNOWN;
    }

    value->type = MU_TYPE_INT32;
    value->value.i32 = bench_values[index];
    value->is_array = false;
    value->array_length = 0;
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_SERVICE_WRITE
static opcua_statuscode_t write_bench_value(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                            const mu_variant_t *value) {
    (void)handle;
    if (attribute_id != BENCH_ATTRIBUTE_VALUE) {
        return MU_STATUS_BAD_NOTWRITABLE;
    }
    if (node_id == NULL || node_id->identifier_type != MU_NODEID_NUMERIC ||
        node_id->identifier.numeric < BENCH_NODE_BASE) {
        return MU_STATUS_BAD_NODEIDUNKNOWN;
    }

    opcua_uint32_t index = node_id->identifier.numeric - BENCH_NODE_BASE;
    if (index >= MAX_BENCH_NODES) {
        return MU_STATUS_BAD_NODEIDUNKNOWN;
    }
    if (value == NULL || value->is_array || value->type != MU_TYPE_INT32) {
        return MU_STATUS_BAD_TYPEMISMATCH;
    }

    bench_values[index] = value->value.i32;
    return MU_STATUS_GOOD;
}
#endif

static const char *scenario_name(scenario_t scenario) {
    switch (scenario) {
    case SCENARIO_READ_VALUE:
        return "read-value";
    case SCENARIO_READ_BAD_NODE:
        return "read-bad-node";
    case SCENARIO_BINARY_PRIMITIVE:
        return "binary-primitive";
    case SCENARIO_MESSAGE_CHUNK_HEADER:
        return "message-chunk-header";
    case SCENARIO_UASC_RESPONSE_FRAMING:
        return "uasc-response-framing";
    case SCENARIO_WRITE_VALUE:
        return "write-value";
    case SCENARIO_WRITE_BAD_TYPE:
        return "write-bad-type";
    case SCENARIO_SUBSCRIPTION_IDLE_TICK:
        return "subscription-idle-tick";
    case SCENARIO_SUBSCRIPTION_ACTIVE_TICK:
        return "subscription-active-tick";
    default:
        return "unknown";
    }
}

static scenario_t parse_scenario(const char *text) {
    if (strcmp(text, "read-value") == 0) {
        return SCENARIO_READ_VALUE;
    }
    if (strcmp(text, "read-bad-node") == 0) {
        return SCENARIO_READ_BAD_NODE;
    }
    if (strcmp(text, "binary-primitive") == 0) {
        return SCENARIO_BINARY_PRIMITIVE;
    }
    if (strcmp(text, "message-chunk-header") == 0) {
        return SCENARIO_MESSAGE_CHUNK_HEADER;
    }
    if (strcmp(text, "uasc-response-framing") == 0) {
        return SCENARIO_UASC_RESPONSE_FRAMING;
    }
    if (strcmp(text, "write-value") == 0) {
        return SCENARIO_WRITE_VALUE;
    }
    if (strcmp(text, "write-bad-type") == 0) {
        return SCENARIO_WRITE_BAD_TYPE;
    }
    if (strcmp(text, "subscription-idle-tick") == 0) {
        return SCENARIO_SUBSCRIPTION_IDLE_TICK;
    }
    if (strcmp(text, "subscription-active-tick") == 0) {
        return SCENARIO_SUBSCRIPTION_ACTIVE_TICK;
    }
    return SCENARIO_UNKNOWN;
}

static bool scenario_supported(scenario_t scenario) {
    switch (scenario) {
    case SCENARIO_READ_VALUE:
    case SCENARIO_READ_BAD_NODE:
#ifdef MUC_OPCUA_SERVICE_READ
        return true;
#else
        return false;
#endif
    case SCENARIO_BINARY_PRIMITIVE:
    case SCENARIO_MESSAGE_CHUNK_HEADER:
    case SCENARIO_UASC_RESPONSE_FRAMING:
        return true;
    case SCENARIO_WRITE_VALUE:
    case SCENARIO_WRITE_BAD_TYPE:
#ifdef MUC_OPCUA_SERVICE_WRITE
        return true;
#else
        return false;
#endif
    case SCENARIO_SUBSCRIPTION_IDLE_TICK:
    case SCENARIO_SUBSCRIPTION_ACTIVE_TICK:
#ifdef MUC_OPCUA_SUBSCRIPTIONS
        return true;
#else
        return false;
#endif
    default:
        return false;
    }
}

static int parse_u32_arg(const char *name, const char *value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0') {
        (void)fprintf(stderr, "%s requires a value\n", name);
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

static int parse_size_arg(const char *name, const char *value, size_t *out) {
    uint32_t parsed;
    if (parse_u32_arg(name, value, &parsed) != 0) {
        return 1;
    }
    *out = (size_t)parsed;
    return 0;
}

static void print_usage(FILE *stream, const char *program) {
    (void)fprintf(stream,
            "Usage: %s --scenario NAME [--nodes N] [--batch N] [--iterations N] [--warmup N] [--min-ms N]\n"
            "       %s --list\n",
            program, program);
}

static void print_list(void) {
    scenario_t scenarios[] = {
        SCENARIO_READ_VALUE,           SCENARIO_READ_BAD_NODE,          SCENARIO_BINARY_PRIMITIVE,
        SCENARIO_MESSAGE_CHUNK_HEADER, SCENARIO_UASC_RESPONSE_FRAMING,  SCENARIO_WRITE_VALUE,
        SCENARIO_WRITE_BAD_TYPE,       SCENARIO_SUBSCRIPTION_IDLE_TICK, SCENARIO_SUBSCRIPTION_ACTIVE_TICK,
    };
    for (size_t i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); ++i) {
        (void)printf("%s %s\n", scenario_name(scenarios[i]), scenario_supported(scenarios[i]) ? "supported" : "unsupported");
    }
}

static int parse_options(int argc, char **argv, bench_options_t *options) {
    options->scenario = SCENARIO_READ_VALUE;
    options->scenario_name = scenario_name(SCENARIO_READ_VALUE);
    options->nodes = 1u;
    options->batch = 1u;
    options->iterations = DEFAULT_ITERATIONS;
    options->warmup = DEFAULT_WARMUP;
    options->min_ms = 0u;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            print_usage(stdout, argv[0]);
            exit(0);
        }
        if (strcmp(arg, "--list") == 0) {
            print_list();
            exit(0);
        }

        if (i + 1 >= argc) {
            (void)fprintf(stderr, "%s requires a value\n", arg);
            return 2;
        }

        if (strcmp(arg, "--scenario") == 0) {
            options->scenario = parse_scenario(argv[++i]);
            options->scenario_name = argv[i];
        } else if (strcmp(arg, "--nodes") == 0) {
            if (parse_size_arg(arg, argv[++i], &options->nodes) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--batch") == 0) {
            if (parse_size_arg(arg, argv[++i], &options->batch) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--iterations") == 0) {
            if (parse_u32_arg(arg, argv[++i], &options->iterations) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--warmup") == 0) {
            if (parse_u32_arg(arg, argv[++i], &options->warmup) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--min-ms") == 0) {
            if (parse_u32_arg(arg, argv[++i], &options->min_ms) != 0) {
                return 2;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", arg);
            print_usage(stderr, argv[0]);
            return 2;
        }
    }

    if (options->scenario == SCENARIO_UNKNOWN) {
        fprintf(stderr, "unsupported scenario name: %s\n", options->scenario_name);
        return 2;
    }
    if (options->nodes == 0u || options->nodes > MAX_BENCH_NODES) {
        fprintf(stderr, "--nodes must be in the range 1..%u\n", (unsigned)MAX_BENCH_NODES);
        return 2;
    }
    if (options->batch == 0u || options->batch > MAX_BENCH_BATCH) {
        fprintf(stderr, "--batch must be in the range 1..%u\n", (unsigned)MAX_BENCH_BATCH);
        return 2;
    }
    if (options->iterations == 0u) {
        fprintf(stderr, "--iterations must be greater than zero\n");
        return 2;
    }

    return 0;
}

static void write_request_header(mu_binary_writer_t *writer) {
    mu_nodeid_t token = {0, MU_NODEID_NUMERIC, {TEST_FAKE_FIRST_AUTH_TOKEN}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(writer, &token);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, 1);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &null_id, 0);
}

static opcua_statuscode_t build_read_request(const bench_options_t *options, opcua_byte_t *buffer, size_t capacity,
                                             size_t *request_len) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer);
    mu_binary_write_double(&writer, 0.0);
    mu_binary_write_uint32(&writer, 3u);
    mu_binary_write_int32(&writer, (opcua_int32_t)options->batch);

    for (size_t i = 0; i < options->batch; ++i) {
        opcua_uint32_t numeric = BENCH_NODE_BASE + (opcua_uint32_t)(i % options->nodes);
        if (options->scenario == SCENARIO_READ_BAD_NODE) {
            numeric += 100000u;
        }
        mu_nodeid_t node_id = {1u, MU_NODEID_NUMERIC, {numeric}};
        mu_binary_write_nodeid(&writer, &node_id);
        mu_binary_write_uint32(&writer, BENCH_ATTRIBUTE_VALUE);
        mu_binary_write_string(&writer, &null_string);
        mu_binary_write_uint16(&writer, 0);
        mu_binary_write_string(&writer, &null_string);
    }

    if (writer.status != MU_STATUS_GOOD) {
        return writer.status;
    }
    *request_len = writer.position;
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_SERVICE_WRITE
static opcua_statuscode_t build_write_request(const bench_options_t *options, opcua_byte_t *buffer, size_t capacity,
                                              size_t *request_len) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_string_t wrong_type = {4, (const opcua_byte_t *)"oops"};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer);
    mu_binary_write_int32(&writer, (opcua_int32_t)options->batch);

    for (size_t i = 0; i < options->batch; ++i) {
        mu_nodeid_t node_id = {1u, MU_NODEID_NUMERIC, {BENCH_NODE_BASE + (opcua_uint32_t)(i % options->nodes)}};
        mu_datavalue_t value;
        memset(&value, 0, sizeof(value));
        value.has_value = true;
        if (options->scenario == SCENARIO_WRITE_BAD_TYPE) {
            value.value.type = MU_TYPE_STRING;
            value.value.value.str = wrong_type;
        } else {
            value.value.type = MU_TYPE_INT32;
            value.value.value.i32 = (opcua_int32_t)i;
        }

        mu_binary_write_nodeid(&writer, &node_id);
        mu_binary_write_int32(&writer, BENCH_ATTRIBUTE_VALUE);
        mu_binary_write_string(&writer, &null_string);
        mu_binary_write_datavalue(&writer, &value);
    }

    if (writer.status != MU_STATUS_GOOD) {
        return writer.status;
    }
    *request_len = writer.position;
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t build_request(const bench_options_t *options, size_t *request_len, opcua_uint32_t *request_id,
                                        opcua_uint32_t *response_id) {
    switch (options->scenario) {
    case SCENARIO_READ_VALUE:
    case SCENARIO_READ_BAD_NODE:
        *request_id = MU_ID_READREQUEST;
        *response_id = MU_ID_READRESPONSE;
        return build_read_request(options, request_buffer, sizeof(request_buffer), request_len);
    case SCENARIO_WRITE_VALUE:
    case SCENARIO_WRITE_BAD_TYPE:
#ifdef MUC_OPCUA_SERVICE_WRITE
        *request_id = MU_ID_WRITEREQUEST;
        *response_id = MU_ID_WRITERESPONSE;
        return build_write_request(options, request_buffer, sizeof(request_buffer), request_len);
#else
        return MU_STATUS_BAD_NOTSUPPORTED;
#endif
    default:
        *request_id = 0;
        *response_id = 0;
        *request_len = 0;
        return MU_STATUS_GOOD;
    }
}

static void init_address_space(size_t node_count, mu_address_space_t *address_space) {
    static const mu_value_source_t value_source = {MU_VALUESOURCE_CALLBACK, {.callback = {read_bench_value, NULL}}};

    for (size_t i = 0; i < node_count; ++i) {
        bench_values[i] = (opcua_int32_t)i;
        bench_nodes[i].node_id.namespace_index = 1u;
        bench_nodes[i].node_id.identifier_type = MU_NODEID_NUMERIC;
        bench_nodes[i].node_id.identifier.numeric = BENCH_NODE_BASE + (opcua_uint32_t)i;
        bench_nodes[i].node_class = MU_NODECLASS_VARIABLE;
        bench_nodes[i].browse_name.length = 0;
        bench_nodes[i].browse_name.data = NULL;
        bench_nodes[i].display_name.length = 0;
        bench_nodes[i].display_name.data = NULL;
        bench_nodes[i].references = NULL;
        bench_nodes[i].reference_count = 0u;
        bench_nodes[i].value = &value_source;
    }

    address_space->nodes = bench_nodes;
    address_space->node_count = node_count;
}

static opcua_statuscode_t init_server(size_t node_count, mu_server_t **server) {
    mu_server_config_t config;

    init_address_space(node_count, &bench_address_space);
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:muc-opcua:hotpath-benchmark";
    config.product_uri = "urn:muc-opcua:hotpath-benchmark";
    config.application_name = "muc-opcua hotpath benchmark";
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);
    config.max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config.max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;
    config.max_sessions = 1u;
    config.max_secure_channels = 1u;
    config.address_space = &bench_address_space;
#ifdef MUC_OPCUA_SERVICE_WRITE
    config.write_handler = write_bench_value;
#endif
    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);

    opcua_statuscode_t status = mu_server_init(server_storage, sizeof(server_storage), &config, server);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    mu_secure_channel_init(&(*server)->secure_channel);
    (*server)->secure_channel.is_open = true;
    (*server)->secure_channel.channel_id = 1u;
    (*server)->secure_channel.token_id = 1u;
    (*server)->secure_channel.revised_lifetime = 3600000u;

    (*server)->sessions[0].state = MU_SESSION_STATE_ACTIVATED;
    (*server)->sessions[0].session_id = 1u;
    (*server)->sessions[0].auth_token = TEST_FAKE_FIRST_AUTH_TOKEN;
    (*server)->sessions[0].revised_session_timeout_ms = 3600000u;
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    (*server)->sessions[0].secure_channel_id = 1u;
#endif

    return MU_STATUS_GOOD;
}

static int validate_response_header(const opcua_byte_t *response, size_t response_len, opcua_uint32_t response_id) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_int64_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;

    mu_binary_reader_init(&reader, response, response_len);
    mu_binary_read_nodeid(&reader, &response_type);
    mu_binary_read_int64(&reader, &timestamp);
    mu_binary_read_uint32(&reader, &request_handle);
    mu_binary_read_statuscode(&reader, &service_result);
    (void)timestamp;
    (void)request_handle;

    if (reader.status != MU_STATUS_GOOD) {
        return 1;
    }
    if (response_type.identifier_type != MU_NODEID_NUMERIC || response_type.identifier.numeric != response_id) {
        return 1;
    }
    return service_result == MU_STATUS_GOOD ? 0 : 1;
}

static int run_dispatch_once(mu_server_t *server, opcua_uint32_t request_id, size_t request_len,
                             opcua_uint32_t response_id) {
    size_t response_len = sizeof(response_buffer);
    opcua_statuscode_t status =
        mu_service_dispatch(server, request_id, request_buffer, request_len, response_buffer, &response_len);
    if (status != MU_STATUS_GOOD) {
        return 1;
    }
    return validate_response_header(response_buffer, response_len, response_id);
}

static int run_message_chunk_header_once(size_t batch) {
    uint64_t checksum = 0u;

    for (size_t i = 0; i < batch; ++i) {
        mu_message_header_t header;
        opcua_statuscode_t status =
            mu_parse_message_header(message_chunk_header_input, sizeof(message_chunk_header_input), &header);
        if (status != MU_STATUS_GOOD) {
            return 1;
        }

        status = mu_write_message_header(message_chunk_header_output, sizeof(message_chunk_header_output), &header);
        if (status != MU_STATUS_GOOD) {
            return 1;
        }

        checksum += (uint64_t)message_chunk_header_output[0];
        checksum += (uint64_t)message_chunk_header_output[4];
        checksum += (uint64_t)message_chunk_header_output[8];
    }

    calibration_sink ^= checksum;
    return memcmp(message_chunk_header_input, message_chunk_header_output, MESSAGE_CHUNK_HEADER_SIZE) == 0 ? 0 : 1;
}

static int run_binary_primitive_once(size_t batch) {
    mu_binary_writer_t writer;
    mu_binary_reader_t reader;
    uint64_t checksum = 0u;
    const size_t encoded_len_per_item = 51u;

    if (batch > sizeof(request_buffer) / encoded_len_per_item) {
        return 1;
    }

    mu_binary_writer_init(&writer, request_buffer, sizeof(request_buffer));
    for (size_t i = 0; i < batch; ++i) {
        opcua_uint32_t index = (opcua_uint32_t)i;
        mu_binary_write_boolean(&writer, (index & 1u) != 0u);
        mu_binary_write_sbyte(&writer, (opcua_sbyte_t)(-12 + (opcua_int32_t)index));
        mu_binary_write_byte(&writer, (opcua_byte_t)(0xA0u + index));
        mu_binary_write_int16(&writer, (opcua_int16_t)(-1234 + (opcua_int32_t)index));
        mu_binary_write_uint16(&writer, (opcua_uint16_t)(1234u + index));
        mu_binary_write_int32(&writer, (opcua_int32_t)(-123456 + (opcua_int32_t)index));
        mu_binary_write_uint32(&writer, 123456u + index);
        mu_binary_write_int64(&writer, (opcua_int64_t)-1234567890123ll + (opcua_int64_t)index);
        mu_binary_write_uint64(&writer, 1234567890123ull + (opcua_uint64_t)index);
        mu_binary_write_float(&writer, 1.25f + (opcua_float_t)index);
        mu_binary_write_double(&writer, 2.5 + (opcua_double_t)index);
        mu_binary_write_statuscode(&writer, MU_STATUS_GOOD);
        mu_binary_write_int32(&writer, (opcua_int32_t)batch);
    }

    if (writer.status != MU_STATUS_GOOD || writer.position != batch * encoded_len_per_item) {
        return 1;
    }

    mu_binary_reader_init(&reader, request_buffer, writer.position);
    for (size_t i = 0; i < batch; ++i) {
        opcua_boolean_t bool_value = false;
        opcua_sbyte_t sbyte_value = 0;
        opcua_byte_t byte_value = 0;
        opcua_int16_t int16_value = 0;
        opcua_uint16_t uint16_value = 0;
        opcua_int32_t int32_value = 0;
        opcua_uint32_t uint32_value = 0;
        opcua_int64_t int64_value = 0;
        opcua_uint64_t uint64_value = 0;
        opcua_float_t float_value = 0.0f;
        opcua_double_t double_value = 0.0;
        opcua_statuscode_t status_value = 0u;
        opcua_int32_t array_length = 0;
        opcua_uint32_t index = (opcua_uint32_t)i;

        mu_binary_read_boolean(&reader, &bool_value);
        mu_binary_read_sbyte(&reader, &sbyte_value);
        mu_binary_read_byte(&reader, &byte_value);
        mu_binary_read_int16(&reader, &int16_value);
        mu_binary_read_uint16(&reader, &uint16_value);
        mu_binary_read_int32(&reader, &int32_value);
        mu_binary_read_uint32(&reader, &uint32_value);
        mu_binary_read_int64(&reader, &int64_value);
        mu_binary_read_uint64(&reader, &uint64_value);
        mu_binary_read_float(&reader, &float_value);
        mu_binary_read_double(&reader, &double_value);
        mu_binary_read_statuscode(&reader, &status_value);
        mu_binary_read_array_length(&reader, &array_length);

        if (reader.status != MU_STATUS_GOOD || bool_value != ((index & 1u) != 0u) ||
            sbyte_value != (opcua_sbyte_t)(-12 + (opcua_int32_t)index) || byte_value != (opcua_byte_t)(0xA0u + index) ||
            int16_value != (opcua_int16_t)(-1234 + (opcua_int32_t)index) ||
            uint16_value != (opcua_uint16_t)(1234u + index) ||
            int32_value != (opcua_int32_t)(-123456 + (opcua_int32_t)index) || uint32_value != 123456u + index ||
            int64_value != (opcua_int64_t)-1234567890123ll + (opcua_int64_t)index ||
            uint64_value != 1234567890123ull + (opcua_uint64_t)index || float_value != 1.25f + (opcua_float_t)index ||
            double_value != 2.5 + (opcua_double_t)index || status_value != MU_STATUS_GOOD ||
            array_length != (opcua_int32_t)batch) {
            return 1;
        }

        checksum += (uint64_t)byte_value + (uint64_t)uint16_value + (uint64_t)uint32_value + uint64_value;
        checksum += (uint64_t)(opcua_uint32_t)array_length;
    }

    if (reader.position != writer.position) {
        return 1;
    }

    calibration_sink ^= checksum;
    return 0;
}

static int validate_uasc_response_frame(const opcua_byte_t *buffer, size_t total_len, const opcua_byte_t type[3],
                                        opcua_uint32_t secure_channel_id, size_t sequence_offset,
                                        opcua_uint32_t sequence_number, opcua_uint32_t request_id, size_t body_offset,
                                        size_t body_length, opcua_byte_t body_marker, uint64_t *checksum) {
    mu_message_header_t header;
    mu_sequence_header_t sequence;
    size_t offset = sequence_offset;
    opcua_statuscode_t status;

    status = mu_parse_message_header(buffer, total_len, &header);
    if (status != MU_STATUS_GOOD) {
        return 1;
    }

    if (header.message_type[0] != type[0] || header.message_type[1] != type[1] || header.message_type[2] != type[2] ||
        header.chunk_type != 'F' || header.message_size != (opcua_uint32_t)total_len ||
        header.secure_channel_id != secure_channel_id) {
        return 1;
    }

    status = mu_parse_sequence_header(buffer, total_len, &offset, &sequence);
    if (status != MU_STATUS_GOOD || offset != sequence_offset + 8u) {
        return 1;
    }

    if (sequence.sequence_number != sequence_number || sequence.request_id != request_id) {
        return 1;
    }
    if (body_length == 0u || body_offset > total_len || body_length > total_len - body_offset) {
        return 1;
    }
    if (buffer[body_offset] != body_marker ||
        buffer[body_offset + body_length - 1u] != (opcua_byte_t)(body_marker ^ 0x5au)) {
        return 1;
    }

    *checksum += (uint64_t)header.message_size + (uint64_t)header.secure_channel_id;
    *checksum += (uint64_t)sequence.sequence_number + (uint64_t)sequence.request_id;
    *checksum += (uint64_t)buffer[body_offset] + (uint64_t)buffer[body_offset + body_length - 1u];
    return 0;
}

static int run_uasc_response_framing_once(size_t batch) {
    static const opcua_byte_t msg_type[3] = {'M', 'S', 'G'};
    static const opcua_byte_t opn_type[3] = {'O', 'P', 'N'};
    const size_t body_length = 16u;
    const size_t asymmetric_sequence_offset = MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE - 8u;
    uint64_t checksum = 0u;

    for (size_t i = 0; i < batch; ++i) {
        size_t total_len = 0u;
        opcua_uint32_t sequence_number = 1000u + (opcua_uint32_t)(i * 2u);
        opcua_uint32_t request_id = 2000u + (opcua_uint32_t)(i * 2u);
        opcua_byte_t body_marker = (opcua_byte_t)(0x30u + (opcua_byte_t)(i & 0x0fu));
        opcua_statuscode_t status;

        response_buffer[MU_UASC_SYMMETRIC_HEADER_SIZE] = body_marker;
        response_buffer[MU_UASC_SYMMETRIC_HEADER_SIZE + body_length - 1u] = (opcua_byte_t)(body_marker ^ 0x5au);
        status = mu_uasc_finalize_symmetric(response_buffer, sizeof(response_buffer), 1u, 1u, sequence_number,
                                            request_id, body_length, &total_len);
        if (status != MU_STATUS_GOOD) {
            return 1;
        }
        if (validate_uasc_response_frame(response_buffer, total_len, msg_type, 1u, 16u, sequence_number, request_id,
                                         MU_UASC_SYMMETRIC_HEADER_SIZE, body_length, body_marker, &checksum) != 0) {
            return 1;
        }

        sequence_number++;
        request_id++;
        body_marker = (opcua_byte_t)(0x70u + (opcua_byte_t)(i & 0x0fu));
        response_buffer[MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE] = body_marker;
        response_buffer[MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE + body_length - 1u] = (opcua_byte_t)(body_marker ^ 0x5au);
        status = mu_uasc_finalize_asymmetric_none(response_buffer, sizeof(response_buffer), 1u, sequence_number,
                                                  request_id, body_length, &total_len);
        if (status != MU_STATUS_GOOD) {
            return 1;
        }
        if (validate_uasc_response_frame(response_buffer, total_len, opn_type, 1u, asymmetric_sequence_offset,
                                         sequence_number, request_id, MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE, body_length,
                                         body_marker, &checksum) != 0) {
            return 1;
        }
    }

    calibration_sink ^= checksum;
    return 0;
}

static uint64_t monotonic_ns(void) {
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0u;
    }
    return ((uint64_t)timestamp.tv_sec * 1000000000ull) + (uint64_t)timestamp.tv_nsec;
}

static double run_calibration(uint32_t min_ms) {
    uint64_t min_ns = (uint64_t)min_ms * 1000000ull;
    uint64_t start_ns = monotonic_ns();
    uint64_t elapsed_ns = 0u;
    uint64_t iterations = 0u;
    uint64_t state = 0x9e3779b97f4a7c15ull;

    if (start_ns == 0u || min_ns == 0u) {
        return 0.0;
    }

    do {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        iterations++;
        uint64_t now = monotonic_ns();
        if (now == 0u || now < start_ns) {
            return 0.0;
        }
        elapsed_ns = now - start_ns;
    } while (elapsed_ns < min_ns);

    calibration_sink = state;
    return elapsed_ns > 0u ? ((double)iterations * 1000000000.0) / (double)elapsed_ns : 0.0;
}

#ifdef MUC_OPCUA_SUBSCRIPTIONS
static opcua_statuscode_t setup_active_subscription(mu_server_t *server, size_t item_count) {
    if (item_count > MU_MAX_MONITORED_ITEMS) {
        return MU_STATUS_BAD_TOOMANYMONITOREDITEMS;
    }

    mu_subscription_t *sub = NULL;
    opcua_statuscode_t status = mu_subscription_create(&server->subs, 1u, 100u, 30u, 10u, 0u, 0u, true, 0u, &sub);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    for (size_t i = 0; i < item_count; ++i) {
        mu_monitored_item_t *item = NULL;
        status = mu_monitored_item_alloc(&server->subs, sub->subscription_id, &item);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        item->resolved_node = &bench_nodes[i % MAX_BENCH_NODES];
        item->node_id = item->resolved_node->node_id;
        item->attribute_id = BENCH_ATTRIBUTE_VALUE;
        item->sampling_interval_ms = 1u;
        item->next_sample_ms = 1u;
        item->monitoring_mode = MU_MONITORING_MODE_REPORTING;
        item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        item->last_status = MU_STATUS_GOOD;
#ifdef MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        item->queue_size = MU_MONITORED_QUEUE_DEPTH;
        item->discard_oldest = true;
#endif
    }

    return MU_STATUS_GOOD;
}
#endif

static int run_message_chunk_header_benchmark(const bench_options_t *options, uint64_t *elapsed_ns,
                                              uint64_t *actual_iterations) {
    uint64_t start_ns;
    uint64_t end_ns;

    for (uint32_t i = 0; i < options->warmup; ++i) {
        if (run_message_chunk_header_once(options->batch) != 0) {
            fprintf(stderr, "warmup message chunk header failed\n");
            return 1;
        }
    }

    start_ns = monotonic_ns();
    if (start_ns == 0u) {
        return 1;
    }

    uint64_t min_ns = (uint64_t)options->min_ms * 1000000ull;
    uint64_t done = 0u;
    uint64_t current_elapsed = 0u;
    do {
        if (run_message_chunk_header_once(options->batch) != 0) {
            fprintf(stderr, "measured message chunk header failed\n");
            return 1;
        }
        done++;
        end_ns = monotonic_ns();
        if (end_ns == 0u || end_ns < start_ns) {
            return 1;
        }
        current_elapsed = end_ns - start_ns;
    } while (done < (uint64_t)options->iterations || current_elapsed < min_ns);

    *elapsed_ns = current_elapsed;
    *actual_iterations = done;
    return 0;
}

static int run_binary_primitive_benchmark(const bench_options_t *options, uint64_t *elapsed_ns,
                                          uint64_t *actual_iterations) {
    uint64_t start_ns;
    uint64_t end_ns;

    for (uint32_t i = 0; i < options->warmup; ++i) {
        if (run_binary_primitive_once(options->batch) != 0) {
            fprintf(stderr, "warmup binary primitive failed\n");
            return 1;
        }
    }

    start_ns = monotonic_ns();
    if (start_ns == 0u) {
        return 1;
    }

    uint64_t min_ns = (uint64_t)options->min_ms * 1000000ull;
    uint64_t done = 0u;
    uint64_t current_elapsed = 0u;
    do {
        if (run_binary_primitive_once(options->batch) != 0) {
            fprintf(stderr, "measured binary primitive failed\n");
            return 1;
        }
        done++;
        end_ns = monotonic_ns();
        if (end_ns == 0u || end_ns < start_ns) {
            return 1;
        }
        current_elapsed = end_ns - start_ns;
    } while (done < (uint64_t)options->iterations || current_elapsed < min_ns);

    *elapsed_ns = current_elapsed;
    *actual_iterations = done;
    return 0;
}

static int run_uasc_response_framing_benchmark(const bench_options_t *options, uint64_t *elapsed_ns,
                                               uint64_t *actual_iterations) {
    uint64_t start_ns;
    uint64_t end_ns;

    for (uint32_t i = 0; i < options->warmup; ++i) {
        if (run_uasc_response_framing_once(options->batch) != 0) {
            fprintf(stderr, "warmup UASC response framing failed\n");
            return 1;
        }
    }

    start_ns = monotonic_ns();
    if (start_ns == 0u) {
        return 1;
    }

    uint64_t min_ns = (uint64_t)options->min_ms * 1000000ull;
    uint64_t done = 0u;
    uint64_t current_elapsed = 0u;
    do {
        if (run_uasc_response_framing_once(options->batch) != 0) {
            fprintf(stderr, "measured UASC response framing failed\n");
            return 1;
        }
        done++;
        end_ns = monotonic_ns();
        if (end_ns == 0u || end_ns < start_ns) {
            return 1;
        }
        current_elapsed = end_ns - start_ns;
    } while (done < (uint64_t)options->iterations || current_elapsed < min_ns);

    *elapsed_ns = current_elapsed;
    *actual_iterations = done;
    return 0;
}

static int run_benchmark(const bench_options_t *options, uint64_t *elapsed_ns, uint64_t *actual_iterations) {
    mu_server_t *server = NULL;
    opcua_uint32_t request_id = 0;
    opcua_uint32_t response_id = 0;
    size_t request_len = 0;
    uint64_t start_ns;
    uint64_t end_ns;

    if (options->scenario == SCENARIO_MESSAGE_CHUNK_HEADER) {
        return run_message_chunk_header_benchmark(options, elapsed_ns, actual_iterations);
    }
    if (options->scenario == SCENARIO_BINARY_PRIMITIVE) {
        return run_binary_primitive_benchmark(options, elapsed_ns, actual_iterations);
    }
    if (options->scenario == SCENARIO_UASC_RESPONSE_FRAMING) {
        return run_uasc_response_framing_benchmark(options, elapsed_ns, actual_iterations);
    }

    opcua_statuscode_t status = init_server(options->nodes, &server);
    if (status != MU_STATUS_GOOD) {
        fprintf(stderr, "server init failed: 0x%08" PRIx32 "\n", (uint32_t)status);
        return 1;
    }

    status = build_request(options, &request_len, &request_id, &response_id);
    if (status != MU_STATUS_GOOD) {
        fprintf(stderr, "request build failed: 0x%08" PRIx32 "\n", (uint32_t)status);
        return 1;
    }

#ifdef MUC_OPCUA_SUBSCRIPTIONS
    if (options->scenario == SCENARIO_SUBSCRIPTION_ACTIVE_TICK) {
        status = setup_active_subscription(server, options->batch);
        if (status != MU_STATUS_GOOD) {
            fprintf(stderr, "subscription setup failed: 0x%08" PRIx32 "\n", (uint32_t)status);
            return 1;
        }
    }
#endif

    for (uint32_t i = 0; i < options->warmup; ++i) {
        if (options->scenario == SCENARIO_SUBSCRIPTION_IDLE_TICK ||
            options->scenario == SCENARIO_SUBSCRIPTION_ACTIVE_TICK) {
#ifdef MUC_OPCUA_SUBSCRIPTIONS
            mu_subscriptions_tick(server, (opcua_uint64_t)i + 1u);
#endif
        } else if (run_dispatch_once(server, request_id, request_len, response_id) != 0) {
            fprintf(stderr, "warmup dispatch failed\n");
            return 1;
        }
    }

    start_ns = monotonic_ns();
    if (start_ns == 0u) {
        return 1;
    }

    uint64_t min_ns = (uint64_t)options->min_ms * 1000000ull;
    uint64_t done = 0u;
    uint64_t current_elapsed = 0u;
    do {
        if (options->scenario == SCENARIO_SUBSCRIPTION_IDLE_TICK ||
            options->scenario == SCENARIO_SUBSCRIPTION_ACTIVE_TICK) {
#ifdef MUC_OPCUA_SUBSCRIPTIONS
            for (size_t n = 0; n < options->nodes; ++n) {
                bench_values[n]++;
            }
            mu_subscriptions_tick(server, (opcua_uint64_t)done + (opcua_uint64_t)options->warmup + 1u);
#endif
        } else if (run_dispatch_once(server, request_id, request_len, response_id) != 0) {
            fprintf(stderr, "measured dispatch failed\n");
            return 1;
        }
        done++;
        end_ns = monotonic_ns();
        if (end_ns == 0u || end_ns < start_ns) {
            return 1;
        }
        current_elapsed = end_ns - start_ns;
    } while (done < (uint64_t)options->iterations || current_elapsed < min_ns);

    *elapsed_ns = current_elapsed;
    *actual_iterations = done;
    return 0;
}

static void print_unsupported(const bench_options_t *options) {
    printf("{\n"
           "  \"scenario\": \"%s\",\n"
           "  \"nodes\": %zu,\n"
           "  \"batch\": %zu,\n"
           "  \"iterations\": %" PRIu32 ",\n"
           "  \"warmup\": %" PRIu32 ",\n"
           "  \"min_ms\": %" PRIu32 ",\n"
           "  \"supported\": false,\n"
           "  \"elapsed_ns\": 0,\n"
           "  \"ops_per_sec\": 0.0,\n"
           "  \"items_per_sec\": 0.0\n"
           "}\n",
           options->scenario_name, options->nodes, options->batch, options->iterations, options->warmup,
           options->min_ms);
}

int main(int argc, char **argv) {
    bench_options_t options;
    uint64_t elapsed_ns = 0u;
    uint64_t actual_iterations = 0u;

    if (parse_options(argc, argv, &options) != 0) {
        return 2;
    }

    if (!scenario_supported(options.scenario)) {
        print_unsupported(&options);
        return 0;
    }

    if (run_benchmark(&options, &elapsed_ns, &actual_iterations) != 0) {
        return 1;
    }

    double seconds = (double)elapsed_ns / 1000000000.0;
    double ops_per_sec = seconds > 0.0 ? (double)actual_iterations / seconds : 0.0;
    double items_per_sec = ops_per_sec * (double)options.batch;
    uint32_t calibration_ms = options.min_ms >= 80u ? 20u : (options.min_ms > 0u ? options.min_ms : 1u);
    double calibration_ops_per_sec = run_calibration(calibration_ms);
    double normalized_ops_per_sec = calibration_ops_per_sec > 0.0 ? ops_per_sec / calibration_ops_per_sec : ops_per_sec;

    printf("{\n"
           "  \"scenario\": \"%s\",\n"
           "  \"nodes\": %zu,\n"
           "  \"batch\": %zu,\n"
           "  \"requested_iterations\": %" PRIu32 ",\n"
           "  \"iterations\": %" PRIu64 ",\n"
           "  \"warmup\": %" PRIu32 ",\n"
           "  \"min_ms\": %" PRIu32 ",\n"
           "  \"supported\": true,\n"
           "  \"elapsed_ns\": %" PRIu64 ",\n"
           "  \"ops_per_sec\": %.3f,\n"
           "  \"items_per_sec\": %.3f,\n"
           "  \"calibration_ms\": %" PRIu32 ",\n"
           "  \"calibration_ops_per_sec\": %.3f,\n"
           "  \"normalized_ops_per_sec\": %.12f\n"
           "}\n",
           options.scenario_name, options.nodes, options.batch, options.iterations, actual_iterations, options.warmup,
           options.min_ms, elapsed_ns, ops_per_sec, items_per_sec, calibration_ms, calibration_ops_per_sec,
           normalized_ops_per_sec);

    return 0;
}
