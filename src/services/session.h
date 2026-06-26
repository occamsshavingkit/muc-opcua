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
    /* RevisedSessionTimeout as the raw IEEE-754 bits of the Duration (ms). Kept as
       bits so the value is clamped and echoed with no floating-point math (the
       Cortex-M0+ target has no FPU). */
    opcua_uint64_t revised_session_timeout_bits;
} mu_session_t;

void mu_session_init(mu_session_t *session);

/* Create the (single) session. The requested/revised SessionTimeout are passed as
   the raw IEEE-754 bits of the wire Duration; the value is clamped to
   [10000, 3600000] ms via integer bit comparison (valid for positive doubles). */
opcua_statuscode_t mu_session_create(mu_session_t *session,
                                     opcua_uint64_t requested_timeout_bits,
                                     opcua_uint64_t *revised_timeout_bits,
                                     opcua_uint32_t *session_id,
                                     opcua_uint32_t *auth_token);

opcua_statuscode_t mu_session_activate(mu_session_t *session, 
                                       opcua_uint32_t auth_token,
                                       opcua_uint32_t identity_token_encoding_id);

opcua_statuscode_t mu_session_close(mu_session_t *session,
                                    opcua_uint32_t auth_token,
                                    bool delete_subscriptions);

#endif /* MICRO_OPCUA_SERVICES_SESSION_H */
