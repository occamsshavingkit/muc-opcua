/* src/core/service_dispatch.c */
#include "service_dispatch.h"
#include "../services/discovery.h"
#include "../services/read.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/transport.h"
#include "server_internal.h"
#ifdef MUC_OPCUA_SERVICE_HISTORY
#include "../services/history.h"
#endif
#include "../services/service_header.h"
#ifdef MUC_OPCUA_SECURITY
#include "../security/certificate.h"
#include "../security/key_derivation.h"
#include "../security/security_policy.h"
#include "../security/sym_chunk.h"
#include "muc_opcua/security.h"
#endif
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
#include "../services/node_management.h"
#endif
#include <stddef.h>
#include <string.h>

#define MU_SERVER_NONCE_LENGTH 32

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM
#define MU_DISPATCH_CALL_ENABLED 1
#else
#define MU_DISPATCH_CALL_ENABLED 0
#endif

#ifdef MUC_OPCUA_SERVICE_BROWSE
/* mu_browse_process_with_user_index forward declaration moved to
   dispatch_view.c (T011), the only caller after Browse-family handlers were
   extracted. */
#endif

typedef opcua_statuscode_t (*mu_service_dispatch_handler_fn)(mu_server_t *server, mu_binary_reader_t *r,
                                                             mu_binary_writer_t *w, size_t *response_length);

typedef struct {
    mu_service_handler_t service;
    mu_service_dispatch_handler_fn handler;
} mu_service_descriptor_t;

static opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
#if MUC_OPCUA_SUBSCRIPTIONS
/* handle_create_subscription, handle_modify_subscription, handle_set_publishing_mode,
 *   handle_delete_subscriptions, handle_publish, handle_republish and their
 *   exclusive helpers (set_publishing_mode_result, delete_subscription_result)
 *   moved to dispatch_subscription.c (T012). Extern prototypes live in
 *   server_internal.h under MUC_OPCUA_SUBSCRIPTIONS; the dispatch table below
 *   references them by name. publishing_interval_bits_to_ms,
 *   publishing_interval_ms_to_bits and drive_subscription_id_status_array stay
 *   here (shared with the MonitoredItems handlers below) and are now non-static
 *   extern so dispatch_subscription.c can call them. */
static opcua_statuscode_t handle_create_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
static opcua_statuscode_t handle_modify_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
static opcua_statuscode_t handle_set_monitoring_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static opcua_statuscode_t handle_set_triggering(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length);
#endif
static opcua_statuscode_t handle_delete_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
#endif /* MUC_OPCUA_SUBSCRIPTIONS */
#ifdef MUC_OPCUA_SERVICE_REGISTER_NODES
static opcua_statuscode_t handle_register_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length);
static opcua_statuscode_t handle_unregister_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                  size_t *response_length);
#endif
#ifdef MUC_OPCUA_SERVICE_BROWSE
/* handle_browse, handle_browse_next, handle_translate_browse_paths and the
   static helper browse_name_equals moved to dispatch_view.c (T011). Extern
   prototypes live in server_internal.h under MUC_OPCUA_SERVICE_BROWSE; the
   dispatch table above references them by name. */
#endif
#ifdef MUC_OPCUA_SERVICE_HISTORY
opcua_statuscode_t handle_history_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length);
opcua_statuscode_t handle_history_update(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
#endif
/* handle_call moved to dispatch_method.c (T014). Extern prototype lives in
 * server_internal.h under the MU_DISPATCH_CALL_ENABLED-expanding condition; the
 * dispatch table below references it by name. */
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
/* handle_add_nodes, handle_add_references, handle_delete_nodes, and
 *   handle_delete_references moved to dispatch_node_mgmt.c (T013). Extern
 *   prototypes live in server_internal.h under
 *   MUC_OPCUA_SERVICE_NODEMANAGEMENT; the dispatch table below references them
 *   by name. */
#endif

/* write_response_prefix, ensure_reader_bytes_remaining, skip_extension_object_body,
   and ensure_array_items_min_remaining are shared across the dispatch_* modules
   (T008+); their extern prototypes live in server_internal.h. */

#ifdef MUC_OPCUA_SERVICE_QUERY
#include "../services/query.h"
static opcua_statuscode_t handle_query_first(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                             size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t status = mu_request_header_decode(r, &req_header);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    mu_query_first_request_t req;
    mu_query_first_response_t resp;

    mu_node_type_description_t node_types[4];
    mu_content_filter_element_t filter_elements[4];
    mu_filter_operand_t filter_operands[8];
    mu_query_data_set_t data_sets[16];

    status = mu_query_first_request_decode(r, &req, node_types, 4, filter_elements, 4, filter_operands, 8);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_query_first_process(server, &req, &resp, data_sets, 16);

    opcua_statuscode_t wstatus = write_response_prefix(w, MU_ID_QUERYFIRSTRESPONSE, req_header.request_handle, status);
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }
    wstatus = mu_query_first_response_encode(w, &resp);
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }

    *response_length = w->position;
    return status;
}

static opcua_statuscode_t handle_query_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                            size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t status = mu_request_header_decode(r, &req_header);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    mu_query_next_request_t req;
    mu_query_next_response_t resp;
    mu_query_data_set_t data_sets[16];

    status = mu_query_next_request_decode(r, &req);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_query_next_process(server, &req, &resp, data_sets, 16);

    opcua_statuscode_t wstatus = write_response_prefix(w, MU_ID_QUERYNEXTRESPONSE, req_header.request_handle, status);
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }
    wstatus = mu_query_next_response_encode(w, &resp);
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }

    *response_length = w->position;
    return status;
}
#endif

/* Service dispatch table. Entries MUST appear in ascending order of
   service.request_id: find_service_descriptor() performs a binary search.
   Grounding: OPC-10000-4 §4.1 (Service Set model) and §7.1 — Services are
   dispatched by the numeric identifier of their request NodeId, and the
   specification assigns those identifiers in ascending order per Service Set.
   Each entry retains its own feature-guard so any subset produced by the
   preprocessor remains a sorted subsequence. */
static const mu_service_descriptor_t g_supported_services[] = {
#ifdef MUC_OPCUA_SERVICE_DISCOVERY
    {{MU_ID_FINDSERVERSREQUEST, MU_ID_FINDSERVERSRESPONSE, false}, handle_find_servers},
    {{MU_ID_GETENDPOINTSREQUEST, MU_ID_GETENDPOINTSRESPONSE, false}, handle_get_endpoints},
#endif
    {{MU_ID_OPENSECURECHANNELREQUEST, MU_ID_OPENSECURECHANNELRESPONSE, false}, handle_open_secure_channel},
    {{MU_ID_CLOSESECURECHANNELREQUEST, MU_ID_CLOSESECURECHANNELRESPONSE, false}, handle_close_secure_channel},
    {{MU_ID_CREATESESSIONREQUEST, MU_ID_CREATESESSIONRESPONSE, false},
     handle_create_session}, /* Does not require an activated session */
    {{MU_ID_ACTIVATESESSIONREQUEST, MU_ID_ACTIVATESESSIONRESPONSE, false},
     handle_activate_session}, /* Does not require an activated session to activate */
    {{MU_ID_CLOSESESSIONREQUEST, MU_ID_CLOSESESSIONRESPONSE, true}, handle_close_session},
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
    {{MU_ID_ADDNODESREQUEST, MU_ID_ADDNODESRESPONSE, true}, handle_add_nodes},
    {{MU_ID_ADDREFERENCESREQUEST, MU_ID_ADDREFERENCESRESPONSE, true}, handle_add_references},
    {{MU_ID_DELETENODESREQUEST, MU_ID_DELETENODESRESPONSE, true}, handle_delete_nodes},
    {{MU_ID_DELETEREFERENCESREQUEST, MU_ID_DELETEREFERENCESRESPONSE, true}, handle_delete_references},
#endif
#ifdef MUC_OPCUA_SERVICE_BROWSE
    {{MU_ID_BROWSEREQUEST, MU_ID_BROWSERESPONSE, true}, handle_browse},
    {{MU_ID_BROWSENEXTREQUEST, MU_ID_BROWSENEXTRESPONSE, true}, handle_browse_next},
    {{MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, true},
     handle_translate_browse_paths},
#endif
#ifdef MUC_OPCUA_SERVICE_REGISTER_NODES
    {{MU_ID_REGISTERNODESREQUEST, MU_ID_REGISTERNODESRESPONSE, true}, handle_register_nodes},
    {{MU_ID_UNREGISTERNODESREQUEST, MU_ID_UNREGISTERNODESRESPONSE, true}, handle_unregister_nodes},
#endif
#ifdef MUC_OPCUA_SERVICE_QUERY
    {{MU_ID_QUERYFIRSTREQUEST, MU_ID_QUERYFIRSTRESPONSE, true}, handle_query_first},
    {{MU_ID_QUERYNEXTREQUEST, MU_ID_QUERYNEXTRESPONSE, true}, handle_query_next},
#endif
#ifdef MUC_OPCUA_SERVICE_READ
    {{MU_ID_READREQUEST, MU_ID_READRESPONSE, true}, handle_read},
#endif
#ifdef MUC_OPCUA_SERVICE_HISTORY
    {{MU_ID_HISTORYREADREQUEST, MU_ID_HISTORYREADRESPONSE, true}, handle_history_read},
#endif
#ifdef MUC_OPCUA_SERVICE_WRITE
    {{MU_ID_WRITEREQUEST, MU_ID_WRITERESPONSE, true}, handle_write},
#endif
#ifdef MUC_OPCUA_SERVICE_HISTORY
    {{MU_ID_HISTORYUPDATEREQUEST, MU_ID_HISTORYUPDATEREQUEST, true}, handle_history_update},
#endif
#if MU_DISPATCH_CALL_ENABLED
    {{MU_ID_CALLREQUEST, MU_ID_CALLRESPONSE, true}, handle_call},
#endif
#if MUC_OPCUA_SUBSCRIPTIONS
    {{MU_ID_CREATEMONITOREDITEMSREQUEST, MU_ID_CREATEMONITOREDITEMSRESPONSE, true}, handle_create_monitored_items},
    {{MU_ID_MODIFYMONITOREDITEMSREQUEST, MU_ID_MODIFYMONITOREDITEMSRESPONSE, true}, handle_modify_monitored_items},
    {{MU_ID_SETMONITORINGMODEREQUEST, MU_ID_SETMONITORINGMODERESPONSE, true}, handle_set_monitoring_mode},
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    {{MU_ID_SETTRIGGERINGREQUEST, MU_ID_SETTRIGGERINGRESPONSE, true}, handle_set_triggering},
#endif
    {{MU_ID_DELETEMONITOREDITEMSREQUEST, MU_ID_DELETEMONITOREDITEMSRESPONSE, true}, handle_delete_monitored_items},
    {{MU_ID_CREATESUBSCRIPTIONREQUEST, MU_ID_CREATESUBSCRIPTIONRESPONSE, true}, handle_create_subscription},
    {{MU_ID_MODIFYSUBSCRIPTIONREQUEST, MU_ID_MODIFYSUBSCRIPTIONRESPONSE, true}, handle_modify_subscription},
    {{MU_ID_SETPUBLISHINGMODEREQUEST, MU_ID_SETPUBLISHINGMODERESPONSE, true}, handle_set_publishing_mode},
    {{MU_ID_PUBLISHREQUEST, MU_ID_PUBLISHRESPONSE, true}, handle_publish},
    {{MU_ID_REPUBLISHREQUEST, MU_ID_REPUBLISHRESPONSE, true}, handle_republish},
#if MUC_OPCUA_REDUNDANCY
    {{MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, true}, handle_transfer_subscriptions},
#endif
    {{MU_ID_DELETESUBSCRIPTIONSREQUEST, MU_ID_DELETESUBSCRIPTIONSRESPONSE, true}, handle_delete_subscriptions},
#endif
};

static const size_t g_num_supported_services = sizeof(g_supported_services) / sizeof(g_supported_services[0]);

/* Binary search over g_supported_services[] (sorted by service.request_id).
   Reduces dispatch lookup from O(N) (~15 comparisons across the full table) to
   O(log N) (≤6 comparisons worst case for the largest profile; ≤5 for typical
   profiles). Grounding: OPC-10000-4 §4.1, §7.1. */
static const mu_service_descriptor_t *find_service_descriptor(opcua_uint32_t request_id) {
    size_t lo = 0;
    size_t hi = g_num_supported_services;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        opcua_uint32_t mid_id = g_supported_services[mid].service.request_id;
        if (mid_id == request_id) {
            return &g_supported_services[mid];
        }
        if (mid_id < request_id) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

/* OPN SecurityPolicyUri is carried in the asymmetric chunk header, while normal
   service dispatch receives only the service body. server.c sets this field
   immediately around OPN dispatch so the handler can validate the requested
   policy without changing the public dispatch signature. */
void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy);

void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy) {
    if (server == NULL) {
        return;
    }

    if (security_policy == NULL) {
        server->opn_pending_security_policy.length = -1;
        server->opn_pending_security_policy.data = NULL;
        return;
    }

    server->opn_pending_security_policy = *security_policy;
}

static const mu_string_t *current_opn_security_policy(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return &server->opn_pending_security_policy;
}

void mu_service_dispatch_set_opn_client_cert(mu_server_t *server, const mu_bytestring_t *client_cert);

void mu_service_dispatch_set_opn_client_cert(mu_server_t *server, const mu_bytestring_t *client_cert) {
    if (server == NULL) {
        return;
    }

    if (client_cert == NULL) {
        server->opn_pending_client_cert.length = -1;
        server->opn_pending_client_cert.data = NULL;
        /* Keep the persistent per-channel copy (channel_client_cert): it must
           outlive OPN so ActivateSession can verify the ClientSignature. */
        return;
    }

    server->opn_pending_client_cert = *client_cert;
#ifdef MUC_OPCUA_SECURITY
    /* Persist a copy of the client application-instance certificate for the
       channel lifetime (OPC-10000-4 §5.7.3 ActivateSession ClientSignature). */
    if (client_cert->length > 0 && client_cert->data != NULL &&
        (size_t)client_cert->length <= sizeof(server->channel_client_cert)) {
        memcpy(server->channel_client_cert, client_cert->data, (size_t)client_cert->length);
        server->channel_client_cert_len = (size_t)client_cert->length;
    } else {
        server->channel_client_cert_len = 0;
    }
#endif
}

#ifdef MUC_OPCUA_SECURITY
static const mu_bytestring_t *current_opn_client_cert(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return &server->opn_pending_client_cert;
}
#endif

const mu_service_handler_t *mu_get_service_handler(opcua_uint32_t request_id) {
    const mu_service_descriptor_t *descriptor = find_service_descriptor(request_id);
    return descriptor != NULL ? &descriptor->service : NULL;
}

/* Write the common response prefix: response-type NodeId + ResponseHeader. */
opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w, opcua_uint32_t response_type_id,
                                         opcua_uint32_t request_handle, opcua_statuscode_t service_result) {
    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {response_type_id}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    return mu_response_header_encode(w, &rh);
}

opcua_statuscode_t ensure_reader_bytes_remaining(const mu_binary_reader_t *r, size_t length) {
    if (length > 0u) {
        if (r->position > r->length || length > (r->length - r->position)) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t skip_extension_object_body(mu_binary_reader_t *r, size_t length) {
    opcua_statuscode_t s = ensure_reader_bytes_remaining(r, length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    r->position += length;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t ensure_array_items_min_remaining(const mu_binary_reader_t *r, opcua_int32_t count,
                                                    size_t min_item_size) {
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (count <= 0) {
        return MU_STATUS_GOOD;
    }
    if (r->position > r->length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if ((size_t)count > (r->length - r->position) / min_item_size) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_auth_token_from_request(const opcua_byte_t *request_body, size_t request_length,
                                                       opcua_uint32_t *auth_token) {
    mu_binary_reader_t reader;
    mu_nodeid_t token;
    opcua_statuscode_t s;

    *auth_token = 0u;
    mu_binary_reader_init(&reader, request_body, request_length);
    s = mu_binary_read_nodeid(&reader, &token);
    if (s != MU_STATUS_GOOD) {
        return MU_STATUS_GOOD;
    }
    /* OPC-10000-6 §5.2.2.9 encodes NodeId identifiers as a discriminated union;
       OPC-10000-4 §7.38.2 defines authenticationToken as a NodeId. A token
       outside this server's ns=0 numeric token space is a non-match, not a
       malformed service body for the session-state gate. */
    if (token.identifier_type != MU_NODEID_NUMERIC || token.namespace_index != 0u) {
        return MU_STATUS_GOOD;
    }

    *auth_token = token.identifier.numeric;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_service_fault(opcua_byte_t *buffer, size_t *length, opcua_uint32_t request_handle,
                                          opcua_statuscode_t service_result) {
    if (!buffer || !length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, *length);

    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_SERVICEFAULT}};
    opcua_statuscode_t s = mu_binary_write_nodeid(&w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    s = mu_response_header_encode(&w, &rh);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *length = w.position;
    return MU_STATUS_GOOD;
}

/* OpenSecureChannel (OPC 10000-4 5.6.2.2): decode the request, open/renew the
   channel, and encode OpenSecureChannelResponse (ChannelSecurityToken + nonce). */
static opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t client_proto, request_type, security_mode, requested_lifetime;
    mu_bytestring_t client_nonce;
    mu_binary_read_uint32(r, &client_proto);
    mu_binary_read_uint32(r, &request_type);
    mu_binary_read_uint32(r, &security_mode);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    /* OPC-10000-4 §5.6.2.3 Table 12: requestType shall be ISSUE (0) or
       RENEW (1). Any other value is rejected with Bad_RequestTypeInvalid. */
    if (request_type > 1) {
        return MU_STATUS_BAD_REQUESTTYPEINVALID;
    }
    s = mu_binary_read_bytestring(r, &client_nonce);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    /* OPC-10000-4 section 5.6.2.3 Table 12: a Server shall check the minimum
        length of the Client nonce and return Bad_NonceInvalid if the length is
        below 32 bytes. The spec also bounds the upper end at 128 bytes (per
        OPC-10000-4 section 5.7.2.2 / OPC-10000-7 SecureChannelNonceLength). A
        null nonce (length == -1) or empty nonce (length == 0) is allowed; this
        sentinel is used with the None SecurityPolicy where no nonce is
        required. asyncua sends an empty (length 0) ByteString for None. */
    if (client_nonce.length > 0 && (client_nonce.length < 32 || client_nonce.length > 128)) {
        return MU_STATUS_BAD_NONCEINVALID;
    }
    mu_binary_read_uint32(r, &requested_lifetime);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    opcua_uint32_t revised = 0;
    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];

    /* OPC-10000-4 sections 5.6.2.3 and 7.38.2: OPN nonce generation is a
       security check; fail closed instead of opening a channel with weak nonce. */
    if (server->config.entropy_adapter.generate_random == NULL ||
        server->config.entropy_adapter.generate_random(server->config.entropy_adapter.context, nonce_buf,
                                                       sizeof(nonce_buf)) != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    s = mu_secure_channel_open(&server_secure_channel, current_opn_security_policy(server),
                               (mu_message_security_mode_t)security_mode, requested_lifetime,
                               &server->config.entropy_adapter, &revised);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

#ifdef MUC_OPCUA_SECURITY
    /* The channel opened as a supported policy; now enforce application
       authentication. This runs AFTER mu_secure_channel_open so an unsupported
       policy or a missing crypto adapter is still reported as
       Bad_SecurityPolicyRejected rather than being masked by a trust failure. */
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID) {
        /* OPC-10000-4 §5.5 / §6.1.3: for any non-None policy, application
           authentication is mandatory. Fail closed when no trust list is
           configured rather than silently accepting the certificate — unless the
           operator has explicitly opted into accepting untrusted clients (demos/
           interop only). Certificate validity was already enforced during the OPN
           asymmetric unwrap regardless of this flag. */
        if (!server->config.allow_untrusted_clients) {
            if (server->config.trust_list == NULL) {
                return MU_STATUS_BAD_SECURITYCHECKSFAILED;
            }
            const mu_bytestring_t *client_cert = current_opn_client_cert(server);
            if (client_cert == NULL || client_cert->length <= 0) {
                return MU_STATUS_BAD_SECURITYCHECKSFAILED;
            }
            if (mu_trust_list_match(server->config.trust_list, client_cert->data, (size_t)client_cert->length) !=
                MU_STATUS_GOOD) {
                return MU_STATUS_BAD_SECURITYCHECKSFAILED;
            }
        }
    }
#endif

    s = write_response_prefix(w, MU_ID_OPENSECURECHANNELRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_datetime_t now = server->config.time_adapter.get_time
                               ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                               : 0;

    mu_binary_write_uint32(w, MU_OPC_UA_TCP_PROTOCOL_VERSION);   /* ServerProtocolVersion (OPC-10000-6 §7.1.2.2) */
    mu_binary_write_uint32(w, server_secure_channel.channel_id); /* SecurityToken.ChannelId */
    mu_binary_write_uint32(w, server_secure_channel.token_id);   /* SecurityToken.TokenId */
    mu_binary_write_int64(w, now);                               /* SecurityToken.CreatedAt */
    mu_binary_write_uint32(w, revised);                          /* SecurityToken.RevisedLifetime */
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }

    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};
    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* Record the negotiated MessageSecurityMode and, for a secured channel,
       derive the symmetric key sets from the nonces (OPC 10000-6 6.7.5). */
    server_secure_channel.mode = (mu_message_security_mode_t)security_mode;
#ifdef MUC_OPCUA_SECURITY
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID && server->config.crypto_adapter != NULL) {
        const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
        size_t cn_len = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
        if (cn_len == 0) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        /* Inbound (client->server) keys use ServerNonce as secret; outbound the reverse. */
        s = mu_sym_keys_derive(cr, server_secure_channel.policy, nonce_buf, sizeof(nonce_buf), client_nonce.data,
                               cn_len, &server_secure_channel.client_keys);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_sym_keys_derive(cr, server_secure_channel.policy, client_nonce.data, cn_len, nonce_buf,
                               sizeof(nonce_buf), &server_secure_channel.server_keys);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        server_secure_channel.keys_valid = true;
        mu_sym_keys_prepare_cipher(&server_secure_channel.client_keys, cr);
        mu_sym_keys_prepare_cipher(&server_secure_channel.server_keys, cr);
    }
#else
    (void)client_nonce;
#endif

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* CloseSecureChannel (OPC-10000-4 §5.6.3.2 Table 13): decode the request
   header, close the active SecureChannel, and encode
   CloseSecureChannelResponse. The response carries only a ResponseHeader — no
   body fields — per Table 13. */
opcua_statuscode_t handle_close_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_secure_channel_close(&server_secure_channel);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* OPC-10000-4 §5.6.3.2 Table 13: CloseSecureChannelResponse has only a
       ResponseHeader; there are no body fields to encode. */
    s = write_response_prefix(w, MU_ID_CLOSESECURECHANNELRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* Discovery service handlers (handle_get_endpoints, handle_find_servers and
   their static helpers) moved to dispatch_discovery.c (T009). Extern
   prototypes live in server_internal.h under MUC_OPCUA_SERVICE_DISCOVERY;
   the dispatch table above references them by name. */

/* Per-request operation bounds (bounded, stack-allocated). Sized so a standards
   client's connect-time batch reads (e.g. the .NET stack reading the ~12
   ServerCapabilities/OperationLimits properties at once) are accepted rather than
   rejected with Bad_TooManyOperations; missing nodes return per-node
   Bad_NodeIdUnknown, which clients tolerate. MU_DISPATCH_MAX_READ_NODES moved
   to dispatch_attribute.c (T010) where the Read/Write handlers live.
   MU_DISPATCH_MAX_BROWSE_* and MU_DISPATCH_MAX_TRANSLATE_* moved to
   dispatch_view.c (T011) where the Browse-family handlers live. */
#define MU_DISPATCH_MAX_REGISTER_NODES 32
#define MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS 32
/* MU_DISPATCH_MAX_CALL_METHODS and MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS moved
   to dispatch_method.c (T014); the only users are handle_call and its helpers
   there. */

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
#ifndef MU_ID_SETTRIGGERINGREQUEST
#define MU_ID_SETTRIGGERINGREQUEST 773
#endif
#ifndef MU_ID_SETTRIGGERINGRESPONSE
#define MU_ID_SETTRIGGERINGRESPONSE 776
#endif
#endif

#if MUC_OPCUA_SUBSCRIPTIONS
/* MU_DISPATCH_MAX_PUBLISH_ACKS moved to dispatch_subscription.c (T012); the only
   remaining user is handle_publish in that module. */
#define MU_DOUBLE_SIGN_BIT 0x8000000000000000ULL
#define MU_PUBLISHING_INTERVAL_MIN_BITS 0x4049000000000000ULL /* 50.0 */
#define MU_PUBLISHING_INTERVAL_MAX_BITS 0x40ed4c0000000000ULL /* 60000.0 */
#define MU_MONITORED_VALUE_ATTRIBUTE_ID 13u
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
/* OPC-10000-4 section 7.22.2 DataChangeFilter binary encoding NodeId. */
#define MU_ID_DATACHANGEFILTER_ENCODING_DEFAULTBINARY 724u
#ifdef MUC_OPCUA_EVENTS
#define MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY 727u
#endif
#ifndef MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED
/* OPC-10000-4 sections 5.13.2.4 and 7.38.2. */
#define MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED ((opcua_statuscode_t)0x80440000)
#endif
#ifndef MU_STATUS_BAD_MONITOREDITEMFILTERINVALID
/* OPC-10000-4 sections 5.13.2.4 and 7.38.2. */
#define MU_STATUS_BAD_MONITOREDITEMFILTERINVALID ((opcua_statuscode_t)0x80430000)
#endif
#endif

#if MU_DISPATCH_CALL_ENABLED
/* MU_ID_SERVER_OBJECT, MU_ID_SERVER_GETMONITOREDITEMS and MU_ID_SERVER_RESENDDATA
   moved to dispatch_method.c (T014); the only user is handle_call there. */
#endif

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    opcua_uint32_t monitoring_mode;
    opcua_uint32_t client_handle;
    opcua_uint64_t sampling_interval_bits;
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
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
#ifdef MUC_OPCUA_EVENTS
    opcua_byte_t select_clauses[8];
    opcua_byte_t select_clauses_count;
#endif
} mu_monitored_item_create_body_t;

typedef opcua_statuscode_t (*mu_subscription_id_result_fn)(mu_server_t *server, opcua_uint32_t id, void *context);

/* mu_set_publishing_mode_context_t moved to dispatch_subscription.c (T012); its
   only users (handle_set_publishing_mode + set_publishing_mode_result) moved. */

typedef struct {
    opcua_uint32_t subscription_id;
    opcua_uint32_t monitoring_mode;
} mu_set_monitoring_mode_context_t;

typedef struct {
    opcua_uint32_t subscription_id;
} mu_monitored_item_id_context_t;

/* publishing_interval_bits_to_ms / publishing_interval_ms_to_bits: defined here
   (non-static extern, declared in server_internal.h) because they are shared
   with the MonitoredItems handlers below and with dispatch_subscription.c (T012). */
opcua_uint32_t publishing_interval_bits_to_ms(opcua_uint64_t bits) {
    if ((bits & MU_DOUBLE_SIGN_BIT) != 0u || bits < MU_PUBLISHING_INTERVAL_MIN_BITS) {
        return MU_MIN_PUBLISHING_INTERVAL_MS;
    }
    if (bits > MU_PUBLISHING_INTERVAL_MAX_BITS) {
        return MU_MAX_PUBLISHING_INTERVAL_MS;
    }

    opcua_uint32_t exponent = (opcua_uint32_t)((bits >> 52) & 0x7FFu);
    opcua_uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;
    opcua_uint64_t mantissa = (1ULL << 52) | fraction;
    opcua_uint32_t unbiased = exponent - 1023u;

    if (unbiased >= 52u) {
        return (opcua_uint32_t)(mantissa << (unbiased - 52u));
    }
    return (opcua_uint32_t)(mantissa >> (52u - unbiased));
}

opcua_uint64_t publishing_interval_ms_to_bits(opcua_uint32_t interval_ms) {
    opcua_uint32_t value = interval_ms;
    opcua_uint32_t exponent = 0u;
    while ((value >> 1u) != 0u) {
        value >>= 1u;
        ++exponent;
    }

    opcua_uint64_t leading = (opcua_uint64_t)1u << exponent;
    opcua_uint64_t fraction = ((opcua_uint64_t)interval_ms - leading) << (52u - exponent);
    return ((opcua_uint64_t)(exponent + 1023u) << 52) | fraction;
}

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static bool is_datachange_filter_binary_type(const mu_nodeid_t *type_id) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == MU_ID_DATACHANGEFILTER_ENCODING_DEFAULTBINARY;
}

static bool is_null_extension_object_type(const mu_nodeid_t *type_id, size_t length) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == 0u && length == 0u;
}

static bool is_known_monitoring_filter_binary_type(const mu_nodeid_t *type_id, size_t length) {
    if (is_null_extension_object_type(type_id, length)) {
        return true;
    }
    if (is_datachange_filter_binary_type(type_id)) {
        return true;
    }
    if (type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
        type_id->identifier.numeric == MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY) {
        return true;
    }
#ifdef MUC_OPCUA_EVENTS
    if (type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
        type_id->identifier.numeric == MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY) {
        return true;
    }
#endif
    return false;
}

static bool is_numeric_variant_type(mu_builtin_type_t type) {
    switch (type) {
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
        return true;
    default:
        return false;
    }
}

static bool monitored_node_has_numeric_static_value(const mu_node_t *node) {
    if (node == NULL || node->value == NULL) {
        return false;
    }
    if (node->value->type != MU_VALUESOURCE_STATIC) {
        /* Callback value types are not exposed at create time; accept and let
           sampling evaluate the value stream. */
        return true;
    }
    if (node->value->data.static_value.is_array) {
        return false;
    }
    return is_numeric_variant_type(node->value->data.static_value.type);
}

static void set_datachange_trigger_from_wire(mu_monitored_item_create_body_t *body, opcua_uint32_t trigger) {
    switch (trigger) {
    case 0u:
        body->trigger = MU_DATACHANGE_TRIGGER_STATUS;
        break;
    case 1u:
        body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        break;
    case 2u:
        body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE_TIMESTAMP;
        break;
    default:
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERINVALID;
        break;
    }
}

static opcua_statuscode_t read_datachange_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                                      mu_monitored_item_create_body_t *body) {
    opcua_uint32_t trigger;
    opcua_uint32_t deadband_type;
    opcua_double_t deadband_value;

    if (filter_length == 0u) {
        return MU_STATUS_GOOD;
    }
    if (filter_length != 16u) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (r->position > r->length || filter_length > r->length - r->position) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    /* OPC-10000-4 section 7.22.2: DataChangeFilter is trigger, deadbandType,
       deadbandValue in the ExtensionObject body. */
    mu_binary_read_uint32(r, &trigger);
    mu_binary_read_uint32(r, &deadband_type);
    mu_binary_read_double(r, &deadband_value);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    set_datachange_trigger_from_wire(body, trigger);
    switch (deadband_type) {
    case 0u:
        body->deadband_type = MU_DEADBAND_TYPE_NONE;
        body->deadband_value = 0.0;
        break;
    case 1u:
        body->deadband_type = MU_DEADBAND_TYPE_ABSOLUTE;
        body->deadband_value = deadband_value;
        break;
    case 2u:
        body->deadband_type = MU_DEADBAND_TYPE_PERCENT;
        body->deadband_value = deadband_value;
        if (body->filter_result == MU_STATUS_GOOD) {
            body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        }
        break;
    default:
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERINVALID;
        break;
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_aggregate_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                                     mu_monitored_item_create_body_t *body) {
    opcua_datetime_t start_time;
    mu_nodeid_t aggregate_type;
    opcua_double_t processing_interval;
    opcua_boolean_t use_defaults;
    opcua_boolean_t treat_uncertain;
    opcua_byte_t percent_bad;
    opcua_byte_t percent_good;
    opcua_boolean_t sloped_extrap;

    if (filter_length == 0u) {
        body->filter_result = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_GOOD;
    }
    if (r->position > r->length || filter_length > r->length - r->position) {
        body->filter_result = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }

    opcua_statuscode_t s = mu_binary_read_int64(r, (opcua_int64_t *)&start_time);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_read_nodeid(r, &aggregate_type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_read_double(r, &processing_interval);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_binary_read_boolean(r, &use_defaults);
    mu_binary_read_boolean(r, &treat_uncertain);
    mu_binary_read_byte(r, &percent_bad);
    mu_binary_read_byte(r, &percent_good);
    mu_binary_read_boolean(r, &sloped_extrap);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    body->has_aggregate = true;
    body->processing_interval = processing_interval;

    if (aggregate_type.identifier_type == MU_NODEID_NUMERIC && aggregate_type.namespace_index == 0u) {
        body->aggregate_type = aggregate_type.identifier.numeric;
    } else {
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        return MU_STATUS_GOOD;
    }

    /* OPC-10000-4 section 7.22.4 carries an AggregateFunction NodeId. This scoped
       subscription implementation supports only the OPC-10000-13 section 5.4.3.5
       Average, section 5.4.3.10 Minimum, and section 5.4.3.11 Maximum functions. */
    if (body->aggregate_type != MU_ID_AGGREGATETYPE_AVERAGE && body->aggregate_type != MU_ID_AGGREGATETYPE_MINIMUM &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_MAXIMUM) {
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        return MU_STATUS_GOOD;
    }

    /* OPC-10000-4 sections 5.13.2.4 and 5.13.3.4: invalid filter parameters are
       reported as per-item filter StatusCodes, not as a service failure. */
    if (processing_interval <= 0.0) {
        body->filter_result = MU_STATUS_BAD_FILTERNOTALLOWED;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_GOOD;
}
#endif

#ifdef MUC_OPCUA_EVENTS
static opcua_statuscode_t read_event_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                                 mu_monitored_item_create_body_t *body) {
    (void)filter_length;
    body->select_clauses_count = 0;
    memset(body->select_clauses, 0, sizeof(body->select_clauses));
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    body->filter_result = MU_STATUS_GOOD;
#endif

    opcua_int32_t select_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &select_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (select_count < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    for (opcua_int32_t i = 0; i < select_count; ++i) {
        mu_nodeid_t type_id;
        s = mu_binary_read_nodeid(r, &type_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        opcua_int32_t bp_count;
        s = mu_binary_read_int32(r, &bp_count);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (bp_count < 0) {
            return MU_STATUS_BAD_DECODINGERROR;
        }

        mu_string_t last_name = {-1, NULL};
        for (opcua_int32_t j = 0; j < bp_count; ++j) {
            opcua_uint16_t ns;
            mu_string_t name;
            mu_binary_read_uint16(r, &ns);
            s = mu_binary_read_string(r, &name);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            last_name = name;
        }

        opcua_uint32_t attr_id;
        mu_binary_read_uint32(r, &attr_id);
        mu_string_t idx_range;
        s = mu_binary_read_string(r, &idx_range);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        opcua_byte_t field_type = 0;
        if (last_name.length > 0 && last_name.data != NULL) {
            if (last_name.length == 7 && memcmp(last_name.data, "EventId", 7) == 0) {
                field_type = 1;
            } else if (last_name.length == 9 && memcmp(last_name.data, "EventType", 9) == 0) {
                field_type = 2;
            } else if (last_name.length == 4 && memcmp(last_name.data, "Time", 4) == 0) {
                field_type = 3;
            } else if (last_name.length == 7 && memcmp(last_name.data, "Message", 7) == 0) {
                field_type = 4;
            } else if (last_name.length == 8 && memcmp(last_name.data, "Severity", 8) == 0) {
                field_type = 5;
            }
        }

        if (i < 8) {
            body->select_clauses[i] = field_type;
            body->select_clauses_count++;
        }
    }

    opcua_int32_t element_count;
    s = mu_binary_read_int32(r, &element_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (element_count < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    for (opcua_int32_t i = 0; i < element_count; ++i) {
        opcua_uint32_t op_type;
        mu_binary_read_uint32(r, &op_type);
        opcua_int32_t arg_count;
        mu_binary_read_int32(r, &arg_count);
        if (arg_count < 0) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        for (opcua_int32_t j = 0; j < arg_count; ++j) {
            mu_nodeid_t ext_id;
            size_t ext_len;
            s = mu_binary_read_extension_object_header(r, &ext_id, &ext_len);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            s = skip_extension_object_body(r, ext_len);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
    }
    return r->status;
}
#endif

static opcua_statuscode_t read_monitored_item_create_body(mu_binary_reader_t *r,
                                                          mu_monitored_item_create_body_t *body) {
    opcua_statuscode_t s;
    mu_string_t index_range;
    opcua_uint16_t data_encoding_namespace_index;
    mu_string_t data_encoding_name;
    mu_nodeid_t filter_type;
    size_t filter_length;

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    body->deadband_type = MU_DEADBAND_TYPE_NONE;
    body->deadband_value = 0.0;
    body->filter_result = MU_STATUS_GOOD;
    body->has_datachange_filter = false;
    body->has_aggregate = false;
    body->aggregate_type = 0u;
    body->processing_interval = 0.0;
#endif

    s = mu_binary_read_nodeid(r, &body->node_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint32(r, &body->attribute_id);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    s = mu_binary_read_string(r, &index_range);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint16(r, &data_encoding_namespace_index);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    s = mu_binary_read_string(r, &data_encoding_name);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint32(r, &body->monitoring_mode);
    mu_binary_read_uint32(r, &body->client_handle);
    mu_binary_read_uint64(r, &body->sampling_interval_bits);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    const size_t filter_body_start = r->position;
    s = ensure_reader_bytes_remaining(r, filter_length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (is_datachange_filter_binary_type(&filter_type)) {
        body->has_datachange_filter = true;
        s = read_datachange_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
    } else if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
               filter_type.identifier.numeric == MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY) {
        s = read_aggregate_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
#ifdef MUC_OPCUA_EVENTS
    } else if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
               filter_type.identifier.numeric == MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY) {
        s = read_event_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
#endif
    } else if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
               filter_type.identifier.numeric == 0u && filter_length == 0u) {
        /* Null ExtensionObject: no MonitoringFilter requested. */
    } else {
        /* OPC-10000-4 section 5.13.2.4: syntactically valid but unsupported
           MonitoringFilter types are reported as per-item filter results. */
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
#else
#ifdef MUC_OPCUA_EVENTS
    if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
        filter_type.identifier.numeric == MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY) {
        s = read_event_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD)
            return s;
    } else {
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD)
            return s;
    }
#else
    s = skip_extension_object_body(r, filter_length);
    if (s != MU_STATUS_GOOD)
        return s;
#endif
#endif
    mu_binary_read_uint32(r, &body->queue_size);
    mu_binary_read_boolean(r, &body->discard_oldest);
    return r->status;
}

static void copy_monitored_node_id(mu_monitored_item_t *item, const mu_nodeid_t *node_id) {
    item->node_id = *node_id;
    if (node_id->identifier_type == MU_NODEID_STRING) {
        opcua_int32_t length = node_id->identifier.string.length;
        if (length > (opcua_int32_t)sizeof(item->node_id_string)) {
            length = (opcua_int32_t)sizeof(item->node_id_string);
        }
        if (length > 0 && node_id->identifier.string.data != NULL) {
            memcpy(item->node_id_string, node_id->identifier.string.data, (size_t)length);
            item->node_id.identifier.string.data = item->node_id_string;
        } else {
            item->node_id.identifier.string.data = NULL;
        }
        item->node_id.identifier.string.length = length;
    }
}

static opcua_statuscode_t write_null_extension_object(mu_binary_writer_t *w) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    return mu_binary_write_extension_object_header(w, &null_id, 0);
}

static opcua_statuscode_t write_monitored_item_create_result(mu_binary_writer_t *w, opcua_statuscode_t result,
                                                             opcua_uint32_t monitored_item_id,
                                                             opcua_uint32_t sampling_interval_ms,
                                                             opcua_uint32_t revised_queue_size) {
    opcua_uint64_t sampling_bits = 0u;
    mu_binary_write_statuscode(w, result);
    mu_binary_write_uint32(w, monitored_item_id);
    if (result == MU_STATUS_GOOD) {
        sampling_bits = publishing_interval_ms_to_bits(sampling_interval_ms);
    }
    mu_binary_write_uint64(w, sampling_bits);
    mu_binary_write_uint32(w, revised_queue_size);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }
    return write_null_extension_object(w);
}

static mu_monitored_item_t *find_monitored_item(mu_server_t *server, opcua_uint32_t subscription_id,
                                                opcua_uint32_t monitored_item_id) {
    size_t active_checked = 0;
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS && active_checked < server->subs.active_monitored_items_count; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!item->in_use) {
            continue;
        }
        active_checked++;
        if (item->subscription_id == subscription_id && item->monitored_item_id == monitored_item_id) {
            return item;
        }
    }

    return NULL;
}

/* drive_subscription_id_status_array: non-static extern (declared in
   server_internal.h). Shared by the MonitoredItems handlers below and by the
   subscription handlers in dispatch_subscription.c (T012). */
opcua_statuscode_t drive_subscription_id_status_array(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                      size_t *response_length, opcua_uint32_t response_type_id,
                                                      opcua_uint32_t request_handle, bool validate_subscription_id,
                                                      opcua_uint32_t subscription_id,
                                                      mu_subscription_id_result_fn item_result, void *context) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (count > 0 && (size_t)count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    if (validate_subscription_id) {
        mu_subscription_t *sub =
            mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
        if (sub == NULL) {
            return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
        }
    }

    if (count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, response_type_id, request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t id;
        s = mu_binary_read_uint32(r, &id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_write_statuscode(w, item_result(server, id, context));
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t set_monitoring_mode_result(mu_server_t *server, opcua_uint32_t monitored_item_id,
                                                     void *context) {
    const mu_set_monitoring_mode_context_t *mode = (const mu_set_monitoring_mode_context_t *)context;
    mu_monitored_item_t *item = find_monitored_item(server, mode->subscription_id, monitored_item_id);
    if (item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    const opcua_byte_t previous_mode = item->monitoring_mode;
    item->monitoring_mode = (opcua_byte_t)mode->monitoring_mode;

    /* OPC-10000-4 §5.13.1.3: when a MonitoredItem is enabled (MonitoringMode
       changes from DISABLED to SAMPLING or REPORTING), the Server shall take
       the first sample as soon as possible, and the time of this sample
       becomes the starting point for the next sampling interval. This mirrors
       the immediate baseline sample taken at MonitoredItem creation. Event
       (EventNotifier, attribute 12) items have no Value to sample. */
    if (previous_mode == MU_MONITORING_MODE_DISABLED &&
        (item->monitoring_mode == MU_MONITORING_MODE_SAMPLING ||
         item->monitoring_mode == MU_MONITORING_MODE_REPORTING) &&
        item->attribute_id != 12u) {
        mu_variant_t cur;
        opcua_statuscode_t read_status = read_monitored_item_value(item, &cur);
        if (read_status == MU_STATUS_GOOD) {
            item->last_value = cur;
            item->last_status = MU_STATUS_GOOD;
            item->has_value = true;
            item->pending = true;
            opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
            item->next_sample_ms = now_ms + item->sampling_interval_ms;
        } else {
            item->last_status = read_status;
        }
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t delete_monitored_item_result(mu_server_t *server, opcua_uint32_t monitored_item_id,
                                                       void *context) {
    const mu_monitored_item_id_context_t *item_context = (const mu_monitored_item_id_context_t *)context;
    return mu_monitored_item_delete(&server->subs, item_context->subscription_id, monitored_item_id);
}

/* CreateSubscription, ModifySubscription, SetPublishingMode, DeleteSubscriptions,
   Publish and Republish handlers + their exclusive helpers
   (set_publishing_mode_result, delete_subscription_result) moved to
   dispatch_subscription.c (T012). Extern prototypes in server_internal.h. */

static opcua_statuscode_t write_create_monitored_items_prefix(mu_binary_writer_t *w, opcua_uint32_t request_handle,
                                                              opcua_int32_t items_to_create) {
    opcua_statuscode_t s = write_response_prefix(w, MU_ID_CREATEMONITOREDITEMSRESPONSE, request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    return mu_binary_write_int32(w, items_to_create);
}

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static opcua_statuscode_t validate_create_item_filter(const mu_monitored_item_create_body_t *body,
                                                      const mu_node_t *node) {
    if (body->filter_result != MU_STATUS_GOOD)
        return body->filter_result;
    if (body->has_datachange_filter && body->attribute_id != MU_MONITORED_VALUE_ATTRIBUTE_ID)
        return MU_STATUS_BAD_FILTERNOTALLOWED;
    if (body->deadband_type == MU_DEADBAND_TYPE_ABSOLUTE && !monitored_node_has_numeric_static_value(node))
        return MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
    if (body->has_aggregate && !monitored_node_has_numeric_static_value(node))
        return MU_STATUS_BAD_FILTERNOTALLOWED;
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t configure_monitored_item(mu_monitored_item_t *item,
                                                   const mu_monitored_item_create_body_t *body, const mu_node_t *node,
                                                   mu_subscription_t *sub, opcua_uint32_t timestamps_to_return,
                                                   opcua_uint64_t now_ms, mu_server_t *server) {
    (void)server; /* used by mu_resolve_eurange_span under MUC_OPCUA_DATA_ACCESS */
    copy_monitored_node_id(item, &body->node_id);
    item->resolved_node = node;
    item->attribute_id = body->attribute_id;
    item->client_handle = body->client_handle;
    item->monitoring_mode = (opcua_byte_t)body->monitoring_mode;
    item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    item->timestamps_to_return = (opcua_byte_t)timestamps_to_return;
    if ((body->sampling_interval_bits & MU_DOUBLE_SIGN_BIT) != 0u) {
        item->sampling_interval_ms = sub->publishing_interval_ms;
    } else {
        item->sampling_interval_ms = publishing_interval_bits_to_ms(body->sampling_interval_bits);
    }
    item->next_sample_ms = now_ms + item->sampling_interval_ms;
    item->has_value = false;
    item->pending = false;
#ifdef MUC_OPCUA_EVENTS
    item->select_clauses_count = body->select_clauses_count;
    memcpy(item->select_clauses, body->select_clauses, sizeof(item->select_clauses));
#endif
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    item->trigger = (opcua_byte_t)body->trigger;
    item->deadband_type = (opcua_byte_t)body->deadband_type;
    item->deadband_value = body->deadband_value;
#if MUC_OPCUA_DATA_ACCESS
    item->deadband_span = 0.0;
    if (item->deadband_type == MU_DEADBAND_TYPE_PERCENT && node != NULL) {
        item->deadband_span = mu_resolve_eurange_span(server, node);
    }
#endif
    item->queue_size = body->queue_size;
    if (item->queue_size == 0u) {
        item->queue_size = 1u;
    }
    if (item->queue_size > MU_MONITORED_QUEUE_DEPTH) {
        item->queue_size = MU_MONITORED_QUEUE_DEPTH;
    }
    item->queue_head = 0u;
    item->queue_tail = 0u;
    item->queue_count = 0u;
    item->discard_oldest = body->discard_oldest;
    item->queue_overflow = false;
    item->has_aggregate = body->has_aggregate;
    if (item->has_aggregate) {
        item->aggregate_state.aggregate_type = body->aggregate_type;
        item->aggregate_state.processing_interval = body->processing_interval;
        item->aggregate_state.last_calculation = (opcua_datetime_t)now_ms;
        item->aggregate_state.sample_count = 0u;
        memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
    }
#endif
    if (body->attribute_id != 12u && node->value != NULL) {
        item->last_status = mu_value_source_read(node->value, &body->node_id, &item->last_value);
        if (item->last_status == MU_STATUS_GOOD) {
            item->has_value = true;
            item->pending = true;
        }
    }
    (void)body->queue_size;
    (void)body->discard_oldest;
    return MU_STATUS_GOOD;
}

/* CreateMonitoredItems (OPC 10000-4 5.13.2): data-change monitoring for Value
   Attributes only, backed by the fixed-size subscription engine. */
static opcua_statuscode_t handle_create_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t subscription_id, timestamps_to_return;
    opcua_int32_t items_to_create;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &timestamps_to_return);
    mu_binary_read_int32(r, &items_to_create);
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    if (timestamps_to_return > MU_TIMESTAMPS_TO_RETURN_NEITHER)
        return MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID;
    if (items_to_create > 0 && (size_t)items_to_create > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS)
        return MU_STATUS_BAD_TOOMANYOPERATIONS;

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL)
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    if (items_to_create <= 0)
        return MU_STATUS_BAD_NOTHINGTODO;

    s = write_create_monitored_items_prefix(w, req.request_handle, items_to_create);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    for (opcua_int32_t i = 0; i < items_to_create; ++i) {
        mu_monitored_item_create_body_t body;
        s = read_monitored_item_create_body(r, &body);
        if (s != MU_STATUS_GOOD)
            return s;

        const mu_node_t *node = mu_resolve_node(server->config.address_space, &server->user_address_space_index,
                                                &server->runtime_base.space, &body.node_id);
        if (node == NULL) {
            s = write_monitored_item_create_result(w, MU_STATUS_BAD_NODEIDUNKNOWN, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        {
            opcua_statuscode_t filter_status = validate_create_item_filter(&body, node);
            if (filter_status != MU_STATUS_GOOD) {
                s = write_monitored_item_create_result(w, filter_status, 0u, 0u, 0u);
                if (s != MU_STATUS_GOOD)
                    return s;
                continue;
            }
        }
#endif

        if (!(body.attribute_id == MU_MONITORED_VALUE_ATTRIBUTE_ID
#ifdef MUC_OPCUA_EVENTS
              || body.attribute_id == 12u
#endif
              )) {
            s = write_monitored_item_create_result(w, MU_STATUS_BAD_ATTRIBUTEIDINVALID, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }

        mu_monitored_item_t *item = NULL;
        opcua_statuscode_t result = mu_monitored_item_alloc(&server->subs, subscription_id, &item);
        if (result != MU_STATUS_GOOD) {
            s = write_monitored_item_create_result(w, result, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }

        configure_monitored_item(item, &body, node, sub, timestamps_to_return, now_ms, server);

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        const opcua_uint32_t revised_queue_size = item->queue_size;
#else
        const opcua_uint32_t revised_queue_size = 1u;
#endif
        s = write_monitored_item_create_result(w, MU_STATUS_GOOD, item->monitored_item_id, item->sampling_interval_ms,
                                               revised_queue_size);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t write_monitored_item_modify_result(mu_binary_writer_t *w, opcua_statuscode_t result,
                                                             opcua_uint32_t revised_sampling_interval_ms,
                                                             opcua_uint32_t revised_queue_size) {
    opcua_uint64_t sampling_bits = 0u;
    mu_binary_write_statuscode(w, result);
    if (result == MU_STATUS_GOOD) {
        sampling_bits = publishing_interval_ms_to_bits(revised_sampling_interval_ms);
    }
    mu_binary_write_uint64(w, sampling_bits);
    mu_binary_write_uint32(w, revised_queue_size);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }
    return write_null_extension_object(w);
}

/* ModifyMonitoredItems (OPC 10000-4 5.13.3): update sampling interval/client handle
   for each MonitoredItem under the session-owned Subscription. */
static opcua_statuscode_t handle_modify_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t subscription_id;
    opcua_uint32_t timestamps_to_return;
    opcua_int32_t count;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &timestamps_to_return);
    mu_binary_read_int32(r, &count);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    (void)timestamps_to_return;

    if (count > 0 && (size_t)count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, MU_ID_MODIFYMONITOREDITEMSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t monitored_item_id;
        opcua_uint32_t client_handle;
        opcua_uint64_t sampling_interval_bits;
        mu_nodeid_t filter_type;
        size_t filter_length;
        opcua_statuscode_t filter_result = MU_STATUS_GOOD;
        bool has_datachange_filter = false;
        bool has_aggregate_filter = false;
        opcua_uint32_t queue_size;
        opcua_boolean_t discard_oldest;
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        mu_monitored_item_create_body_t filter_body;
        filter_body.trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        filter_body.deadband_type = MU_DEADBAND_TYPE_NONE;
        filter_body.deadband_value = 0.0;
        filter_body.filter_result = MU_STATUS_GOOD;
        filter_body.has_datachange_filter = false;
        filter_body.has_aggregate = false;
        filter_body.aggregate_type = 0u;
        filter_body.processing_interval = 0.0;
#endif

        mu_binary_read_uint32(r, &monitored_item_id);
        mu_binary_read_uint32(r, &client_handle);
        mu_binary_read_uint64(r, &sampling_interval_bits);
        if (r->status != MU_STATUS_GOOD) {
            return r->status;
        }
        s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        /* OPC-10000-4 section 5.13.3.4: MonitoringFilter body must be complete. */
        s = ensure_reader_bytes_remaining(r, filter_length);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        if (is_datachange_filter_binary_type(&filter_type)) {
            const size_t filter_body_start = r->position;

            has_datachange_filter = true;
            filter_body.has_datachange_filter = true;

            /* OPC-10000-4 sections 5.13.3.4 and 7.22.2: invalid
               DataChangeFilter enum fields are per-item filter results. */
            s = read_datachange_filter_body(r, filter_length, &filter_body);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            if (r->position != filter_body_start + filter_length) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            filter_result = filter_body.filter_result;
        } else if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
                   filter_type.identifier.numeric == MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY) {
            const size_t filter_body_start = r->position;

            has_aggregate_filter = true;
            s = read_aggregate_filter_body(r, filter_length, &filter_body);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            if (r->position != filter_body_start + filter_length) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            filter_result = filter_body.filter_result;
        } else if (!is_known_monitoring_filter_binary_type(&filter_type, filter_length)) {
            /* OPC-10000-4 section 5.13.3.4: valid but unsupported
               MonitoringFilter types are per-item results. */
            filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
            s = skip_extension_object_body(r, filter_length);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        } else {
            s = skip_extension_object_body(r, filter_length);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
#else
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD)
            return s;
#endif
        mu_binary_read_uint32(r, &queue_size);
        mu_binary_read_boolean(r, &discard_oldest);
        if (r->status != MU_STATUS_GOOD) {
            return r->status;
        }
        (void)filter_type;
        (void)queue_size;
        (void)discard_oldest;

        mu_monitored_item_t *item = find_monitored_item(server, subscription_id, monitored_item_id);
        opcua_statuscode_t result = MU_STATUS_BAD_MONITOREDITEMIDINVALID;
        opcua_uint32_t revised_sampling_interval_ms = 0u;
        opcua_uint32_t revised_queue_size = 1u;
        if (item != NULL) {
            if (filter_result != MU_STATUS_GOOD) {
                result = filter_result;
            } else if (has_datachange_filter && item->attribute_id != MU_MONITORED_VALUE_ATTRIBUTE_ID) {
                /* OPC-10000-4 section 5.13.3.4: valid MonitoringFilters can
                   still be disallowed for the target MonitoredItem. */
                result = MU_STATUS_BAD_FILTERNOTALLOWED;
            } else if (has_aggregate_filter && item->attribute_id != MU_MONITORED_VALUE_ATTRIBUTE_ID) {
                result = MU_STATUS_BAD_FILTERNOTALLOWED;
            } else {
                revised_sampling_interval_ms = publishing_interval_bits_to_ms(sampling_interval_bits);
                item->sampling_interval_ms = revised_sampling_interval_ms;
                item->client_handle = client_handle;
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
                /* OPC-10000-4 §5.13.3.2 Table 67: the Server shall accept
                   and revise queueSize and discardOldest on modify. Clamp the
                   requested queue to [1, MU_MONITORED_QUEUE_DEPTH] mirroring
                   CreateMonitoredItems, and report the revised value. */
                item->queue_size = queue_size;
                if (item->queue_size == 0u) {
                    item->queue_size = 1u;
                }
                if (item->queue_size > MU_MONITORED_QUEUE_DEPTH) {
                    item->queue_size = MU_MONITORED_QUEUE_DEPTH;
                }
                item->discard_oldest = discard_oldest;
                revised_queue_size = item->queue_size;
                /* OPC-10000-4 §5.13.3.1/§5.13.3.2: changes to MonitoredItem
                   settings (including filters) shall be applied immediately.
                   Apply the decoded DataChangeFilter and AggregateFilter
                   fields to the item, mirroring CreateMonitoredItems. */
                if (has_datachange_filter) {
                    item->trigger = (opcua_byte_t)filter_body.trigger;
                    item->deadband_type = (opcua_byte_t)filter_body.deadband_type;
                    item->deadband_value = filter_body.deadband_value;
                }
                if (has_aggregate_filter) {
                    item->has_aggregate = true;
                    item->aggregate_state.aggregate_type = filter_body.aggregate_type;
                    item->aggregate_state.processing_interval = filter_body.processing_interval;
                    item->aggregate_state.sample_count = 0u;
                    memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
                    opcua_uint64_t now_ms =
                        server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
                    item->aggregate_state.last_calculation = (opcua_datetime_t)now_ms;
                }
#endif
                result = MU_STATUS_GOOD;
            }
        }

        s = write_monitored_item_modify_result(w, result, revised_sampling_interval_ms, revised_queue_size);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* SetMonitoringMode (OPC 10000-4 5.13.4): update MonitoringMode per MonitoredItem id
   and report per-id StatusCode results. */
static opcua_statuscode_t handle_set_monitoring_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_set_monitoring_mode_context_t context = {0u, 0u};
    mu_binary_read_uint32(r, &context.subscription_id);
    mu_binary_read_uint32(r, &context.monitoring_mode);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_SETMONITORINGMODERESPONSE,
                                              req.request_handle, true, context.subscription_id,
                                              set_monitoring_mode_result, &context);
}

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static opcua_statuscode_t read_set_triggering_uint32_array(mu_binary_reader_t *r, opcua_uint32_t *items,
                                                           opcua_int32_t *count) {
    opcua_int32_t decoded_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &decoded_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (decoded_count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (decoded_count > 0 && (size_t)decoded_count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = ensure_array_items_min_remaining(r, decoded_count, 4u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (decoded_count <= 0) {
        *count = 0;
        return MU_STATUS_GOOD;
    }

    for (opcua_int32_t i = 0; i < decoded_count; ++i) {
        s = mu_binary_read_uint32(r, &items[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *count = decoded_count;
    return MU_STATUS_GOOD;
}

/* SetTriggering (OPC-10000-4 5.13.5): add/remove triggering links for one
   triggering MonitoredItem and report per-link StatusCode results. */
static opcua_statuscode_t handle_set_triggering(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t subscription_id;
    opcua_uint32_t triggering_item_id;
    opcua_uint32_t links_to_add[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_uint32_t links_to_remove[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_statuscode_t add_results[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_statuscode_t remove_results[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_int32_t add_count = 0;
    opcua_int32_t remove_count = 0;

    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &triggering_item_id);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    s = read_set_triggering_uint32_array(r, links_to_add, &add_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_set_triggering_uint32_array(r, links_to_remove, &remove_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if ((size_t)add_count + (size_t)remove_count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (add_count == 0 && remove_count == 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    for (opcua_int32_t i = 0; i < add_count; ++i) {
        add_results[i] =
            mu_monitored_item_add_trigger_link(&server->subs, subscription_id, triggering_item_id, links_to_add[i]);
    }
    for (opcua_int32_t i = 0; i < remove_count; ++i) {
        remove_results[i] = mu_monitored_item_remove_trigger_link(&server->subs, subscription_id, triggering_item_id,
                                                                  links_to_remove[i]);
    }

    s = write_response_prefix(w, MU_ID_SETTRIGGERINGRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, add_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < add_count; ++i) {
        s = mu_binary_write_statuscode(w, add_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, remove_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < remove_count; ++i) {
        s = mu_binary_write_statuscode(w, remove_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif

/* DeleteMonitoredItems (OPC 10000-4 5.13.6): service-level Good with per-id
   StatusCode results for the subscription-owned MonitoredItems. */
static opcua_statuscode_t handle_delete_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_monitored_item_id_context_t context = {0u};
    mu_binary_read_uint32(r, &context.subscription_id);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_DELETEMONITOREDITEMSRESPONSE,
                                              req.request_handle, true, context.subscription_id,
                                              delete_monitored_item_result, &context);
}

/* DeleteSubscriptions, Publish and Republish handlers moved to
   dispatch_subscription.c (T012). Extern prototypes in server_internal.h. */

/* handle_call and its static helpers (nodeid_is_ns0_numeric,
   write_call_method_result, read_call_input_arguments,
   write_get_monitored_items_result, write_resend_data_result,
   write_single_call_method_result) moved to dispatch_method.c (T014).
   Extern prototype in server_internal.h; the dispatch table above
   references handle_call by name. */
#endif /* MUC_OPCUA_SUBSCRIPTIONS */

#ifdef MUC_OPCUA_SERVICE_REGISTER_NODES
/* RegisterNodes (OPC 10000-4 5.9.5): this server has no alternate optimized
   handles, so registeredNodeIds is the identity mapping of nodesToRegister. */
static opcua_statuscode_t handle_register_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > MU_DISPATCH_MAX_REGISTER_NODES) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_nodeid_t nodes[MU_DISPATCH_MAX_REGISTER_NODES];
    size_t node_count = (size_t)count;
    for (size_t i = 0; i < node_count; ++i) {
        s = mu_binary_read_nodeid(r, &nodes[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = write_response_prefix(w, MU_ID_REGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (size_t i = 0; i < node_count; ++i) {
        s = mu_binary_write_nodeid(w, &nodes[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* UnregisterNodes (OPC 10000-4 5.9.6): consume the NodeIds and return Good. */
static opcua_statuscode_t handle_unregister_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                  size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > MU_DISPATCH_MAX_REGISTER_NODES) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    size_t node_count = (size_t)count;
    for (size_t i = 0; i < node_count; ++i) {
        mu_nodeid_t ignored;
        s = mu_binary_read_nodeid(r, &ignored);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = write_response_prefix(w, MU_ID_UNREGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SERVICE_REGISTER_NODES */

/* Read and Write attribute handlers (handle_read, handle_write) moved to
   dispatch_attribute.c (T010). Extern prototypes live in server_internal.h
   under MUC_OPCUA_SERVICE_READ / MUC_OPCUA_SERVICE_WRITE; the dispatch table
   above references them by name. */

/* Browse-family handlers (handle_browse, handle_browse_next,
   handle_translate_browse_paths and the static helper browse_name_equals) moved
   to dispatch_view.c (T011). Extern prototypes live in server_internal.h under
   MUC_OPCUA_SERVICE_BROWSE; the dispatch table above references them by name. */

#ifdef MUC_OPCUA_SERVICE_HISTORY
#define MU_MAX_HISTORY_NODES_PER_READ 10
opcua_statuscode_t handle_history_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t s = mu_request_header_decode(r, &req_header);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    // Decode request
    mu_history_read_value_id_t nodes_to_read[MU_MAX_HISTORY_NODES_PER_READ];
    mu_history_read_request_t req;
    s = mu_history_read_request_decode(r, &req, nodes_to_read, MU_MAX_HISTORY_NODES_PER_READ);
    if (s != MU_STATUS_GOOD) {
        s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, s);
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

    if (!server->config.history_adapter.read_raw_modified) {
        s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, MU_STATUS_BAD_NOTSUPPORTED);
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

    // Prepare response
    mu_history_read_result_t results[MU_MAX_HISTORY_NODES_PER_READ];
    mu_datavalue_t dvals[MU_MAX_HISTORY_NODES_PER_READ][10];
    opcua_byte_t continuation_points[MU_MAX_HISTORY_NODES_PER_READ][MU_MAX_HISTORY_READ_CONTINUATION_POINT_LENGTH];
    memset(dvals, 0, sizeof(dvals));
    mu_history_read_response_t resp;
    resp.num_results = req.num_nodes_to_read;
    resp.results = results;

    // We process each node
    for (size_t i = 0; i < req.num_nodes_to_read; i++) {
        mu_history_read_value_id_t *node = &req.nodes_to_read[i];
        mu_history_read_result_t *res = &results[i];

        res->continuation_point.length = -1;
        res->continuation_point.data = NULL;
        res->history_data.num_data_values = 0;
        res->history_data.data_values = NULL;

        size_t cp_out_length = sizeof(continuation_points[i]);
        mu_historical_data_point_t data_points[10];
        memset(data_points, 0, sizeof(data_points));
        size_t actual_data_points = 0;

        res->status_code = server->config.history_adapter.read_raw_modified(
            server->config.history_adapter.context, &node->node_id, req.details.is_read_modified,
            req.details.start_time, req.details.end_time, req.details.num_values_per_node, req.details.return_bounds,
            node->continuation_point.data,
            node->continuation_point.length > 0 ? (size_t)node->continuation_point.length : 0, continuation_points[i],
            &cp_out_length, data_points, 10, &actual_data_points);

        if (res->status_code == MU_STATUS_GOOD) {
            if (cp_out_length > 0 && cp_out_length <= sizeof(continuation_points[i])) {
                res->continuation_point.length = (opcua_int32_t)cp_out_length;
                res->continuation_point.data = continuation_points[i];
            }
            for (size_t j = 0; j < actual_data_points; j++) {
                dvals[i][j].has_value = true;
                dvals[i][j].value = data_points[j].value;
                dvals[i][j].has_source_timestamp = true;
                dvals[i][j].source_timestamp = data_points[j].source_timestamp;
                dvals[i][j].has_server_timestamp = true;
                dvals[i][j].server_timestamp = data_points[j].server_timestamp;
                dvals[i][j].has_status = true;
                dvals[i][j].status = data_points[j].status;
            }
            res->history_data.num_data_values = actual_data_points;
            res->history_data.data_values = dvals[i];
        }
    }

    s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_history_read_response_encode(w, &resp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#define MU_MAX_HISTORY_UPDATE_ITEMS 5
opcua_statuscode_t handle_history_update(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t s = mu_request_header_decode(r, &req_header);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    // Decode request
    mu_history_update_item_t items[MU_MAX_HISTORY_UPDATE_ITEMS];
    mu_history_update_request_t req;
    s = mu_history_update_request_decode(r, &req, items, MU_MAX_HISTORY_UPDATE_ITEMS);
    if (s != MU_STATUS_GOOD) {
        s = write_response_prefix(w, MU_ID_HISTORYUPDATERESPONSE, req_header.request_handle, s);
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

    // Prepare response
    mu_history_update_result_t results[MU_MAX_HISTORY_UPDATE_ITEMS];
    mu_history_update_response_t resp;
    resp.num_results = req.num_items;
    resp.results = results;

    for (size_t i = 0; i < req.num_items; ++i) {
        mu_history_update_item_t *item = &req.items[i];
        mu_history_update_result_t *res = &results[i];
        res->status_code = MU_STATUS_GOOD;
        res->num_operation_results = 0;

        if (item->type == MU_HISTORY_UPDATE_TYPE_DATA) {
            if (!server->config.history_adapter.update_data) {
                res->status_code = MU_STATUS_BAD_NOTSUPPORTED;
                continue;
            }

            res->num_operation_results = item->body.data.num_values;
            res->status_code = server->config.history_adapter.update_data(
                server->config.history_adapter.context, &item->body.data.node_id,
                item->body.data.perform_insert_replace, item->body.data.values, item->body.data.num_values,
                res->operation_results);
        } else if (item->type == MU_HISTORY_UPDATE_TYPE_DELETE) {
            if (!server->config.history_adapter.delete_raw_modified) {
                res->status_code = MU_STATUS_BAD_NOTSUPPORTED;
                continue;
            }

            res->status_code = server->config.history_adapter.delete_raw_modified(
                server->config.history_adapter.context, &item->body.delete_raw.node_id,
                item->body.delete_raw.is_delete_modified, item->body.delete_raw.start_time,
                item->body.delete_raw.end_time);
        } else {
            res->status_code = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
        }
    }

    s = write_response_prefix(w, MU_ID_HISTORYUPDATERESPONSE, req_header.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_history_update_response_encode(w, &resp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SERVICE_HISTORY */

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
/* AddNodes/AddReferences/DeleteNodes/DeleteReferences handlers moved to
   dispatch_node_mgmt.c (T013). */
#endif /* MUC_OPCUA_SERVICE_NODEMANAGEMENT */

opcua_statuscode_t mu_service_dispatch(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *request_body,
                                       size_t request_length, opcua_byte_t *response_body, size_t *response_length) {
    if (!server || !request_body || !response_body || !response_length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    server->active_session = NULL;

    const mu_service_descriptor_t *descriptor = find_service_descriptor(request_id);
    if (descriptor == NULL) {
        return MU_STATUS_BAD_SERVICEUNSUPPORTED;
    }

    if (request_id != MU_ID_OPENSECURECHANNELREQUEST) {
        if (!server_secure_channel.is_open) {
            /* If we haven't even opened a secure channel, we can't process requests over it */
            /* OPC UA Part 4, 7.38.2: Bad_SecureChannelIdInvalid */
            return MU_STATUS_BAD_SECURECHANNELIDINVALID;
        }
    }

    if (descriptor->service.requires_session) {
        opcua_uint32_t auth_token = 0u;
        opcua_statuscode_t s = read_auth_token_from_request(request_body, request_length, &auth_token);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (auth_token == 0u) {
            /* OPC-10000-4 section 7.38.2: an absent/null Session
               authenticationToken is not a known SessionId for this server. */
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
        mu_session_t *session = mu_session_find_by_token(server->sessions, MU_MAX_SESSIONS, auth_token);
        if (session == NULL) {
            /* OPC-10000-4 section 5.7.4.1: a request carrying the token of a
               Session that has been closed (but whose slot has not yet been
               reused) is Bad_SessionClosed, not Bad_SessionIdInvalid. */
            if (mu_session_find_closed_by_token(server->sessions, MU_MAX_SESSIONS, auth_token) != NULL) {
                return MU_STATUS_BAD_SESSIONCLOSED;
            }
            /* OPC-10000-4 section 7.38.2: an unknown Session authenticationToken
               is Bad_SessionIdInvalid, before channel-binding or activation checks. */
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
        /* OPC-10000-4 section 7.38.2: session-bound service use through a
           different SecureChannel is Bad_SecureChannelIdInvalid. */
        s = mu_session_validate_secure_channel(session, server_secure_channel.channel_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (session->state != MU_SESSION_STATE_ACTIVATED) {
            /* OPC-10000-4 section 7.38.2: a known Session that has not been
               activated is Bad_SessionNotActivated. */
            return MU_STATUS_BAD_SESSIONNOTACTIVATED;
        }
        server->active_session = session;
        /* OPC-10000-4 section 5.7.2.1: every service request on a Session
           refreshes its idle timer; the Server closes a Session that sees no
           requests for revised_session_timeout_ms. Guard get_tick_ms for
           white-box tests that dispatch without a full mu_server_init. */
        if (server->config.time_adapter.get_tick_ms != NULL) {
            session->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        }
    }

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, request_body, request_length);
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, response_body, *response_length);

    if (descriptor->handler != NULL) {
        return descriptor->handler(server, &reader, &writer, response_length);
    }

    *response_length = 0;
    return MU_STATUS_GOOD;
}
