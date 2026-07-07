/* src/client/client_internal.h */
#ifndef MUC_OPCUA_CLIENT_INTERNAL_H
#define MUC_OPCUA_CLIENT_INTERNAL_H

#include "muc_opcua/client.h"

opcua_statuscode_t mu_client_open_secure_channel(mu_client_t *client);
opcua_statuscode_t mu_client_close_secure_channel(mu_client_t *client);

#endif /* MUC_OPCUA_CLIENT_INTERNAL_H */
