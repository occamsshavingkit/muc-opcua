/* include/muc_opcua/services/diagnostics.h
 *
 * Server Diagnostics API. OPC-10000-5 §6.3.
 * All counters live in caller-provided storage (mu_server_t.diag).
 */
#ifndef MUC_OPCUA_SERVICES_DIAGNOSTICS_H
#define MUC_OPCUA_SERVICES_DIAGNOSTICS_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/types.h"
#include <stdbool.h>

/* ServerStatusDataType source (OPC-10000-5 §12.10). CurrentTime is filled live
   by the read callback; identity/build fields are set by the caller at init. */
typedef struct {
    opcua_datetime_t start_time;
    opcua_datetime_t current_time; /* filled by the read callback via the time adapter */
    opcua_int32_t state;           /* ServerState enum, 0 = Running */
    mu_string_t product_uri;
    mu_string_t manufacturer_name;
    mu_string_t product_name;
    mu_string_t software_version;
    mu_string_t build_number;
    opcua_datetime_t build_date;
    const mu_time_adapter_t *time; /* CurrentTime source; used only by the read callback */
} mu_server_status_t;

#if MUC_OPCUA_SERVER_DIAGNOSTICS

/* Field order and names mirror ServerDiagnosticsSummaryDataType (OPC-10000-5 §12.9,
   DataType i=859), 12 x UInt32, so the ExtensionObject encoder writes the struct in
   declared order. Counters live in caller-owned storage (mu_server_t.diag); zero BSS. */
typedef struct {
    opcua_uint32_t server_view_count;
    opcua_uint32_t current_session_count;
    opcua_uint32_t cumulated_session_count;
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
void mu_diagnostics_session_security_rejected(struct mu_server *server);
void mu_diagnostics_subscription_created(struct mu_server *server);
void mu_diagnostics_subscription_closed(struct mu_server *server);
/* A request rejected before/at dispatch (security==true also bumps the security-rejected
   counter). OPC-10000-5 §12.9 rejectedRequestsCount / securityRejectedRequestsCount. */
void mu_diagnostics_request_rejected(struct mu_server *server, bool security);

#else /* !MUC_OPCUA_SERVER_DIAGNOSTICS */

/* No-op stubs so shared dispatch/session/subscription code can call the diagnostics hooks
   unconditionally; profiles without the facet compile them away (byte-identical). */
struct mu_server;
static inline void mu_diagnostics_session_created(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_session_closed(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_session_timeout(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_session_rejected(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_session_security_rejected(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_subscription_created(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_subscription_closed(struct mu_server *server) {
    (void)server;
}
static inline void mu_diagnostics_request_rejected(struct mu_server *server, bool security) {
    (void)server;
    (void)security;
}

#endif

#endif
