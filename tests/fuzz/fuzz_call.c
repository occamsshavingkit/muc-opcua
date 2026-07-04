/* tests/fuzz/fuzz_call.c
 *
 * Fuzz the Feature 005 US3 Call decoder/routing path. The harness prefixes a valid
 * RequestHeader and feeds arbitrary CallMethodRequest bytes into the Call service.
 *
 * OPC-10000-4 5.12.2.2: Call Service.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/core/server_internal.h"
#include "muc_opcua/muc_opcua.h"

#define FUZZ_BODY_CAPACITY MU_DEFAULT_MAX_MESSAGE_SIZE
#define FUZZ_RESPONSE_CAPACITY 1024u

static opcua_statuscode_t fuzz_write(void *context, void *handle, const opcua_byte_t *buffer, size_t length,
                                     size_t *written) {
    (void)context;
    (void)handle;
    (void)buffer;
    *written = length;
    return MU_STATUS_GOOD;
}

static void fuzz_close(void *context, void *handle) {
    (void)context;
    (void)handle;
}

static int prepare_server(mu_server_t *server, opcua_byte_t *send_buffer) {
    opcua_uint32_t revised_lifetime = 0u;
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;

    (void)memset(server, 0, sizeof(*server));
    server->config.send_buffer = send_buffer;
    server->config.send_buffer_size = FUZZ_BODY_CAPACITY;
    server->config.tcp_adapter.write = fuzz_write;
    server->config.tcp_adapter.close_connection = fuzz_close;
    server->client_handle = (void *)1;

    mu_secure_channel_init(&server->secure_channel);
    if (mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE, 3600000u,
                               &revised_lifetime) != MU_STATUS_GOOD) {
        return 0;
    }
    for (size_t i = 0u; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    if (mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token) != MU_STATUS_GOOD) {
        return 0;
    }
    if (mu_session_activate(&server->sessions[0], auth_token, MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY) !=
        MU_STATUS_GOOD) {
        return 0;
    }

#if MUC_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_init(&server->subs);
    {
        mu_subscription_t *sub = NULL;
        (void)mu_subscription_create(&server->subs, session_id, 100u, 30u, 10u, 0u, 0u, true, 0u, &sub);
    }
#endif
    return 1;
}

static opcua_statuscode_t write_request_header(mu_binary_writer_t *writer) {
    mu_nodeid_t token = {0u, MU_NODEID_NUMERIC, {12345u}};
    mu_nodeid_t null_node = {0u, MU_NODEID_NUMERIC, {0u}};
    mu_string_t null_string = {-1, NULL};
    opcua_statuscode_t status;

    status = mu_binary_write_nodeid(writer, &token);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_write_int64(writer, 0);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_uint32(writer, 1u);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_uint32(writer, 0u);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_string(writer, &null_string);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_write_uint32(writer, 0u);
    if (status != MU_STATUS_GOOD)
        return status;
    return mu_binary_write_extension_object_header(writer, &null_node, 0u);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const uint8_t empty = 0u;
    mu_server_t server;
    opcua_byte_t send_buffer[FUZZ_BODY_CAPACITY];
    opcua_byte_t request_body[FUZZ_BODY_CAPACITY];
    opcua_byte_t response_body[FUZZ_RESPONSE_CAPACITY];
    size_t response_length = sizeof(response_body);
    mu_binary_writer_t writer;

    if (data == NULL) {
        data = &empty;
        size = 0u;
    }
    if (!prepare_server(&server, send_buffer)) {
        return 0;
    }

    mu_binary_writer_init(&writer, request_body, sizeof(request_body));
    if (write_request_header(&writer) != MU_STATUS_GOOD) {
        return 0;
    }
    if (size > sizeof(request_body) - writer.position) {
        size = sizeof(request_body) - writer.position;
    }
    if (size > 0u) {
        (void)memcpy(request_body + writer.position, data, size);
        writer.position += size;
    }

    (void)mu_service_dispatch(&server, MU_ID_CALLREQUEST, request_body, writer.position, response_body,
                              &response_length);
    return 0;
}
