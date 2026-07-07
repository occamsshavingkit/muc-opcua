/* src/client/client.c */
#include "client_internal.h"
#include <string.h>

static opcua_boolean_t config_valid(const mu_client_config_t *config) {
    return config != NULL && config->endpoint_url != NULL && config->send_buffer != NULL &&
           config->send_buffer_size > 0 && config->receive_buffer != NULL && config->receive_buffer_size > 0;
}

opcua_statuscode_t mu_client_init(mu_client_t *client, const mu_client_config_t *config,
                                  mu_client_transport_t *transport) {
    if (client == NULL || !config_valid(config) || transport == NULL || transport->connect == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    (void)memset(client, 0, sizeof(*client));
    client->config = *config;
    if (client->config.timeout_ms == 0) {
        client->config.timeout_ms = MU_CLIENT_DEFAULT_TIMEOUT_MS;
    }
    client->transport = transport;
    client->state = MU_CLIENT_DISCONNECTED;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_client_connect(mu_client_t *client) {
    if (client == NULL || client->transport == NULL || client->state != MU_CLIENT_DISCONNECTED) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    opcua_statuscode_t st = mu_client_state_transition(client, MU_CLIENT_RESOLVING);
    if (st != MU_STATUS_GOOD) {
        return st;
    }
    st = mu_client_state_transition(client, MU_CLIENT_CONNECTING);
    if (st != MU_STATUS_GOOD) {
        return st;
    }

    st = client->transport->connect(client->transport->context, client->config.endpoint_url, client->config.timeout_ms);
    if (st != MU_STATUS_GOOD) {
        client->state = MU_CLIENT_ERROR;
        client->last_error = st;
        return st;
    }

    static const opcua_byte_t hello_marker[3] = {'H', 'E', 'L'};
    if (client->transport->send != NULL) {
        size_t sent = 0;
        st = client->transport->send(client->transport->context, hello_marker, sizeof(hello_marker), &sent,
                                     client->config.timeout_ms);
        if (st != MU_STATUS_GOOD || sent != sizeof(hello_marker)) {
            client->state = MU_CLIENT_ERROR;
            client->last_error = st == MU_STATUS_GOOD ? MU_STATUS_BAD_CONNECTIONCLOSED : st;
            return client->last_error;
        }
    }

    return mu_client_open_secure_channel(client);
}

opcua_statuscode_t mu_client_disconnect(mu_client_t *client) {
    if (client == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (client->state == MU_CLIENT_ACTIVE) {
        opcua_statuscode_t st = mu_client_close_session(client);
        if (st != MU_STATUS_GOOD) {
            return st;
        }
    }
    (void)mu_client_close_secure_channel(client);
    if (client->transport != NULL && client->transport->close != NULL) {
        client->transport->close(client->transport->context);
    }
    client->state = MU_CLIENT_DISCONNECTED;
    client->last_error = MU_STATUS_GOOD;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_client_get_endpoints(mu_client_t *client, mu_endpoint_description_t *endpoints,
                                           size_t *num_endpoints) {
    if (client == NULL || endpoints == NULL || num_endpoints == NULL || *num_endpoints == 0) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (!client->channel.open) {
        return MU_STATUS_BAD_SECURECHANNELIDINVALID;
    }

    endpoints[0].endpoint_url = client->config.endpoint_url;
    endpoints[0].security_policy = client->config.security_policy;
    endpoints[0].security_policy_uri = client->config.security_policy == MU_CLIENT_SECURITY_BASIC256SHA256
                                           ? "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256"
                                           : "http://opcfoundation.org/UA/SecurityPolicy#None";
    *num_endpoints = 1;
    return MU_STATUS_GOOD;
}
