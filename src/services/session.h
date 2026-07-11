/* src/services/session.h */
#ifndef MUC_OPCUA_SERVICES_SESSION_H
#define MUC_OPCUA_SERVICES_SESSION_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum { MU_SESSION_STATE_CLOSED = 0, MU_SESSION_STATE_CREATED, MU_SESSION_STATE_ACTIVATED } mu_session_state_t;

typedef struct {
    mu_session_state_t state;
    opcua_uint32_t session_id;
    opcua_uint32_t auth_token;
    /* RevisedSessionTimeout as the raw IEEE-754 bits of the Duration (ms). Kept as
       bits so the value is clamped and echoed with no floating-point math (the
       Cortex-M0+ target has no FPU). */
    opcua_uint32_t revised_session_timeout_ms;
    opcua_uint64_t last_activity_ms;
    opcua_byte_t server_nonce[32];
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    opcua_uint32_t secure_channel_id;
#endif
#if MUC_OPCUA_REDUNDANCY
    /* User-identity fingerprint captured at ActivateSession, for the same-user
       (ClientValidated) check in TransferSubscriptions (OPC-10000-4 §5.14.1.4 /
       §6.6.3). kind = the identity-token type (321 anonymous, 324 username, 327
       x509); the bounded discriminator is the username bytes / cert prefix. */
    opcua_byte_t user_identity_kind;
    opcua_byte_t user_identity_len;
    opcua_byte_t user_identity[64];
#endif
} mu_session_t;

#if MUC_OPCUA_REDUNDANCY
/* TRUE only when both Sessions were activated on behalf of the same user (same
   identity kind and discriminator). Anonymous == anonymous. */
bool mu_session_same_user(const mu_session_t *a, const mu_session_t *b);
#endif

void mu_session_init(mu_session_t *session);

mu_session_t *mu_session_find_by_token(mu_session_t *sessions, size_t count, opcua_uint32_t auth_token);

/* OPC-10000-4 section 5.7.4.1: find a CLOSED session by its (preserved)
   authenticationToken. Used by the request dispatch path to distinguish a
   request on a closed Session (Bad_SessionClosed) from a request with an
   unknown token (Bad_SessionIdInvalid). */
mu_session_t *mu_session_find_closed_by_token(mu_session_t *sessions, size_t count, opcua_uint32_t auth_token);

mu_session_t *mu_session_find_free(mu_session_t *sessions, size_t count);

/* Create the (single) session. The requested/revised SessionTimeout are passed as
   the raw IEEE-754 bits of the wire Duration; the value is clamped to
   [10000, 3600000] ms via integer bit comparison and stored internally as milliseconds
   without floating-point math (valid for positive doubles). */
opcua_statuscode_t mu_session_create(mu_session_t *session, opcua_uint64_t requested_timeout_bits,
                                     opcua_uint64_t *revised_timeout_bits, opcua_uint32_t *session_id,
                                     opcua_uint32_t *auth_token);

opcua_statuscode_t mu_session_generate_session_id(const mu_session_t *sessions, size_t count,
                                                  const mu_entropy_adapter_t *entropy, opcua_uint32_t *session_id);

opcua_statuscode_t mu_session_generate_authentication_token(const mu_session_t *sessions, size_t count,
                                                            const mu_entropy_adapter_t *entropy,
                                                            opcua_uint32_t *auth_token);

opcua_statuscode_t mu_session_generate_server_nonce(const mu_entropy_adapter_t *entropy, opcua_byte_t *server_nonce,
                                                    size_t server_nonce_len);

opcua_statuscode_t mu_session_create_with_identifiers(mu_session_t *session, opcua_uint64_t requested_timeout_bits,
                                                      opcua_uint32_t assigned_session_id,
                                                      opcua_uint32_t assigned_auth_token,
                                                      opcua_uint32_t creating_secure_channel_id,
                                                      opcua_uint64_t *revised_timeout_bits, opcua_uint32_t *session_id,
                                                      opcua_uint32_t *auth_token);

opcua_statuscode_t mu_session_validate_secure_channel(const mu_session_t *session,
                                                      opcua_uint32_t active_secure_channel_id);

opcua_statuscode_t mu_session_activate(mu_session_t *session, opcua_uint32_t auth_token,
                                       opcua_uint32_t identity_token_encoding_id);

opcua_statuscode_t mu_session_close(mu_session_t *session, opcua_uint32_t auth_token, bool delete_subscriptions);

/* OPC-10000-4 section 5.7.2.1: close a Session due to SessionTimeout expiry.
   Unlike mu_session_close, no authenticationToken is required — the Server
   itself is closing the idle Session. As with CloseSession, the state is set to
   CLOSED and the serverNonce is zeroed, while the session_id and auth_token are
   preserved so subsequent requests on the stale token return Bad_SessionClosed.
   When MUC_OPCUA_SESSION_TIMEOUT is off, session timeout is not
   tracked; the inline no-op satisfies callers that are compiled out. */
#if defined(MUC_OPCUA_SESSION_TIMEOUT)
void mu_session_close_timeout(mu_session_t *session);
#else
static inline void mu_session_close_timeout(mu_session_t *session) {
    (void)session;
}
#endif

#endif /* MUC_OPCUA_SERVICES_SESSION_H */
