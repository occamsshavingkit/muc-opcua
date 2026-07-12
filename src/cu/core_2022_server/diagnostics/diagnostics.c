/* src/services/diagnostics.c
 *
 * Server Diagnostics: counter management and session diagnostics.
 * OPC-10000-5 §6.3.3 (ServerDiagnosticsSummary), §6.3.5 (SessionDiagnostics).
 *
 * Counters live in caller-provided storage (mu_server_t.diag) —
 * zero BSS per Constitution VII.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_DIAGNOSTICS

#include "core/server_internal.h"
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

#endif /* MUC_OPCUA_CU_DIAGNOSTICS */
