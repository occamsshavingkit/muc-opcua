#ifndef MUC_OPCUA_SERVICE_DISPATCH_COMMON_H
#define MUC_OPCUA_SERVICE_DISPATCH_COMMON_H

#include "../../services/discovery.h"
#include "../../services/secure_channel.h"
#include "../../services/service_header.h"
#include "../../services/session.h"
#include "../server_internal.h"
#include "../service_dispatch.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/transport.h"
#ifdef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
#include "../../services/history.h"
#endif
#ifdef MUC_OPCUA_CU_SUBSCRIPTION_BASIC
#include "../../services/subscription.h"
#endif
#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ
#include "../../services/read.h"
#endif
#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
#include "../../services/write.h"
#endif
#ifdef MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH
#include "../../services/browse.h"
#endif
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
#include "../../security/certificate.h"
#include "../../security/key_derivation.h"
#include "../../security/security_policy.h"
#include "../../security/sym_chunk.h"
#include "muc_opcua/security.h"
#endif
#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
#include "../../services/node_management.h"
#endif
#ifdef MUC_OPCUA_CU_QUERY
#include "../../services/query.h"

opcua_statuscode_t handle_query_first(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length);
opcua_statuscode_t handle_query_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                     size_t *response_length);
#endif
#include <stddef.h>
#include <string.h>

#ifndef MUC_OPCUA_CU_SUBSCRIPTION_BASIC
#ifndef MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
#ifndef MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
#ifndef MU_DISPATCH_CALL_ENABLED
#define MU_DISPATCH_CALL_ENABLED 0
#endif
#endif
#endif
#endif

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC && MUC_OPCUA_CU_SUBSCRIPTION_STANDARD && MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
#undef MU_DISPATCH_CALL_ENABLED
#define MU_DISPATCH_CALL_ENABLED 1
#endif

#define MU_SERVER_NONCE_LENGTH 32

#ifdef MUC_OPCUA_CU_TIME_SYNC
/* Spec 055: returns true if an OpenSecureChannel carrying client_time should be
 * allowed under MU_TIME_SYNC_MAX_CLOCK_SKEW_MS: within tolerance, or either side
 * 0 (unknown time / clockless peer). Both times are opcua_datetime_t 100ns ticks. */
bool mu_opn_time_sync_allows(opcua_datetime_t server_time, opcua_datetime_t client_time);
#endif

#define MU_DISPATCH_MAX_REGISTER_NODES 32
#define MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS 32

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
#define MU_DOUBLE_SIGN_BIT 0x8000000000000000ULL
#define MU_PUBLISHING_INTERVAL_MIN_BITS 0x4049000000000000ULL
#define MU_PUBLISHING_INTERVAL_MAX_BITS 0x40ed4c0000000000ULL
#define MU_MONITORED_VALUE_ATTRIBUTE_ID 13u
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
#define MU_ID_DATACHANGEFILTER_ENCODING_DEFAULTBINARY 724u
#ifdef MUC_OPCUA_CU_EVENTS
#define MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY 727u
#endif
#ifndef MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED
#define MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED ((opcua_statuscode_t)0x80440000)
#endif
#ifndef MU_STATUS_BAD_MONITOREDITEMFILTERINVALID
#define MU_STATUS_BAD_MONITOREDITEMFILTERINVALID ((opcua_statuscode_t)0x80430000)
#endif
#endif

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    opcua_uint32_t monitoring_mode;
    opcua_uint32_t client_handle;
    opcua_uint64_t sampling_interval_bits;
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    mu_datachange_trigger_t trigger;
    mu_deadband_type_t deadband_type;
    opcua_double_t deadband_value;
    opcua_statuscode_t filter_result;
    opcua_boolean_t has_datachange_filter;
    opcua_boolean_t has_aggregate;
    opcua_uint32_t aggregate_type;
    opcua_double_t processing_interval;
#endif
    opcua_uint32_t queue_size;
    opcua_boolean_t discard_oldest;
#ifdef MUC_OPCUA_CU_EVENTS
    opcua_byte_t select_clauses[8];
    opcua_byte_t select_clauses_count;
#if MUC_OPCUA_CU_EVENT_FILTER_WHERE
    mu_where_clause_t where_clause; /* decoded EventFilter WhereClause */
#endif
#endif
} mu_monitored_item_create_body_t;

typedef opcua_statuscode_t (*mu_subscription_id_result_fn)(mu_server_t *server, opcua_uint32_t id, void *context);

#endif

typedef struct {
    mu_service_handler_t service;
    opcua_statuscode_t (*handler)(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                  size_t *response_length);
} mu_service_descriptor_t;

opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w, opcua_uint32_t response_type_id,
                                         opcua_uint32_t request_handle, opcua_statuscode_t service_result
#ifdef MUC_OPCUA_CU_TIME_SYNC
                                         ,
                                         const mu_server_t *server
#endif
);
opcua_statuscode_t ensure_reader_bytes_remaining(const mu_binary_reader_t *r, size_t length);
opcua_statuscode_t skip_extension_object_body(mu_binary_reader_t *r, size_t length);
opcua_statuscode_t ensure_array_items_min_remaining(const mu_binary_reader_t *r, opcua_int32_t count,
                                                    size_t min_item_size);

void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy);
void mu_service_dispatch_set_opn_client_cert(mu_server_t *server, const mu_bytestring_t *client_cert);

const mu_string_t *current_opn_security_policy(const mu_server_t *server);
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
const mu_bytestring_t *current_opn_client_cert(const mu_server_t *server);
#endif

opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
opcua_statuscode_t handle_close_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length);

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
typedef struct {
    opcua_uint32_t subscription_id;
    opcua_uint32_t monitoring_mode;
} mu_set_monitoring_mode_context_t;

typedef struct {
    opcua_uint32_t subscription_id;
} mu_monitored_item_id_context_t;

opcua_statuscode_t handle_create_monitored_items(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
opcua_statuscode_t handle_modify_monitored_items(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
opcua_statuscode_t handle_set_monitoring_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
opcua_statuscode_t handle_delete_monitored_items(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
opcua_statuscode_t handle_set_triggering(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
#endif
#endif

opcua_statuscode_t handle_create_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
opcua_statuscode_t handle_activate_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length);
opcua_statuscode_t handle_close_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length);

#ifdef MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS
opcua_statuscode_t handle_get_endpoints(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length);
opcua_statuscode_t handle_find_servers(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length);
#endif

#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ
opcua_statuscode_t handle_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length);
#endif
#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length);
#endif

#ifdef MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH
opcua_statuscode_t handle_browse(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                 size_t *response_length);
opcua_statuscode_t handle_browse_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length);
opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
#endif

#ifdef MUC_OPCUA_CU_VIEW_REGISTERNODES
opcua_statuscode_t handle_register_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
opcua_statuscode_t handle_unregister_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length);
#endif

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

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC && MUC_OPCUA_CU_SUBSCRIPTION_STANDARD && MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
opcua_statuscode_t handle_call(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length);
#endif

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
#if MUC_OPCUA_CU_REDUNDANCY
opcua_statuscode_t handle_transfer_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length);
#endif

opcua_uint32_t publishing_interval_bits_to_ms(opcua_uint64_t bits);
opcua_uint64_t publishing_interval_ms_to_bits(opcua_uint32_t interval_ms);
opcua_statuscode_t drive_subscription_id_status_array(
    mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w, size_t *response_length,
    opcua_uint32_t response_type_id, opcua_uint32_t request_handle, bool validate_subscription_id,
    opcua_uint32_t subscription_id,
    opcua_statuscode_t (*item_result)(mu_server_t *server, opcua_uint32_t id, void *context), void *context);
opcua_statuscode_t read_monitored_item_create_body(mu_binary_reader_t *r, mu_monitored_item_create_body_t *body);
opcua_statuscode_t configure_monitored_item(mu_monitored_item_t *item, const mu_monitored_item_create_body_t *body,
                                            const mu_node_t *node, mu_subscription_t *sub,
                                            opcua_uint32_t timestamps_to_return, opcua_uint64_t now_ms,
                                            mu_server_t *server);
opcua_statuscode_t write_monitored_item_create_result(mu_binary_writer_t *w, opcua_statuscode_t result,
                                                      opcua_uint32_t monitored_item_id,
                                                      opcua_uint32_t sampling_interval_ms,
                                                      opcua_uint32_t revised_queue_size);
mu_monitored_item_t *find_monitored_item(mu_server_t *server, opcua_uint32_t subscription_id,
                                         opcua_uint32_t monitored_item_id);
opcua_statuscode_t write_null_extension_object(mu_binary_writer_t *w);
bool is_datachange_filter_binary_type(const mu_nodeid_t *type_id);
bool is_aggregate_filter_binary_type(const mu_nodeid_t *type_id);
#ifdef MUC_OPCUA_CU_EVENTS
bool is_event_filter_binary_type(const mu_nodeid_t *type_id);
#endif
bool is_known_monitoring_filter_binary_type(const mu_nodeid_t *type_id, size_t length);
opcua_statuscode_t read_datachange_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                               mu_monitored_item_create_body_t *body);
opcua_statuscode_t read_aggregate_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                              mu_monitored_item_create_body_t *body);
#ifdef MUC_OPCUA_CU_EVENTS
opcua_statuscode_t read_event_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                          mu_monitored_item_create_body_t *body);
#endif
opcua_statuscode_t set_monitoring_mode_result(mu_server_t *server, opcua_uint32_t monitored_item_id, void *context);
opcua_statuscode_t delete_monitored_item_result(mu_server_t *server, opcua_uint32_t monitored_item_id, void *context);
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
opcua_statuscode_t validate_create_item_filter(mu_server_t *server, const mu_monitored_item_create_body_t *body,
                                               const mu_node_t *node);
bool monitored_node_has_numeric_static_value(const mu_node_t *node);
#endif
#endif

#ifdef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
opcua_statuscode_t handle_history_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length);
opcua_statuscode_t handle_history_update(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
#endif

#endif
