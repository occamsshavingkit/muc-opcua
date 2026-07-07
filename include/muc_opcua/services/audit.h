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
    } specific;
} mu_audit_event_t;

typedef void (*mu_audit_callback_t)(struct mu_server *server, const mu_audit_event_t *event, void *context);

#endif /* MUC_OPCUA_AUDITING */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_AUDIT_H */
