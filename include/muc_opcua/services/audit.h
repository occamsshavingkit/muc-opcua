/* include/muc_opcua/services/audit.h
 *
 * Auditing Server Facet: audit event type definitions.
 * OPC-10000-5 §6.5 (Audit Event Types).
 */
#ifndef MUC_OPCUA_SERVICES_AUDIT_H
#define MUC_OPCUA_SERVICES_AUDIT_H

#include "muc_opcua/config.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MUC_OPCUA_AUDITING

struct mu_server;

#define MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL 0
#define MU_AUDIT_EVENT_CREATE_SESSION 1
#define MU_AUDIT_EVENT_ACTIVATE_SESSION 2
#define MU_AUDIT_EVENT_WRITE_UPDATE 3
#define MU_AUDIT_EVENT_NODE_MANAGEMENT 4
#define MU_AUDIT_EVENT_METHOD 5
#define MU_AUDIT_EVENT_CONDITION_ENABLE 6
#define MU_AUDIT_EVENT_CONDITION_ACKNOWLEDGE 7
#define MU_AUDIT_EVENT_CONDITION_CONFIRM 8
#define MU_AUDIT_EVENT_CONDITION_RESPOND 9

typedef struct {
    opcua_uint32_t event_type;
    opcua_datetime_t action_timestamp;
    opcua_boolean_t status;
    mu_string_t server_id;
    mu_string_t client_audit_entry_id;
    mu_string_t client_user_id;
    union {
        struct {
            mu_string_t secure_channel_id;
        } open_channel;
        struct {
            mu_nodeid_t session_id;
        } create_session;
        struct {
            mu_nodeid_t session_id;
            mu_string_t user_name;
        } activate_session;
        struct {
            mu_nodeid_t node_id;
            mu_variant_t old_value;
            mu_variant_t new_value;
        } write_update;
        struct {
            mu_nodeid_t object_id;
            mu_nodeid_t method_id;
        } method;
        struct {
            mu_nodeid_t condition_id;
            mu_nodeid_t session_id;
        } condition;
    } specific;
} mu_audit_event_t;

typedef void (*mu_audit_callback_t)(struct mu_server *server, const mu_audit_event_t *event, void *context);

/* Shared audit-payload pool (spec 074 Decision 7): full AuditEvent field values
 * captured at emit time and referenced from a queued event by index+sequence, so
 * the values are NOT multiplied into every event-queue slot. Static/bounded — no
 * heap. Ring: when full, the oldest slot is reused (a stale reference resolves to
 * Null, like event-queue overflow). */
#ifndef MU_MAX_AUDIT_PAYLOADS
#define MU_MAX_AUDIT_PAYLOADS 16u
#endif
#ifndef MU_AUDIT_STR_MAX
#define MU_AUDIT_STR_MAX 32
#endif

typedef struct {
    opcua_byte_t data[MU_AUDIT_STR_MAX];
    opcua_int32_t len; /* -1 = Null; 0..MU_AUDIT_STR_MAX = copied (truncated) length */
} mu_audit_str_t;

typedef struct {
    opcua_uint32_t sequence; /* ring generation stamp; 0 = empty slot */
    mu_nodeid_t source_node; /* the notifier (Server Object i=2253) */
    opcua_boolean_t status;
    opcua_datetime_t action_timestamp;
    mu_nodeid_t session_id;      /* create/activate-session audits */
    opcua_uint32_t attribute_id; /* write audit */
    mu_variant_t old_value;      /* write audit; scalar only (arrays -> Null) */
    mu_variant_t new_value;      /* write audit; scalar only */
    /* Bounded inline copies of the request-lifetime strings (source buffers are
       gone by Publish time): AuditEventType.ServerId / ClientUserId /
       ClientAuditEntryId and AuditChannelEventType.SecureChannelId. */
    mu_audit_str_t server_id;
    mu_audit_str_t client_user_id;
    mu_audit_str_t client_audit_entry_id;
    mu_audit_str_t secure_channel_id;
} mu_audit_payload_t;

/* Store `payload` in the server's audit pool and return a reference (index +
 * sequence) for embedding in the emitted event; resolves to Null once the ring
 * wraps past it. `mu_audit_ref_t` is defined in muc_opcua/types.h. */
mu_audit_ref_t mu_audit_pool_store(struct mu_server *server, const mu_audit_payload_t *payload);

#endif /* MUC_OPCUA_AUDITING */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_AUDIT_H */
