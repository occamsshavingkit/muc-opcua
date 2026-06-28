/* src/services/session.c */
#include "session.h"
#include <stddef.h>
#include <string.h>

/* IEEE-754 bit patterns of the SessionTimeout bounds (ms). For positive doubles
   the unsigned bit pattern is monotonic in value, so we clamp by integer compare.
   To avoid bringing in FPU operations or soft-float emulation on the Cortex-M0+
   target, the double bits are converted to milliseconds using integer-only shifts
   and bitmasks. */
#define MU_SESSION_TIMEOUT_MIN_BITS 0x40c3880000000000ULL /* 10000.0   */
#define MU_SESSION_TIMEOUT_MAX_BITS 0x414b774000000000ULL /* 3600000.0 */
#define MU_DOUBLE_SIGN_BIT          0x8000000000000000ULL

static opcua_uint64_t clamp_timeout_bits(opcua_uint64_t bits) {
    /* Sign set (negative / -0 / -NaN) or below the minimum -> minimum; above the
       maximum (incl. +Inf / +NaN) -> maximum. */
    if (bits & MU_DOUBLE_SIGN_BIT) return MU_SESSION_TIMEOUT_MIN_BITS;
    if (bits < MU_SESSION_TIMEOUT_MIN_BITS) return MU_SESSION_TIMEOUT_MIN_BITS;
    if (bits > MU_SESSION_TIMEOUT_MAX_BITS) return MU_SESSION_TIMEOUT_MAX_BITS;
    return bits;
}

void mu_session_init(mu_session_t *session) {
    if (session) {
        session->state = MU_SESSION_STATE_CLOSED;
        session->session_id = 0;
        session->auth_token = 0;
        session->revised_session_timeout_ms = 0;
        memset(session->server_nonce, 0, sizeof(session->server_nonce));
    }
}

mu_session_t *mu_session_find_by_token(mu_session_t *sessions,
                                       size_t count,
                                       opcua_uint32_t auth_token)
{
    if (sessions == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        if (sessions[i].state != MU_SESSION_STATE_CLOSED &&
            sessions[i].auth_token == auth_token) {
            return &sessions[i];
        }
    }

    return NULL;
}

mu_session_t *mu_session_find_free(mu_session_t *sessions,
                                   size_t count)
{
    if (sessions == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        if (sessions[i].state == MU_SESSION_STATE_CLOSED) {
            return &sessions[i];
        }
    }

    return NULL;
}

opcua_statuscode_t mu_session_create(mu_session_t *session,
                                     opcua_uint64_t requested_timeout_bits,
                                     opcua_uint64_t *revised_timeout_bits,
                                     opcua_uint32_t *session_id,
                                     opcua_uint32_t *auth_token)
{
    if (!session || !revised_timeout_bits || !session_id || !auth_token) return MU_STATUS_BAD_INTERNALERROR;

    if (session->state != MU_SESSION_STATE_CLOSED) {
        return MU_STATUS_BAD_INTERNALERROR; /* Only 1 session in minimal server */
    }

    session->session_id = 1;
    session->auth_token = 12345; /* Mock token */

    opcua_uint64_t revised = clamp_timeout_bits(requested_timeout_bits);
    double d;
    memcpy(&d, &revised, sizeof(d));
    session->revised_session_timeout_ms = (opcua_uint32_t)d;
    session->state = MU_SESSION_STATE_CREATED;

    *revised_timeout_bits = revised;
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
    
    /* We support anonymous (321), username (324), and certificate (327) identity tokens */
    if (identity_token_encoding_id != 321 && identity_token_encoding_id != 324 &&
        identity_token_encoding_id != 327) {
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
