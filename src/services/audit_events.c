/* src/services/audit_events.c
 *
 * Auditing Server Facet: audit event dispatch with callback mechanism.
 * OPC-10000-5 §6.5 (Audit Event Types).
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_AUDITING

#include "muc_opcua/services/audit.h"
#include "muc_opcua/types.h"
#include "muc_opcua/server.h"

#include "../core/server_internal.h"

void mu_server_set_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context) {
    if (server == NULL) return;
    server->audit_callback_count = 0;
    if (callback != NULL) {
        server->audit_callbacks[0].callback = callback;
        server->audit_callbacks[0].context = context;
        server->audit_callback_count = 1;
    }
}

opcua_statuscode_t mu_server_add_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context) {
    if (server == NULL || callback == NULL) return MU_STATUS_BAD_ARGUMENTSMISSING;
    if (server->audit_callback_count >= MU_MAX_AUDIT_CALLBACKS) return MU_STATUS_BAD_OUTOFMEMORY;
    size_t idx = server->audit_callback_count++;
    server->audit_callbacks[idx].callback = callback;
    server->audit_callbacks[idx].context = context;
    return MU_STATUS_GOOD;
}

void mu_raise_audit_event(mu_server_t *server, const mu_audit_event_t *event) {
    if (server == NULL || event == NULL) return;
    if (!server->config.auditing_enabled) return;

    mu_audit_event_t mutable_event = *event;
    mutable_event.action_timestamp = 0; /* placeholder — platform time adapter needed for real timestamp */

    size_t i;
    for (i = 0; i < server->audit_callback_count; i++) {
        server->audit_callbacks[i].callback(server, &mutable_event, server->audit_callbacks[i].context);
    }
}

#endif /* MUC_OPCUA_AUDITING */
