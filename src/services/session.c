/* src/services/session.c */
#include "session.h"
#include "../core/server_internal.h"
#include <stddef.h>
#include <string.h>

/* IEEE-754 bit patterns of the SessionTimeout bounds (ms). For positive doubles
   the unsigned bit pattern is monotonic in value, so we clamp by integer compare.
   To avoid bringing in FPU operations or soft-float emulation on the Cortex-M0+
   target, the double bits are converted to milliseconds using integer-only shifts
   and bitmasks. */
#define MU_SESSION_TIMEOUT_MIN_BITS 0x40c3880000000000ULL /* 10000.0   */
#define MU_SESSION_TIMEOUT_MAX_BITS 0x414b774000000000ULL /* 3600000.0 */
#define MU_DOUBLE_SIGN_BIT 0x8000000000000000ULL

static opcua_uint64_t clamp_timeout_bits(opcua_uint64_t bits) {
    /* Sign set (negative / -0 / -NaN) or below the minimum -> minimum; above the
       maximum (incl. +Inf / +NaN) -> maximum. */
    if (bits & MU_DOUBLE_SIGN_BIT) {
        return MU_SESSION_TIMEOUT_MIN_BITS;
    }
    if (bits < MU_SESSION_TIMEOUT_MIN_BITS) {
        return MU_SESSION_TIMEOUT_MIN_BITS;
    }
    if (bits > MU_SESSION_TIMEOUT_MAX_BITS) {
        return MU_SESSION_TIMEOUT_MAX_BITS;
    }
    return bits;
}

static opcua_uint32_t duration_bits_to_ms(opcua_uint64_t bits) {
    opcua_uint32_t exponent = (opcua_uint32_t)((bits >> 52) & 0x7FFu);
    opcua_uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;
    opcua_uint64_t mantissa = (1ULL << 52) | fraction;
    opcua_uint32_t unbiased = exponent - 1023u;

    if (unbiased >= 52u) {
        return (opcua_uint32_t)(mantissa << (unbiased - 52u));
    }
    return (opcua_uint32_t)(mantissa >> (52u - unbiased));
}

void mu_session_init(mu_session_t *session) {
    if (session) {
        session->state = MU_SESSION_STATE_CLOSED;
        session->session_id = 0;
        session->auth_token = 0;
        session->revised_session_timeout_ms = 0;
        session->last_activity_ms = 0;
        (void)memset(session->server_nonce, 0, sizeof(session->server_nonce));
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        session->secure_channel_id = 0;
#endif
#if MUC_OPCUA_REDUNDANCY
        session->user_identity_kind = 0;
        session->user_identity_len = 0;
        (void)memset(session->user_identity, 0, sizeof(session->user_identity));
#endif
    }
}

#if MUC_OPCUA_REDUNDANCY
bool mu_session_same_user(const mu_session_t *a, const mu_session_t *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (a->user_identity_kind != b->user_identity_kind || a->user_identity_len != b->user_identity_len) {
        return false;
    }
    return memcmp(a->user_identity, b->user_identity, a->user_identity_len) == 0;
}
#endif

mu_session_t *mu_session_find_by_token(mu_session_t *sessions, size_t count, opcua_uint32_t auth_token) {
    if (sessions == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        if (sessions[i].state != MU_SESSION_STATE_CLOSED && sessions[i].auth_token == auth_token) {
            return &sessions[i];
        }
    }

    return NULL;
}

mu_session_t *mu_session_find_free(mu_session_t *sessions, size_t count) {
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

static opcua_boolean_t session_id_active(const mu_session_t *sessions, size_t count, opcua_uint32_t session_id) {
    if (sessions == NULL) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (sessions[i].state != MU_SESSION_STATE_CLOSED && sessions[i].session_id == session_id) {
            return true;
        }
    }

    return false;
}

static opcua_boolean_t authentication_token_active(const mu_session_t *sessions, size_t count,
                                                   opcua_uint32_t auth_token) {
    if (sessions == NULL) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (sessions[i].state != MU_SESSION_STATE_CLOSED && sessions[i].auth_token == auth_token) {
            return true;
        }
    }

    return false;
}

opcua_statuscode_t mu_session_generate_session_id(const mu_session_t *sessions, size_t count,
                                                  const mu_entropy_adapter_t *entropy, opcua_uint32_t *session_id) {
    if (entropy == NULL || entropy->generate_random == NULL || session_id == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_byte_t random[sizeof(opcua_uint32_t)];
    opcua_statuscode_t status = entropy->generate_random(entropy->context, random, sizeof(random));
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_uint32_t base = ((opcua_uint32_t)random[0]) | ((opcua_uint32_t)random[1] << 8) |
                          ((opcua_uint32_t)random[2] << 16) | ((opcua_uint32_t)random[3] << 24);

    for (size_t salt = 0; salt <= count; ++salt) {
        /* OPC-10000-4 section 5.7.2.2: CreateSession SessionId is generated
           from entropy; the salt only resolves active-slot collisions.
           Session ID is generated via XOR with 0x9E3779B9u; bounded by
           MU_INTERN_MAX_SESSIONS (default 2). Not cryptographically random —
           acceptable for single-connection profile. */
        opcua_uint32_t candidate = base ^ (opcua_uint32_t)(0x9E3779B9u * (opcua_uint32_t)salt);
        if (candidate == 0u) {
            candidate = (opcua_uint32_t)(0x85EBCA6Bu ^ (opcua_uint32_t)salt);
        }
        if (!session_id_active(sessions, count, candidate)) {
            *session_id = candidate;
            return MU_STATUS_GOOD;
        }
    }

    return MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

opcua_statuscode_t mu_session_generate_authentication_token(const mu_session_t *sessions, size_t count,
                                                            const mu_entropy_adapter_t *entropy,
                                                            opcua_uint32_t *auth_token) {
    if (entropy == NULL || entropy->generate_random == NULL || auth_token == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_byte_t random[sizeof(opcua_uint32_t)];
    opcua_statuscode_t status = entropy->generate_random(entropy->context, random, sizeof(random));
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_uint32_t base = ((opcua_uint32_t)random[0]) | ((opcua_uint32_t)random[1] << 8) |
                          ((opcua_uint32_t)random[2] << 16) | ((opcua_uint32_t)random[3] << 24);

    for (size_t salt = 0; salt <= count; ++salt) {
        /* OPC-10000-4 section 5.7.2.2: CreateSession authenticationToken is
           generated from entropy; the salt only resolves active-slot collisions. */
        opcua_uint32_t candidate = (base ^ 0xA5A5A5A5u) + (opcua_uint32_t)(0x9E3779B9u * (opcua_uint32_t)salt);
        if (candidate == 0u) {
            candidate = (opcua_uint32_t)(0xC2B2AE35u ^ (opcua_uint32_t)salt);
        }
        if (!authentication_token_active(sessions, count, candidate)) {
            *auth_token = candidate;
            return MU_STATUS_GOOD;
        }
    }

    return MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

opcua_statuscode_t mu_session_generate_server_nonce(const mu_entropy_adapter_t *entropy, opcua_byte_t *server_nonce,
                                                    size_t server_nonce_len) {
    if (entropy == NULL || entropy->generate_random == NULL || server_nonce == NULL || server_nonce_len == 0u) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    if (entropy->generate_random(entropy->context, server_nonce, server_nonce_len) != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_session_create_with_identifiers(mu_session_t *session, opcua_uint64_t requested_timeout_bits,
                                                      opcua_uint32_t assigned_session_id,
                                                      opcua_uint32_t assigned_auth_token,
                                                      opcua_uint32_t creating_secure_channel_id,
                                                      opcua_uint64_t *revised_timeout_bits, opcua_uint32_t *session_id,
                                                      opcua_uint32_t *auth_token) {
    if (!session || !revised_timeout_bits || !session_id || !auth_token) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (session->state != MU_SESSION_STATE_CLOSED) {
        return MU_STATUS_BAD_INTERNALERROR; /* Only 1 session in minimal server */
    }

    session->session_id = assigned_session_id;
    session->auth_token = assigned_auth_token;
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    /* OPC-10000-4 section 5.7.2.1 binds a Session to the SecureChannel that
       created it; later service checks validate use through that channel. */
    session->secure_channel_id = creating_secure_channel_id;
#else
    (void)creating_secure_channel_id;
#endif

    opcua_uint64_t revised = clamp_timeout_bits(requested_timeout_bits);
    session->revised_session_timeout_ms = duration_bits_to_ms(revised);
    session->state = MU_SESSION_STATE_CREATED;

    *revised_timeout_bits = revised;
    *session_id = session->session_id;
    *auth_token = session->auth_token;

    return MU_STATUS_GOOD;
}
opcua_statuscode_t mu_session_create(mu_session_t *session, opcua_uint64_t requested_timeout_bits,
                                     opcua_uint64_t *revised_timeout_bits, opcua_uint32_t *session_id,
                                     opcua_uint32_t *auth_token) {
    return mu_session_create_with_identifiers(session, requested_timeout_bits, 1u, 12345u, 0u, revised_timeout_bits,
                                              session_id, auth_token);
}

opcua_statuscode_t mu_session_validate_secure_channel(const mu_session_t *session,
                                                      opcua_uint32_t active_secure_channel_id) {
    if (!session) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    /* OPC-10000-4 section 7.38.2: reject Session use through a SecureChannel
       different from the one that created the Session. */
    if (session->secure_channel_id != active_secure_channel_id) {
        return MU_STATUS_BAD_SECURECHANNELIDINVALID;
    }
#else
    (void)active_secure_channel_id;
#endif

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_session_activate(mu_session_t *session, opcua_uint32_t auth_token,
                                       opcua_uint32_t identity_token_encoding_id) {
    if (!session) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (session->state != MU_SESSION_STATE_CREATED && session->state != MU_SESSION_STATE_ACTIVATED) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }

    if (session->auth_token != auth_token) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }

    /* We support anonymous (321), username (324), and certificate (327) identity tokens */
    if (identity_token_encoding_id != 321 && identity_token_encoding_id != 324 && identity_token_encoding_id != 327) {
        return MU_STATUS_BAD_IDENTITYTOKENINVALID;
    }

    session->state = MU_SESSION_STATE_ACTIVATED;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_session_close(mu_session_t *session, opcua_uint32_t auth_token, bool delete_subscriptions) {
    if (!session) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    (void)delete_subscriptions; /* Not supported yet */

    if (session->state == MU_SESSION_STATE_CLOSED) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }

    if (session->auth_token != auth_token) {
        return MU_STATUS_BAD_SESSIONIDINVALID;
    }

    /* OPC-10000-4 section 5.7.4.1: mark the Session CLOSED but preserve the
       authenticationToken and SessionId so that subsequent requests carrying
       this token can be rejected with Bad_SessionClosed (rather than
       Bad_SessionIdInvalid, which is reserved for a token the server has never
       issued). The slot remains reusable: mu_session_find_free returns CLOSED
       slots and mu_session_create_with_identifiers overwrites the token on
       reuse. The server nonce is cleared as a security hygiene measure. */
    session->state = MU_SESSION_STATE_CLOSED;
    (void)memset(session->server_nonce, 0, sizeof(session->server_nonce));
    return MU_STATUS_GOOD;
}

mu_session_t *mu_session_find_closed_by_token(mu_session_t *sessions, size_t count, opcua_uint32_t auth_token) {
    if (sessions == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        if (sessions[i].state == MU_SESSION_STATE_CLOSED && sessions[i].auth_token == auth_token) {
            return &sessions[i];
        }
    }

    return NULL;
}

#ifdef MUC_OPCUA_SESSION_TIMEOUT
void mu_session_close_timeout(mu_session_t *session) {
    if (session == NULL) {
        return;
    }

    /* OPC-10000-4 section 5.7.2.1: a Session whose last_activity_ms predates
       now - revised_session_timeout_ms is closed by the Server. The state
       transition and nonce-clear mirror mu_session_close; the auth_token and
       session_id are preserved so the dispatch path can distinguish a timed-out
       Session (Bad_SessionClosed) from an unknown token (Bad_SessionIdInvalid). */
    if (session->state == MU_SESSION_STATE_CLOSED) {
        return;
    }

    session->state = MU_SESSION_STATE_CLOSED;
    (void)memset(session->server_nonce, 0, sizeof(session->server_nonce));
}
#endif /* MUC_OPCUA_SESSION_TIMEOUT */
