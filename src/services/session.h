/* src/services/session.h */
#ifndef MICRO_OPCUA_SERVICES_SESSION_H
#define MICRO_OPCUA_SERVICES_SESSION_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include <stddef.h>

typedef enum {
    MU_SESSION_STATE_CLOSED = 0,
    MU_SESSION_STATE_CREATED,
    MU_SESSION_STATE_ACTIVATED
} mu_session_state_t;

typedef struct {
    mu_session_state_t state;
    opcua_uint32_t session_id;
    opcua_uint32_t auth_token;
    double revised_session_timeout;
    
    opcua_byte_t server_nonce[32];
    size_t server_nonce_length;
} mu_session_t;

void mu_session_init(mu_session_t *session);

opcua_statuscode_t mu_session_create(mu_session_t *session, 
                                     double requested_session_timeout, 
                                     double *revised_session_timeout,
                                     opcua_uint32_t *session_id,
                                     opcua_uint32_t *auth_token);

opcua_statuscode_t mu_session_activate(mu_session_t *session, 
                                       opcua_uint32_t auth_token,
                                       opcua_uint32_t identity_token_encoding_id);

opcua_statuscode_t mu_session_close(mu_session_t *session,
                                    opcua_uint32_t auth_token,
                                    bool delete_subscriptions);

#endif /* MICRO_OPCUA_SERVICES_SESSION_H */
