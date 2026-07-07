/* src/client/client_session.c */
#include "client_internal.h"
#include <string.h>

opcua_statuscode_t mu_client_create_session(mu_client_t *client) {
    if (client == NULL || client->state != MU_CLIENT_CHANNEL_OPEN || !client->channel.open) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }

    opcua_statuscode_t st = mu_client_state_transition(client, MU_CLIENT_SESSION_CREATING);
    if (st != MU_STATUS_GOOD) {
        return st;
    }

    client->session.session_id.namespace_index = 0;
    client->session.session_id.identifier_type = MU_NODEID_NUMERIC;
    client->session.session_id.identifier.numeric = 1;
    client->session.authentication_token.namespace_index = 0;
    client->session.authentication_token.identifier_type = MU_NODEID_NUMERIC;
    client->session.authentication_token.identifier.numeric = 1001;
    client->session.active = false;
    return mu_client_state_transition(client, MU_CLIENT_ACTIVATING);
}

opcua_statuscode_t mu_client_activate_session(mu_client_t *client, const mu_identity_token_t *identity) {
    if (client == NULL || client->state != MU_CLIENT_ACTIVATING) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }
    if (identity != NULL && identity->type != MU_IDENTITY_ANONYMOUS) {
        client->state = MU_CLIENT_ERROR;
        client->last_error = MU_STATUS_BAD_IDENTITYTOKENINVALID;
        return MU_STATUS_BAD_IDENTITYTOKENINVALID;
    }

    client->session.active = true;
    return mu_client_state_transition(client, MU_CLIENT_ACTIVE);
}

opcua_statuscode_t mu_client_close_session(mu_client_t *client) {
    if (client == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (!client->session.active && client->state != MU_CLIENT_ACTIVE) {
        return MU_STATUS_GOOD;
    }

    opcua_statuscode_t st = mu_client_state_transition(client, MU_CLIENT_CLOSING);
    if (st != MU_STATUS_GOOD) {
        return st;
    }
    (void)memset(&client->session, 0, sizeof(client->session));
    return mu_client_state_transition(client, MU_CLIENT_CHANNEL_OPEN);
}
