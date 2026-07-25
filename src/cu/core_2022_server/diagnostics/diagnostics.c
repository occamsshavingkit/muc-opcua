/* src/services/diagnostics.c
 *
 * Server Diagnostics: counter management and session diagnostics.
 * OPC-10000-5 §6.3.3 (ServerDiagnosticsSummary), §6.3.5 (SessionDiagnostics).
 *
 * Counters live in caller-provided storage (mu_server_t.diag) —
 * zero BSS per Constitution VII.
 */
#include "muc_opcua/config.h" // IWYU pragma: keep

#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS

#include "core/server_internal.h" // IWYU pragma: keep
#include "cu/core_2022_server/diagnostics/diagnostics.h"
#include "muc_opcua/server.h"

void mu_diagnostics_session_created(mu_server_t *server) {
    if (server) {
        server->diag.cumulated_session_count++;
        server->diag.current_session_count++;
    }
}

void mu_diagnostics_session_closed(mu_server_t *server) {
    if (server && server->diag.current_session_count > 0)
        server->diag.current_session_count--;
}

void mu_diagnostics_session_timeout(mu_server_t *server) {
    if (server)
        server->diag.session_timeout_count++;
}

void mu_diagnostics_session_rejected(mu_server_t *server) {
    if (server)
        server->diag.rejected_session_count++;
}

void mu_diagnostics_session_security_rejected(mu_server_t *server) {
    if (server) {
        server->diag.security_rejected_session_count++;
        server->diag.rejected_session_count++;
    }
}

void mu_diagnostics_request_rejected(mu_server_t *server, bool security) {
    if (server) {
        server->diag.rejected_requests_count++;
        if (security)
            server->diag.security_rejected_requests_count++;
    }
}

void mu_diagnostics_subscription_created(mu_server_t *server) {
    if (server) {
        server->diag.cumulated_subscription_count++;
        server->diag.current_subscription_count++;
    }
}

void mu_diagnostics_subscription_closed(mu_server_t *server) {
    if (server && server->diag.current_subscription_count > 0)
        server->diag.current_subscription_count--;
}

#if MUC_OPCUA_CU_USER_TOKEN_JWT
/* T048 (spec 093): expose JWT user identity and mapped roles from an active
   session for the diagnostics read path (OPC-10000-5 §6.3.5). Returns the
   kind byte (4 = JWT), the NUL-terminated sub string, and the bounded role
   array. All pointers may be NULL (selective query). */
void mu_diagnostics_session_identity(const mu_session_t *session, opcua_byte_t *out_kind,
                                     const opcua_byte_t **out_identity, opcua_byte_t *out_identity_len) {
    if (session == NULL)
        return;
    if (out_kind != NULL)
        *out_kind = session->user_identity_kind;
    if (out_identity != NULL)
        *out_identity = session->user_identity_kind == 4u && session->user_identity_len > 0 ? session->user_identity
                                                                                             : NULL;
    if (out_identity_len != NULL)
        *out_identity_len = session->user_identity_kind == 4u ? session->user_identity_len : 0u;
}

opcua_byte_t mu_diagnostics_session_role_count(const mu_session_t *session) {
    if (session == NULL)
        return 0;
#if MUC_OPCUA_CU_REDUNDANCY || MUC_OPCUA_CU_USER_TOKEN_JWT
    return session->session_role_count;
#else
    return 0;
#endif
}

bool mu_diagnostics_session_role_id(const mu_session_t *session, opcua_byte_t index,
                                    opcua_uint32_t *out_role_id) {
    if (session == NULL || out_role_id == NULL || index >= session->session_role_count)
        return false;
    *out_role_id = session->session_roles[index];
    return true;
}

#endif /* MUC_OPCUA_CU_USER_TOKEN_JWT */

#endif /* MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS */
