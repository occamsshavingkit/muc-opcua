/* src/services/session.c */
#include "session.h"
#include <stddef.h>

void mu_session_init(mu_session_t *session) {
    if (session) {
        session->state = MU_SESSION_STATE_CLOSED;
        session->session_id = 0;
        session->auth_token = 0;
        session->revised_session_timeout = 0.0;
        session->server_nonce_length = 32;
        for (size_t i = 0; i < 32; ++i) {
            session->server_nonce[i] = (opcua_byte_t)i; /* Simple mock entropy */
        }
    }
}

opcua_statuscode_t mu_session_create(mu_session_t *session, 
                                     double requested_session_timeout, 
                                     double *revised_session_timeout,
                                     opcua_uint32_t *session_id,
                                     opcua_uint32_t *auth_token)
{
    if (!session || !revised_session_timeout || !session_id || !auth_token) return MU_STATUS_BAD_INTERNALERROR;

    if (session->state != MU_SESSION_STATE_CLOSED) {
        return MU_STATUS_BAD_INTERNALERROR; /* Only 1 session in minimal server */
    }

    session->session_id = 1;
    session->auth_token = 12345; /* Mock token */
    
    double timeout = requested_session_timeout;
    if (timeout < 10000.0) timeout = 10000.0;
    if (timeout > 3600000.0) timeout = 3600000.0;

    session->revised_session_timeout = timeout;
    session->state = MU_SESSION_STATE_CREATED;

    *revised_session_timeout = timeout;
    *session_id = session->session_id;
    *auth_token = session->auth_token;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_session_activate(mu_session_t *session, 
                                       opcua_uint32_t auth_token,
                                       opcua_uint32_t identity_token_encoding_id)
{
    if (!session) return MU_STATUS_BAD_INTERNALERROR;
    
    if (session->state != MU_SESSION_STATE_CREATED && session->state != MU_SESSION_STATE_ACTIVATED) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }
    
    if (session->auth_token != auth_token) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }
    
    /* We only support anonymous identity tokens per profile */
    if (identity_token_encoding_id != 321) { /* MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY */
        return MU_STATUS_BAD_IDENTITYTOKENINVALID;
    }
    
    session->state = MU_SESSION_STATE_ACTIVATED;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_session_close(mu_session_t *session,
                                    opcua_uint32_t auth_token,
                                    bool delete_subscriptions)
{
    if (!session) return MU_STATUS_BAD_INTERNALERROR;
    (void)delete_subscriptions; /* Not supported yet */
    
    if (session->state == MU_SESSION_STATE_CLOSED) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }
    
    if (session->auth_token != auth_token) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }
    
    mu_session_init(session); /* Revert back to closed state */
    return MU_STATUS_GOOD;
}
