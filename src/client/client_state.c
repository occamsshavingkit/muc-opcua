/* src/client/client_state.c */
#include "muc_opcua/client.h"

static opcua_boolean_t transition_allowed(mu_client_state_t current, mu_client_state_t next) {
    if (next == MU_CLIENT_ERROR || next == MU_CLIENT_DISCONNECTED) {
        return true;
    }

    switch (current) {
    case MU_CLIENT_DISCONNECTED:
        return next == MU_CLIENT_RESOLVING;
    case MU_CLIENT_RESOLVING:
        return next == MU_CLIENT_CONNECTING;
    case MU_CLIENT_CONNECTING:
        return next == MU_CLIENT_CHANNEL_OPENING;
    case MU_CLIENT_CHANNEL_OPENING:
        return next == MU_CLIENT_CHANNEL_OPEN;
    case MU_CLIENT_CHANNEL_OPEN:
        return next == MU_CLIENT_SESSION_CREATING || next == MU_CLIENT_CLOSING;
    case MU_CLIENT_SESSION_CREATING:
        return next == MU_CLIENT_ACTIVATING || next == MU_CLIENT_CHANNEL_OPEN;
    case MU_CLIENT_ACTIVATING:
        return next == MU_CLIENT_ACTIVE || next == MU_CLIENT_CHANNEL_OPEN;
    case MU_CLIENT_ACTIVE:
        return next == MU_CLIENT_CLOSING || next == MU_CLIENT_CHANNEL_OPEN;
    case MU_CLIENT_CLOSING:
        return next == MU_CLIENT_CHANNEL_OPEN || next == MU_CLIENT_DISCONNECTED;
    case MU_CLIENT_ERROR:
        return next == MU_CLIENT_DISCONNECTED;
    default:
        return false;
    }
}

opcua_statuscode_t mu_client_state_transition(mu_client_t *client, mu_client_state_t next_state) {
    if (client == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (!transition_allowed(client->state, next_state)) {
        client->state = MU_CLIENT_ERROR;
        client->last_error = MU_STATUS_BAD_INVALIDARGUMENT;
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    client->state = next_state;
    return MU_STATUS_GOOD;
}

opcua_boolean_t mu_client_state_is_connected(mu_client_state_t state) {
    return state == MU_CLIENT_CHANNEL_OPEN || state == MU_CLIENT_SESSION_CREATING || state == MU_CLIENT_ACTIVATING ||
           state == MU_CLIENT_ACTIVE || state == MU_CLIENT_CLOSING;
}

const char *mu_client_state_name(mu_client_state_t state) {
    switch (state) {
    case MU_CLIENT_DISCONNECTED:
        return "DISCONNECTED";
    case MU_CLIENT_RESOLVING:
        return "RESOLVING";
    case MU_CLIENT_CONNECTING:
        return "CONNECTING";
    case MU_CLIENT_CHANNEL_OPENING:
        return "CHANNEL_OPENING";
    case MU_CLIENT_CHANNEL_OPEN:
        return "CHANNEL_OPEN";
    case MU_CLIENT_SESSION_CREATING:
        return "SESSION_CREATING";
    case MU_CLIENT_ACTIVATING:
        return "ACTIVATING";
    case MU_CLIENT_ACTIVE:
        return "ACTIVE";
    case MU_CLIENT_CLOSING:
        return "CLOSING";
    case MU_CLIENT_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
