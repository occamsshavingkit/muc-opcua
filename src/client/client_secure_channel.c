/* src/client/client_secure_channel.c */
#include "client_internal.h"
#include <string.h>

opcua_statuscode_t mu_client_open_secure_channel(mu_client_t *client) {
    if (client == NULL || client->transport == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    opcua_statuscode_t st = mu_client_state_transition(client, MU_CLIENT_CHANNEL_OPENING);
    if (st != MU_STATUS_GOOD) {
        return st;
    }

    /* Minimal OPN marker for the transport adapter boundary. Full service-body
       encoding remains isolated behind this call site per OPC-10000-4 §5.5.2. */
    static const opcua_byte_t opn_marker[3] = {'O', 'P', 'N'};
    size_t sent = 0;
    if (client->transport->send != NULL) {
        st = client->transport->send(client->transport->context, opn_marker, sizeof(opn_marker), &sent,
                                     client->config.timeout_ms);
        if (st != MU_STATUS_GOOD || sent != sizeof(opn_marker)) {
            client->state = MU_CLIENT_ERROR;
            client->last_error = st == MU_STATUS_GOOD ? MU_STATUS_BAD_CONNECTIONCLOSED : st;
            return client->last_error;
        }
    }

    if (client->transport->recv != NULL && client->config.receive_buffer != NULL &&
        client->config.receive_buffer_size > 0) {
        size_t received = 0;
        st = client->transport->recv(client->transport->context, client->config.receive_buffer,
                                     client->config.receive_buffer_size, &received, client->config.timeout_ms);
        if (st != MU_STATUS_GOOD) {
            client->state = MU_CLIENT_ERROR;
            client->last_error = st;
            return st;
        }
    }

    client->channel.channel_id = 1;
    client->channel.token_id = 1;
    client->channel.sequence_number = 1;
    client->channel.open = true;
    return mu_client_state_transition(client, MU_CLIENT_CHANNEL_OPEN);
}

opcua_statuscode_t mu_client_close_secure_channel(mu_client_t *client) {
    if (client == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (!client->channel.open) {
        return MU_STATUS_GOOD;
    }

    static const opcua_byte_t close_marker[3] = {'C', 'L', 'O'};
    if (client->transport != NULL && client->transport->send != NULL) {
        size_t sent = 0;
        opcua_statuscode_t st = client->transport->send(client->transport->context, close_marker, sizeof(close_marker),
                                                        &sent, client->config.timeout_ms);
        if (st != MU_STATUS_GOOD) {
            client->state = MU_CLIENT_ERROR;
            client->last_error = st;
            return st;
        }
    }

    (void)memset(&client->channel, 0, sizeof(client->channel));
    return MU_STATUS_GOOD;
}
