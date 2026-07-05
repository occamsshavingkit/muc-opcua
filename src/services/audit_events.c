/* src/services/audit_events.c
 *
 * Audit event stub — full implementation pending dispatch integration.
 * OPC-10000-5 §6.5 (Audit Event Types)
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_AUDITING

#include "muc_opcua/services/audit.h"
#include "muc_opcua/types.h"

struct mu_server;

void mu_raise_audit_event(struct mu_server *server, const mu_audit_event_t *event) {
    (void)server;
    (void)event;
}

#endif /* MUC_OPCUA_AUDITING */
