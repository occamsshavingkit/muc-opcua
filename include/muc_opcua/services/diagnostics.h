/* include/muc_opcua/services/diagnostics.h
 *
 * Server Diagnostics API. OPC-10000-5 §6.3.
 * All counters live in caller-provided storage (mu_server_t.diag).
 */
#ifndef MUC_OPCUA_SERVICES_DIAGNOSTICS_H
#define MUC_OPCUA_SERVICES_DIAGNOSTICS_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"

#if MUC_OPCUA_SERVER_DIAGNOSTICS

typedef struct {
    opcua_uint32_t cumulated_session_count;
    opcua_uint32_t current_session_count;
    opcua_uint32_t security_rejected_session_count;
    opcua_uint32_t rejected_session_count;
    opcua_uint32_t session_timeout_count;
    opcua_uint32_t session_abort_count;
    opcua_uint32_t current_subscription_count;
    opcua_uint32_t cumulated_subscription_count;
    opcua_uint32_t publishing_interval_count;
    opcua_uint32_t security_rejected_requests_count;
    opcua_uint32_t rejected_requests_count;
} mu_diagnostics_summary_t;

struct mu_server;
void mu_diagnostics_session_created(struct mu_server *server);
void mu_diagnostics_session_closed(struct mu_server *server);
void mu_diagnostics_session_timeout(struct mu_server *server);
void mu_diagnostics_session_rejected(struct mu_server *server);
void mu_diagnostics_subscription_created(struct mu_server *server);
void mu_diagnostics_subscription_closed(struct mu_server *server);

#endif

#endif
