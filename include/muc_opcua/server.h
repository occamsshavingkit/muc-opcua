/* include/muc_opcua/server.h */
/*
 * Spec grounding:
 *   OPC-10000-4 §5 OPC UA Services
 *   OPC-10000-7 §6.6 Server Profiles
 */
#ifndef MUC_OPCUA_SERVER_H
#define MUC_OPCUA_SERVER_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/config.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/security.h"
#include "muc_opcua/status.h"
#ifdef MUC_OPCUA_PUBSUB
#include "muc_opcua/pubsub.h"
#endif
#ifdef MUC_OPCUA_SERVICE_HISTORY
#include "muc_opcua/services/history.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque Server State
 * Allocated by the user (usually statically) to avoid heap usage.
 */
typedef struct mu_server mu_server_t;

typedef opcua_statuscode_t (*mu_user_auth_handler_t)(void *handle, const mu_string_t *username,
                                                     const mu_bytestring_t *password, const mu_string_t *policy_id);

typedef opcua_statuscode_t (*mu_write_handler_t)(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                                 const mu_datavalue_t *value);

/* Server Configuration */
typedef struct {
    /* Identity and Discovery */
    const char *endpoint_url;
#if MUC_OPCUA_REVERSE_CONNECT
    const char *reverse_connect_url;
#endif
    const char *application_uri;
    const char *product_uri;
    const char *application_name;

    /* Buffers for networking (caller-owned to avoid heap) */
    opcua_byte_t *receive_buffer;
    size_t receive_buffer_size;
    opcua_byte_t *send_buffer;
    size_t send_buffer_size;

    /* Limits */
    opcua_uint32_t max_chunk_count;
    opcua_uint32_t max_message_size;
    opcua_uint32_t max_sessions;
    opcua_uint32_t max_secure_channels;

    /* Platform Adapters */
    mu_tcp_adapter_t tcp_adapter;
    mu_time_adapter_t time_adapter;
    mu_entropy_adapter_t entropy_adapter;

    /* Optional Adapters */
    mu_persistence_adapter_t *persistence_adapter; /* NULL if unsupported */
    mu_crypto_adapter_t *crypto_adapter;           /* NULL if unsupported */

    /* Static Address Space (optional) */
    const mu_address_space_t *address_space;

    /* Authorization */
    opcua_boolean_t allow_node_management;

    /* User Authentication (optional) */
    mu_user_auth_handler_t user_auth_handler;
    void *user_auth_handler_handle;

    /* TrustList for Application Authentication (optional) */
    const mu_trust_list_t *trust_list;

    /* Feature 025 (F3): explicit opt-in to accept client application-instance
       certificates that are not in the trust list, for a non-None SecurityPolicy.
       Defaults to false (fail closed): a secured server with no trust list rejects
       connections rather than silently accepting any certificate. Set true ONLY
       for demos/interop/bring-up where clients present freshly generated
       self-signed certificates; it disables application authentication and MUST
       NOT be used in production. Certificate validity (notBefore/notAfter) is
       still enforced. */
    opcua_boolean_t allow_untrusted_clients;

#ifdef MUC_OPCUA_AUDITING
    opcua_boolean_t auditing_enabled;
#endif

#ifdef MUC_OPCUA_PUBSUB
    /* PubSub Configuration (optional) */
    mu_pubsub_connection_t pubsub;
    mu_udp_adapter_t udp_adapter;
#endif

    /* Write Service Callback (optional) */
#ifdef MUC_OPCUA_SERVICE_WRITE
    mu_write_handler_t write_handler;
    void *write_handler_handle;
#endif

#ifdef MUC_OPCUA_SERVICE_HISTORY
    mu_history_adapter_t history_adapter;
#endif

    /* mDNS Discovery Adapter (optional) — NULL = disabled, zero storage impact */
    mu_mdns_adapter_t *mdns_adapter;

} mu_server_config_t;

/*
 * Initialize the server with the given config and static storage.
 * Storage must be at least MU_SERVER_STORAGE_BYTES bytes and aligned for the
 * server struct (it holds pointers/size_t); a plain `static opcua_byte_t[]` is
 * over-aligned by the compiler, but a byte/offset buffer is rejected.
 */
opcua_statuscode_t mu_server_init(void *storage, size_t storage_size, const mu_server_config_t *config,
                                  mu_server_t **out_server);

/*
 * Run a single non-blocking iteration of the server.
 * This handles accepting connections, reading, parsing, dispatching, and writing.
 */
opcua_statuscode_t mu_server_poll(mu_server_t *server);

/*
 * Cleanly close all connections and shutdown the server.
 */
void mu_server_close(mu_server_t *server);

/*
 * Validation API for configuration.
 */
opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config);

#ifdef MUC_OPCUA_CUSTOM_METHODS
/*
 * Register a callback handler for a method node.
 */
opcua_statuscode_t mu_server_register_method_callback(mu_server_t *server, const mu_nodeid_t *method_id,
                                                      mu_method_callback_t callback, void *context);
#endif

#ifdef MUC_OPCUA_EVENTS
/*
 * Trigger an Event to be published to all subscriptions that monitor events.
 */
opcua_statuscode_t mu_server_trigger_event(mu_server_t *server, const mu_event_notification_t *event);
#endif

#ifdef MUC_OPCUA_AUDITING
#include "muc_opcua/services/audit.h"
void mu_raise_audit_event(mu_server_t *server, const mu_audit_event_t *event);
void mu_server_set_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context);
opcua_statuscode_t mu_server_add_audit_callback(mu_server_t *server, mu_audit_callback_t callback, void *context);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVER_H */
