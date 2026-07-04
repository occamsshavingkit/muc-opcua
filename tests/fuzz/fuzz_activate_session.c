/* tests/fuzz/fuzz_activate_session.c */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/core/server_internal.h"
#include "muc_opcua/muc_opcua.h"

#define FUZZ_REQUEST_CAPACITY MU_DEFAULT_MAX_MESSAGE_SIZE
#define FUZZ_RESPONSE_CAPACITY 512u

static opcua_statuscode_t fuzz_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer == NULL && length != 0u) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (buffer != NULL) {
        (void)memset(buffer, 0xa5, length);
    }
    return MU_STATUS_GOOD;
}

static bool prepare_activate_session_server(mu_server_t *server) {
    opcua_uint32_t revised_lifetime = 0;
    opcua_uint64_t revised_timeout = 0;
    opcua_uint32_t session_id = 0;
    opcua_uint32_t auth_token = 0;

    (void)memset(server, 0, sizeof(*server));
    server->config.entropy_adapter.generate_random = fuzz_entropy;

    mu_secure_channel_init(&server->secure_channel);
    if (mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 3600000u,
                               &revised_lifetime) != MU_STATUS_GOOD) {
        return false;
    }

    for (size_t i = 0; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }

    return mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token) == MU_STATUS_GOOD;
}

static opcua_statuscode_t write_activate_prefix(opcua_byte_t *buffer, size_t capacity, size_t *prefix_length) {
    mu_binary_writer_t writer;
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {12345u}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0u}};
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    opcua_statuscode_t status;

    mu_binary_writer_init(&writer, buffer, capacity);

    status = mu_binary_write_nodeid(&writer, &auth_token);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_write_int64(&writer, 0);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_write_uint32(&writer, 0);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_uint32(&writer, 0);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_string(&writer, &null_string);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_uint32(&writer, 0);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_extension_object_header(&writer, &null_id, 0);
    if (status != MU_STATUS_GOOD)
        return status;

    status = mu_binary_write_string(&writer, &null_string);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_bytestring(&writer, &null_bytes);
    if (status != MU_STATUS_GOOD)
        return status;

    *prefix_length = writer.position;
    return MU_STATUS_GOOD;
}

static size_t build_structured_request(const uint8_t *data, size_t size, opcua_byte_t *request, size_t capacity) {
    /* OPC-10000-4 5.7.3.2 fields after ClientSignature:
       ClientSoftwareCertificates[], LocaleIds[], UserIdentityToken,
       UserTokenSignature. The seed keeps those frames present while fuzz bytes
       mutate their encoded lengths, NodeIds, masks, and payloads. */
    static const opcua_byte_t seed_tail[] = {
        0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x41, 0x42, 0x43, 0x02, 0x00, 0x00, 0x00, 0x58, 0x59,
        0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x65, 0x6e, 0x2d, 0x55, 0x53, 0x01, 0x00, 0x41, 0x01,
        0x01, 0x0d, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x61, 0x6e, 0x6f, 0x6e, 0x79, 0x6d, 0x6f, 0x75,
        0x73, 0x06, 0x00, 0x00, 0x00, 0x73, 0x69, 0x67, 0x61, 0x6c, 0x67, 0x01, 0x00, 0x00, 0x00, 0xab};
    size_t prefix_length = 0;
    size_t tail_capacity;
    size_t tail_length;
    size_t overlay_length;

    if (write_activate_prefix(request, capacity, &prefix_length) != MU_STATUS_GOOD) {
        return 0;
    }

    tail_capacity = capacity - prefix_length;
    tail_length = sizeof(seed_tail);
    if (tail_length > tail_capacity) {
        tail_length = tail_capacity;
    }
    (void)memcpy(request + prefix_length, seed_tail, tail_length);

    overlay_length = size;
    if (overlay_length > tail_capacity) {
        overlay_length = tail_capacity;
    }
    if (overlay_length != 0u) {
        (void)memcpy(request + prefix_length, data, overlay_length);
    }

    if (overlay_length > tail_length) {
        tail_length = overlay_length;
    }
    return prefix_length + tail_length;
}

static void dispatch_activate_session_body(const opcua_byte_t *request_body, size_t request_length) {
    mu_server_t server;
    opcua_byte_t response_body[FUZZ_RESPONSE_CAPACITY];
    size_t response_length = sizeof(response_body);

    if (!prepare_activate_session_server(&server)) {
        return;
    }

    (void)mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, request_body, request_length, response_body,
                              &response_length);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const opcua_byte_t empty_body = 0;
    opcua_byte_t structured_request[FUZZ_REQUEST_CAPACITY];
    size_t raw_length = size;
    size_t structured_length;

    if (data == NULL) {
        data = &empty_body;
        size = 0u;
        raw_length = 0u;
    }

    if (raw_length > FUZZ_REQUEST_CAPACITY) {
        raw_length = FUZZ_REQUEST_CAPACITY;
    }

    dispatch_activate_session_body(data, raw_length);

    structured_length = build_structured_request(data, size, structured_request, sizeof(structured_request));
    if (structured_length != 0u) {
        dispatch_activate_session_body(structured_request, structured_length);
    }

    return 0;
}
