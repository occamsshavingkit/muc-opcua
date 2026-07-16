/* src/services/audit_events.c
 *
 * Auditing Server Facet: audit event dispatch with callback mechanism.
 * OPC-10000-5 §6.5 (Audit Event Types).
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_AUDITING

#include "muc_opcua/server.h"
#include "muc_opcua/services/audit.h"
#include "muc_opcua/types.h"

#include "core/server_internal.h"

#include <string.h>

#if defined(MUC_OPCUA_EVENTS)
/* Bounded copy of a request-lifetime string into the audit pool (source buffers
   are gone by Publish time). Truncates at MU_AUDIT_STR_MAX; len -1 = Null. */
static void audit_str_copy(mu_audit_str_t *dst, const mu_string_t *src) {
    if (src == NULL || src->length < 0 || src->data == NULL) {
        dst->len = -1;
        return;
    }
    opcua_int32_t n = src->length;
    if (n > MU_AUDIT_STR_MAX) {
        n = MU_AUDIT_STR_MAX;
    }
    if (n > 0) {
        (void)memcpy(dst->data, src->data, (size_t)n);
    }
    dst->len = n;
}

/* Only variant types whose value lives entirely in the union (no external
   pointer) survive being queued; strings/bytestrings/nodeids point at caller
   memory and are dropped to Null (documented, spec 074 Decision 7). */
static opcua_boolean_t audit_scalar_inline(const mu_variant_t *v) {
    if (v->is_array) {
        return false;
    }
    switch (v->type) {
    case MU_TYPE_BOOLEAN:
    case MU_TYPE_SBYTE:
    case MU_TYPE_BYTE:
    case MU_TYPE_INT16:
    case MU_TYPE_UINT16:
    case MU_TYPE_INT32:
    case MU_TYPE_UINT32:
    case MU_TYPE_INT64:
    case MU_TYPE_UINT64:
    case MU_TYPE_FLOAT:
    case MU_TYPE_DOUBLE:
    case MU_TYPE_DATETIME:
    case MU_TYPE_STATUSCODE:
        return true;
    default:
        return false;
    }
}

static void audit_capture_scalar(mu_variant_t *dst, const mu_variant_t *src) {
    if (audit_scalar_inline(src)) {
        *dst = *src;
    } else {
        (void)memset(dst, 0, sizeof(*dst));
        dst->type = MU_TYPE_NULL;
    }
}

static mu_nodeid_t audit_event_type_nodeid(opcua_uint32_t event_type) {
    opcua_uint32_t numeric;
    switch (event_type) {
    case MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL:
        numeric = 2060u; /* AuditOpenSecureChannelEventType (OPC-10000-5 §6.4.6) */
        break;
    case MU_AUDIT_EVENT_CREATE_SESSION:
        numeric = 2071u; /* AuditCreateSessionEventType §6.4.8 */
        break;
    case MU_AUDIT_EVENT_ACTIVATE_SESSION:
        numeric = 2075u; /* AuditActivateSessionEventType §6.4.10 */
        break;
    case MU_AUDIT_EVENT_WRITE_UPDATE:
        numeric = 2100u; /* AuditWriteUpdateEventType §6.4.25 */
        break;
    default:
        numeric = 2052u; /* AuditEventType base §6.4.3 */
        break;
    }
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {.numeric = numeric}};
    return id;
}

mu_audit_ref_t mu_audit_pool_store(mu_server_t *server, const mu_audit_payload_t *payload) {
    mu_audit_ref_t ref = {false, 0u, 0u};
    if (server == NULL || payload == NULL) {
        return ref;
    }
    opcua_byte_t idx = server->audit_pool_next;
    opcua_uint32_t seq = server->audit_pool_sequence + 1u;
    if (seq == 0u) {
        seq = 1u; /* 0 means empty slot */
    }
    server->audit_pool_sequence = seq;
    server->audit_pool[idx] = *payload;
    server->audit_pool[idx].sequence = seq;
    server->audit_pool_next = (opcua_byte_t)((idx + 1u) % MU_MAX_AUDIT_PAYLOADS);
    ref.valid = true;
    ref.index = idx;
    ref.sequence = seq;
    return ref;
}
#endif /* MUC_OPCUA_EVENTS */

void mu_server_set_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context) {
    if (server == NULL)
        return;
    server->audit_callback_count = 0;
    if (callback != NULL) {
        server->audit_callbacks[0].callback = callback;
        server->audit_callbacks[0].context = context;
        server->audit_callback_count = 1;
    }
}

opcua_statuscode_t mu_server_add_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context) {
    if (server == NULL || callback == NULL)
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    if (server->audit_callback_count >= MU_MAX_AUDIT_CALLBACKS)
        return MU_STATUS_BAD_OUTOFMEMORY;
    size_t idx = server->audit_callback_count++;
    server->audit_callbacks[idx].callback = callback;
    server->audit_callbacks[idx].context = context;
    return MU_STATUS_GOOD;
}

void mu_raise_audit_event(mu_server_t *server, const mu_audit_event_t *event) {
    if (server == NULL || event == NULL)
        return;
    if (!server->config.auditing_enabled)
        return;

    mu_audit_event_t mutable_event = *event;
    if (server->config.time_adapter.get_time != NULL) {
        mutable_event.action_timestamp = server->config.time_adapter.get_time(server->config.time_adapter.context);
    }

    size_t i;
    for (i = 0; i < server->audit_callback_count; i++) {
        server->audit_callbacks[i].callback(server, &mutable_event, server->audit_callbacks[i].context);
    }

#if defined(MUC_OPCUA_EVENTS)
    /* Route the AuditEvent into the OPC UA event pipeline so a client subscribed
       to the Server Object's EventNotifier observes it (spec 074). The full audit
       fields live in the shared pool; the queued event carries only a ref. */
    mu_audit_payload_t payload;
    (void)memset(&payload, 0, sizeof(payload));
    payload.source_node = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 2253u}}; /* Server Object */
    payload.status = mutable_event.status;
    payload.action_timestamp = mutable_event.action_timestamp;
    audit_str_copy(&payload.server_id, &mutable_event.server_id);
    audit_str_copy(&payload.client_user_id, &mutable_event.client_user_id);
    audit_str_copy(&payload.client_audit_entry_id, &mutable_event.client_audit_entry_id);
    payload.secure_channel_id.len = -1;
    payload.old_value.type = MU_TYPE_NULL;
    payload.new_value.type = MU_TYPE_NULL;

    switch (mutable_event.event_type) {
    case MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL:
        audit_str_copy(&payload.secure_channel_id, &mutable_event.specific.open_channel.secure_channel_id);
        break;
    case MU_AUDIT_EVENT_CREATE_SESSION:
        payload.session_id = mutable_event.specific.create_session.session_id;
        break;
    case MU_AUDIT_EVENT_ACTIVATE_SESSION:
        payload.session_id = mutable_event.specific.activate_session.session_id;
        audit_str_copy(&payload.client_user_id, &mutable_event.specific.activate_session.user_name);
        break;
    case MU_AUDIT_EVENT_WRITE_UPDATE:
        payload.attribute_id = 13u; /* Value attribute (OPC-10000-6) */
        audit_capture_scalar(&payload.old_value, &mutable_event.specific.write_update.old_value);
        audit_capture_scalar(&payload.new_value, &mutable_event.specific.write_update.new_value);
        break;
    default:
        break;
    }

    mu_event_notification_t notif;
    (void)memset(&notif, 0, sizeof(notif));
    notif.event_type = audit_event_type_nodeid(mutable_event.event_type);
    notif.time = mutable_event.action_timestamp;
    notif.severity = 1u;
    notif.message = (mu_string_t){-1, NULL};
    notif.audit_ref = mu_audit_pool_store(server, &payload);
    (void)mu_server_trigger_event(server, &notif);
#endif /* MUC_OPCUA_EVENTS */
}

#endif /* MUC_OPCUA_CU_AUDITING */
