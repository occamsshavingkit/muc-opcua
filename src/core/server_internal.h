/* src/core/server_internal.h */
#ifndef MUC_OPCUA_SERVER_INTERNAL_H
#define MUC_OPCUA_SERVER_INTERNAL_H

#include "../address_space/base_nodes.h"
#include "../services/read/common.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#include "../services/subscription.h"
#include "message_chunk.h"
#include "muc_opcua/address_space/complex_types.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/alarms_conditions.h"
#include "muc_opcua/services/diagnostics.h"
#include "service_dispatch.h"
#include "tcp_connection.h"

/* Scratch layout within server->secure_scratch (all under MUC_OPCUA_FACET_CORE_2022_SERVER):
   [0,                  MU_SECURE_RESP_MAX)           response body
   [MU_SECURE_RESP_MAX, MU_SECURE_RESP_MAX + OPN_MAX) OPN request unwrap
   [RESP_MAX + OPN_MAX, end)                          session handshake buffers
   Details and _Static_asserts in server.c. */
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
#define MU_SECURE_RESP_MAX 11264
#define MU_SECURE_OPN_REQ_MAX 1024
#define MU_SECURE_SESSION_OFFSET (MU_SECURE_RESP_MAX + MU_SECURE_OPN_REQ_MAX)

/* Session-handshake scratch region — used by CreateSession (to_sign/sig) and
   ActivateSession (verify_buf/decrypt_buf). Handler writes the response body into
   respbody BEFORE signing/verification, so the session region MUST NOT overlap. */
#define MU_SECURE_SESSION_MAX 2048
#define MU_SESSION_TO_SIGN_SIZE 1536
#define MU_SESSION_SIG_SIZE 512
#endif

#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
typedef struct {
    mu_nodeid_t source_node_id;
    mu_reference_t ref;
} mu_dynamic_reference_t;

typedef struct {
    mu_node_t nodes[MU_INTERN_MAX_DYNAMIC_NODES];
    opcua_byte_t browse_name_storage[MU_INTERN_MAX_DYNAMIC_NODES][MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH];
    opcua_byte_t display_name_storage[MU_INTERN_MAX_DYNAMIC_NODES][MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH];
    opcua_byte_t string_nodeid_storage[MU_INTERN_MAX_DYNAMIC_NODES][MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH];
    size_t nodes_count;

    mu_dynamic_reference_t references[MU_INTERN_MAX_DYNAMIC_REFERENCES];
    opcua_byte_t reference_source_nodeid_storage[MU_INTERN_MAX_DYNAMIC_REFERENCES]
                                                [MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH];
    opcua_byte_t reference_type_nodeid_storage[MU_INTERN_MAX_DYNAMIC_REFERENCES]
                                              [MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH];
    opcua_byte_t reference_target_nodeid_storage[MU_INTERN_MAX_DYNAMIC_REFERENCES]
                                                [MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH];
    size_t references_count;
} mu_dynamic_address_space_t;
#endif

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
typedef struct {
    void *client_handle;
    size_t rx_len;
    size_t rx_read_pos; /* start offset of unconsumed data in rx_buffer (HP8) */
    opcua_uint64_t last_activity_ms;
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;
    opcua_byte_t rx_buffer[MU_CONNECTION_RX_BUFFER_SIZE];
} mu_connection_t;

_Static_assert(sizeof(mu_connection_t) <= (MU_CONNECTION_RX_BUFFER_SIZE + MU_CONNECTION_BASE_STORAGE_BYTES),
               "MU_CONNECTION_BASE_STORAGE_BYTES must cover mu_connection_t fields outside rx_buffer");
#endif

struct mu_server {
    mu_server_config_t config;
    mu_base_runtime_nodes_t runtime_base;
    mu_address_space_index_t user_address_space_index;
    opcua_boolean_t is_running;

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
    mu_connection_t conns[MU_INTERN_MAX_CONNECTIONS];
    mu_connection_t *active_conn;
#endif
    void *client_handle;
    size_t rx_len;                   /* bytes accumulated in config.receive_buffer (stream reassembly) */
    size_t rx_read_pos;              /* start offset of unconsumed data in receive_buffer (HP8) */
    opcua_uint64_t last_activity_ms; /* monotonic tick of last inbound traffic (idle timeout) */
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;
#ifdef MUC_OPCUA_CU_MULTI_CHUNK
    mu_chunk_assembler_t chunk_assembly;
#endif

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
#define server_secure_channel (*(server->active_conn ? &server->active_conn->secure_channel : &server->secure_channel))
#define server_tcp_conn (*(server->active_conn ? &server->active_conn->tcp_conn : &server->tcp_conn))
#define server_client_handle (*(server->active_conn ? &server->active_conn->client_handle : &server->client_handle))
#define server_rx_len (*(server->active_conn ? &server->active_conn->rx_len : &server->rx_len))
#define server_rx_read_pos (*(server->active_conn ? &server->active_conn->rx_read_pos : &server->rx_read_pos))
#define server_last_activity_ms                                                                                        \
    (*(server->active_conn ? &server->active_conn->last_activity_ms : &server->last_activity_ms))
#define server_chunk_assembly (server->chunk_assembly)
#else
#define server_secure_channel (server->secure_channel)
#define server_tcp_conn (server->tcp_conn)
#define server_client_handle (server->client_handle)
#define server_rx_len (server->rx_len)
#define server_rx_read_pos (server->rx_read_pos)
#define server_last_activity_ms (server->last_activity_ms)
#define server_chunk_assembly (server->chunk_assembly)
#endif

    mu_string_t opn_pending_security_policy;
    mu_bytestring_t opn_pending_client_cert;
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
    opcua_byte_t secure_scratch[MU_SECURE_SCRATCH_SIZE];
    /* Persistent copy of the current SecureChannel's client application-instance
       certificate, populated at OpenSecureChannel and used to verify the
       ActivateSession ClientSignature (OPC-10000-4 §5.7.3). Needed because the OPN
       sender certificate points into transient receive-buffer memory. */
    opcua_byte_t channel_client_cert[MU_MAX_CLIENT_CERT_SIZE];
    size_t channel_client_cert_len;
#endif
    mu_session_t sessions[MU_INTERN_MAX_SESSIONS];
    mu_session_t *active_session;
#if defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS
    /* Current request's returnDiagnostics header, set per-request by
       mu_service_dispatch and read by the response-header writers. Per-instance
       state (the server is synchronous), replacing a former file-static. */
    opcua_uint32_t return_diagnostics;
#endif
#if MUC_OPCUA_READ_CACHE
    mu_read_cache_t read_cache;
#endif
#if defined(MUC_OPCUA_CU_SESSION_TIMEOUT)
    opcua_uint64_t next_session_timeout_ms;
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    mu_subscriptions_t subs;
    opcua_uint32_t current_request_id;
#endif
#if MUC_OPCUA_CU_METHOD_SERVER
#define MU_MAX_REGISTERED_METHODS 16
    struct {
        mu_nodeid_t method_id;
        mu_method_callback_t callback;
        void *context;
        const mu_argument_t *input_args;
        size_t input_count;
        const mu_argument_t *output_args;
        size_t output_count;
        opcua_boolean_t executable;
    } registered_methods[MU_MAX_REGISTERED_METHODS];
    size_t registered_method_count;
#endif

#ifdef MUC_OPCUA_CU_PUBSUB
#define MU_MAX_WRITER_GROUPS 2
    mu_pubsub_writer_group_t writer_groups[MU_MAX_WRITER_GROUPS];
    size_t writer_group_count;
#endif

#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
    mu_dynamic_address_space_t dynamic_address_space;
#endif

#ifdef MUC_OPCUA_CU_QUERY
    struct {
        struct {
            opcua_byte_t id_buf[8];
            mu_string_t id;
            opcua_uint32_t session_id;
            size_t next_index; /* Index into address space to resume from */
            opcua_uint64_t timestamp_ms;
        } continuation_points[MU_INTERN_MAX_QUERY_CONTINUATION_POINTS];
    } query_context;
#endif

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    mu_condition_t conditions[MU_INTERN_MAX_CONDITIONS];
    size_t condition_count;
#endif

#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS
    mu_diagnostics_summary_t diag;
#endif

#if MUC_OPCUA_CU_COMPLEX_TYPES
#define MU_MAX_COMPLEX_TYPE_ENTRIES 8
    struct {
        const mu_structure_definition_t *structures[MU_MAX_COMPLEX_TYPE_ENTRIES];
        mu_nodeid_t structure_ids[MU_MAX_COMPLEX_TYPE_ENTRIES];
        opcua_uint16_t structure_count;
        const mu_enum_definition_t *enums[MU_MAX_COMPLEX_TYPE_ENTRIES];
        mu_nodeid_t enum_ids[MU_MAX_COMPLEX_TYPE_ENTRIES];
        opcua_uint16_t enum_count;
    } complex_types;
#endif

#ifdef MUC_OPCUA_CU_AUDITING
#define MU_MAX_AUDIT_CALLBACKS 4
    struct {
        mu_audit_callback_t callback;
        void *context;
    } audit_callbacks[MU_MAX_AUDIT_CALLBACKS];
    size_t audit_callback_count;
    /* Shared audit-payload ring (spec 074): full AuditEvent field values live
       here, referenced by index+sequence from queued events (audit.h). */
    mu_audit_payload_t audit_pool[MU_MAX_AUDIT_PAYLOADS];
    opcua_byte_t audit_pool_next;       /* ring write index */
    opcua_uint32_t audit_pool_sequence; /* monotonic stamp; never 0 (0 = empty slot) */
#endif
};

_Static_assert(MU_SERVER_STORAGE_BYTES >= sizeof(struct mu_server),
               "MU_SERVER_STORAGE_BYTES must cover struct mu_server for enabled feature gates");

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
opcua_statuscode_t mu_server_emit_message(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *body,
                                          size_t body_len);

/* White-box exposure (T002/T007) of the publish-timer advance helper so the
 * iteration bound can be unit-tested directly, independent of the
 * multi-connection tick path. OPC-10000-4 §5.13.1.2. */
void advance_publish_timer(mu_subscription_t *sub, opcua_uint64_t now_ms);

void advance_sample_timer(mu_monitored_item_t *item, opcua_uint64_t now_ms);
opcua_statuscode_t read_monitored_item_value(const mu_monitored_item_t *item, mu_variant_t *cur);
bool monitored_item_sample_changed(const mu_monitored_item_t *item, const mu_variant_t *cur, opcua_statuscode_t status);
bool monitored_item_reportable(const mu_monitored_item_t *item, const mu_subscription_t *sub);
#endif

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
bool variant_numeric_to_double(const mu_variant_t *value, opcua_double_t *out);
bool monitored_item_change_reportable(const mu_monitored_item_t *item, const mu_variant_t *cur,
                                      opcua_statuscode_t status);
opcua_uint32_t monitored_item_effective_queue_size(mu_monitored_item_t *item);
opcua_byte_t monitored_item_queue_next(opcua_byte_t index, opcua_uint32_t queue_size);
void monitored_item_enqueue_report(mu_monitored_item_t *item, const mu_variant_t *cur, opcua_statuscode_t status);
void monitored_item_prepare_pending_queue(mu_monitored_item_t *item);
void monitored_item_accumulate_aggregate(mu_monitored_item_t *item, const mu_variant_t *cur);
void monitored_item_publish_aggregate(mu_monitored_item_t *item, opcua_uint64_t now_ms);
bool monitored_item_in_subscription(const mu_monitored_item_t *item, const mu_subscription_t *sub);
bool monitored_item_reports_by_trigger(const struct mu_server *server, const mu_subscription_t *sub,
                                       const mu_monitored_item_t *linked_item);
#endif

/* Shared dispatch helpers — extracted modules call these across translation-unit
 * boundaries (T008+). Definitions live in service_dispatch.c. */
opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w, opcua_uint32_t response_type_id,
                                         opcua_uint32_t request_handle, opcua_statuscode_t service_result
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                         ,
                                         const mu_server_t *server
#endif
);
opcua_statuscode_t ensure_reader_bytes_remaining(const mu_binary_reader_t *r, size_t length);
opcua_statuscode_t skip_extension_object_body(mu_binary_reader_t *r, size_t length);
opcua_statuscode_t ensure_array_items_min_remaining(const mu_binary_reader_t *r, opcua_int32_t count,
                                                    size_t min_item_size);

/* SecureChannel service handler — implemented in service_dispatch.c (T001).
 * OpenSecureChannel remains static in service_dispatch.c; CloseSecureChannel is
 * exposed for direct unit testing. Grounding: OPC-10000-4 §5.6.3.2 Table 13. */
opcua_statuscode_t handle_close_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length);

/* Session service handlers — implemented in dispatch_session.c (T008). */
opcua_statuscode_t handle_create_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
opcua_statuscode_t handle_activate_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length);
opcua_statuscode_t handle_close_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length);

/* Discovery service handlers. Dispatch registers each Service only when its
 * CU gate is enabled (OPC-10000-4 sections 5.5.1 and 5.5.4). */
#ifdef MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS
opcua_statuscode_t handle_get_endpoints(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length);
#endif
#ifdef MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED
opcua_statuscode_t handle_find_servers(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length);
#endif

/* Read/Write attribute service handlers — implemented in dispatch_attribute.c
 * (T010). The dispatch table in service_dispatch.c references these by name. */
#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ
opcua_statuscode_t handle_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length);
#endif
#ifdef MUC_OPCUA_SERVICE_WRITE
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length);
#endif

/* Browse-family service handlers — implemented in dispatch_view.c (T011).
 * The dispatch table in service_dispatch.c references these by name. */
#ifdef MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH
opcua_statuscode_t handle_browse(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                 size_t *response_length);
opcua_statuscode_t handle_browse_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length);
opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
#endif

/* Subscription service-set handlers — implemented in dispatch_subscription.c
 * (T012). The dispatch table in service_dispatch.c references these by name;
 * the MonitoredItems handlers remain in service_dispatch.c. */
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
opcua_statuscode_t handle_create_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
opcua_statuscode_t handle_modify_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
opcua_statuscode_t handle_set_publishing_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
opcua_statuscode_t handle_delete_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length);
opcua_statuscode_t handle_publish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                  size_t *response_length);
opcua_statuscode_t handle_republish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                    size_t *response_length);

/* Shared subscription dispatch helpers — defined in service_dispatch.c, shared
 * with the MonitoredItems handlers that remain there (T012). */
opcua_uint32_t publishing_interval_bits_to_ms(opcua_uint64_t bits);
opcua_uint64_t publishing_interval_ms_to_bits(opcua_uint32_t interval_ms);
opcua_statuscode_t drive_subscription_id_status_array(
    mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w, size_t *response_length,
    opcua_uint32_t response_type_id, opcua_uint32_t request_handle, bool validate_subscription_id,
    opcua_uint32_t subscription_id,
    opcua_statuscode_t (*item_result)(mu_server_t *server, opcua_uint32_t id, void *context), void *context);
#endif

/* NodeManagement service-set handlers — implemented in dispatch_node_mgmt.c
 * (T013). The dispatch table in service_dispatch.c references these by name. */
#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
opcua_statuscode_t handle_add_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                    size_t *response_length);
opcua_statuscode_t handle_add_references(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
opcua_statuscode_t handle_delete_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length);
opcua_statuscode_t handle_delete_references(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                            size_t *response_length);
#endif

/* Call service handler — implemented in dispatch_method.c (T014). The dispatch
 * table in service_dispatch.c references this by name. The guard matches the
 * original MU_DISPATCH_CALL_ENABLED condition in service_dispatch.c
 * (MUC_OPCUA_METHOD_CALL is not defined in this codebase). */
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC && MUC_OPCUA_CU_SUBSCRIPTION_STANDARD && MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
opcua_statuscode_t handle_call(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length);
#endif

#if MUC_OPCUA_CU_REDUNDANCY
opcua_statuscode_t handle_transfer_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
#endif

#endif /* MUC_OPCUA_SERVER_INTERNAL_H */
