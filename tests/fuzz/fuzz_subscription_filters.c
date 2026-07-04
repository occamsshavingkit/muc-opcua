/* tests/fuzz/fuzz_subscription_filters.c
 *
 * Fuzz the Standard DataChange Subscription 2017 facet decoders (feature 005):
 * the DataChangeFilter / deadband decode in CreateMonitoredItems (OPC-10000-4 §7.22.2)
 * and the SetTriggering request decode (§5.13.5). The harness drives raw fuzz bytes
 * straight into mu_service_dispatch so malformed arrays, lengths, NodeIds, and filter
 * bodies must be rejected with a StatusCode and never read out of bounds.
 *
 * Meaningful coverage requires the library to be built with
 * MUC_OPCUA_SUBSCRIPTIONS_STANDARD (and MUC_OPCUA_SUBSCRIPTIONS); without it the
 * dispatch simply reports the service unsupported, which is still crash-free.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/core/server_internal.h"
#include "muc_opcua/muc_opcua.h"

#define FUZZ_REQUEST_CAPACITY MU_DEFAULT_MAX_MESSAGE_SIZE
#define FUZZ_RESPONSE_CAPACITY 1024u

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

/* Build a server with an open secure channel, one active session, and (when the engine
   supports it) one subscription so SetTriggering/CreateMonitoredItems have a target. */
static bool prepare_server(mu_server_t *server, opcua_uint32_t *session_id_out) {
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
    if (mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token) != MU_STATUS_GOOD) {
        return false;
    }
    *session_id_out = session_id;

#if MUC_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_init(&server->subs);
    {
        mu_subscription_t *sub = NULL;
        (void)mu_subscription_create(&server->subs, session_id, 200u, 100u, 10u, 0u, 0u, true, 0u, &sub);
    }
#endif
    return true;
}

static void dispatch_one(opcua_uint32_t request_type, const opcua_byte_t *body, size_t len) {
    mu_server_t server;
    opcua_uint32_t session_id = 0;
    opcua_byte_t response_body[FUZZ_RESPONSE_CAPACITY];
    size_t response_length = sizeof(response_body);

    if (!prepare_server(&server, &session_id)) {
        return;
    }
    (void)mu_service_dispatch(&server, request_type, body, len, response_body, &response_length);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const opcua_byte_t empty = 0;
    size_t len = size;

    if (data == NULL) {
        data = &empty;
        len = 0u;
    }
    if (len > FUZZ_REQUEST_CAPACITY) {
        len = FUZZ_REQUEST_CAPACITY;
    }

    /* CreateMonitoredItems exercises the DataChangeFilter/deadband decode (§7.22.2). */
    dispatch_one(MU_ID_CREATEMONITOREDITEMSREQUEST, data, len);
#ifdef MU_ID_SETTRIGGERINGREQUEST
    /* SetTriggering exercises the §5.13.5 array decode. */
    dispatch_one(MU_ID_SETTRIGGERINGREQUEST, data, len);
#endif
    return 0;
}
