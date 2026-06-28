/* src/core/service_dispatch.c */
#include "service_dispatch.h"
#include "micro_opcua/address_space.h"
#include "micro_opcua/encoding.h"
#include "server_internal.h"
#ifdef MICRO_OPCUA_SERVICE_BROWSE
#include "../services/browse.h"
#endif
#include "../services/discovery.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#ifdef MICRO_OPCUA_SERVICE_READ
#include "../services/read.h"
#endif
#ifdef MICRO_OPCUA_SERVICE_WRITE
#include "../services/write.h"
#endif
#include "../services/service_header.h"
#ifdef MICRO_OPCUA_SECURITY
#include "../security/key_derivation.h"
#include "../security/security_policy.h"
#include "../security/sym_chunk.h"
#include "micro_opcua/security.h"
#endif
#include <stddef.h>
#include <string.h>

#define MU_SERVER_NONCE_LENGTH 32

#if MICRO_OPCUA_SUBSCRIPTIONS && MICRO_OPCUA_SUBSCRIPTIONS_STANDARD && MICRO_OPCUA_BASE_TYPE_SYSTEM
#define MU_DISPATCH_CALL_ENABLED 1
#else
#define MU_DISPATCH_CALL_ENABLED 0
#endif

#ifdef MICRO_OPCUA_SERVICE_READ
opcua_statuscode_t mu_read_process_with_user_index(const mu_address_space_t *address_space,
                                                   mu_address_space_index_t *user_index,
                                                   const mu_address_space_t *dynamic, const mu_read_request_t *req,
                                                   mu_read_response_t *resp, mu_datavalue_t *results_array,
                                                   size_t max_results);
#endif

#ifdef MICRO_OPCUA_SERVICE_BROWSE
opcua_statuscode_t mu_browse_process_with_user_index(const mu_address_space_t *address_space,
                                                     mu_address_space_index_t *user_index,
                                                     const mu_address_space_t *dynamic, const mu_browse_request_t *req,
                                                     mu_browse_result_t *results, size_t max_results,
                                                     mu_reference_description_t *ref_pool, size_t max_total_refs);
#endif

typedef opcua_statuscode_t (*mu_service_dispatch_handler_fn)(mu_server_t *server, mu_binary_reader_t *r,
                                                             mu_binary_writer_t *w, size_t *response_length);

typedef struct {
    mu_service_handler_t service;
    mu_service_dispatch_handler_fn handler;
} mu_service_descriptor_t;

static opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
static opcua_statuscode_t handle_create_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length);
static opcua_statuscode_t handle_activate_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                  size_t *response_length);
static opcua_statuscode_t handle_close_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length);
#ifdef MICRO_OPCUA_SERVICE_DISCOVERY
static opcua_statuscode_t handle_get_endpoints(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length);
static opcua_statuscode_t handle_find_servers(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length);
#endif
#if MICRO_OPCUA_SUBSCRIPTIONS
static opcua_statuscode_t handle_create_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
static opcua_statuscode_t handle_modify_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
static opcua_statuscode_t handle_set_publishing_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
static opcua_statuscode_t handle_create_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
static opcua_statuscode_t handle_modify_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
static opcua_statuscode_t handle_set_monitoring_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length);
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static opcua_statuscode_t handle_set_triggering(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length);
#endif
static opcua_statuscode_t handle_delete_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
static opcua_statuscode_t handle_delete_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                      size_t *response_length);
static opcua_statuscode_t handle_publish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length);
static opcua_statuscode_t handle_republish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_REGISTER_NODES
static opcua_statuscode_t handle_register_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length);
static opcua_statuscode_t handle_unregister_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                  size_t *response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_BROWSE
static opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length);
static opcua_statuscode_t handle_browse(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length);
static opcua_statuscode_t handle_browse_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                             size_t *response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_READ
static opcua_statuscode_t handle_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_WRITE
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length);
#endif
#if MU_DISPATCH_CALL_ENABLED
static opcua_statuscode_t handle_call(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length);
#endif

/* Fill a ServerNonce from the entropy adapter (zeros if unavailable). */
static void fill_server_nonce(mu_server_t *server, opcua_byte_t *nonce, size_t len) {
    if (server->config.entropy_adapter.generate_random != NULL &&
        server->config.entropy_adapter.generate_random(server->config.entropy_adapter.context, nonce, len) ==
            MU_STATUS_GOOD) {
        return;
    }
    memset(nonce, 0, len);
}

static const mu_service_descriptor_t g_supported_services[] = {
#ifdef MICRO_OPCUA_SERVICE_DISCOVERY
    {{MU_ID_FINDSERVERSREQUEST, MU_ID_FINDSERVERSRESPONSE, false}, handle_find_servers},
    {{MU_ID_GETENDPOINTSREQUEST, MU_ID_GETENDPOINTSRESPONSE, false}, handle_get_endpoints},
#endif
    {{MU_ID_OPENSECURECHANNELREQUEST, MU_ID_OPENSECURECHANNELRESPONSE, false}, handle_open_secure_channel},
    {{MU_ID_CLOSESECURECHANNELREQUEST, MU_ID_CLOSESECURECHANNELRESPONSE, false}, NULL},
    {{MU_ID_CREATESESSIONREQUEST, MU_ID_CREATESESSIONRESPONSE, false},
     handle_create_session}, /* Does not require an activated session */
    {{MU_ID_ACTIVATESESSIONREQUEST, MU_ID_ACTIVATESESSIONRESPONSE, false},
     handle_activate_session}, /* Does not require an activated session to activate */
    {{MU_ID_CLOSESESSIONREQUEST, MU_ID_CLOSESESSIONRESPONSE, true}, handle_close_session},
#ifdef MICRO_OPCUA_SERVICE_REGISTER_NODES
    {{MU_ID_REGISTERNODESREQUEST, MU_ID_REGISTERNODESRESPONSE, true}, handle_register_nodes},
    {{MU_ID_UNREGISTERNODESREQUEST, MU_ID_UNREGISTERNODESRESPONSE, true}, handle_unregister_nodes},
#endif
#ifdef MICRO_OPCUA_SERVICE_BROWSE
    {{MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, true},
     handle_translate_browse_paths},
    {{MU_ID_BROWSEREQUEST, MU_ID_BROWSERESPONSE, true}, handle_browse},
    {{MU_ID_BROWSENEXTREQUEST, MU_ID_BROWSENEXTRESPONSE, true}, handle_browse_next},
#endif
#ifdef MICRO_OPCUA_SERVICE_READ
    {{MU_ID_READREQUEST, MU_ID_READRESPONSE, true}, handle_read},
#endif
#ifdef MICRO_OPCUA_SERVICE_WRITE
    {{MU_ID_WRITEREQUEST, MU_ID_WRITERESPONSE, true}, handle_write},
#endif
#if MU_DISPATCH_CALL_ENABLED
    {{MU_ID_CALLREQUEST, MU_ID_CALLRESPONSE, true}, handle_call},
#endif
#if MICRO_OPCUA_SUBSCRIPTIONS
    {{MU_ID_CREATEMONITOREDITEMSREQUEST, MU_ID_CREATEMONITOREDITEMSRESPONSE, true}, handle_create_monitored_items},
    {{MU_ID_MODIFYMONITOREDITEMSREQUEST, MU_ID_MODIFYMONITOREDITEMSRESPONSE, true}, handle_modify_monitored_items},
    {{MU_ID_SETMONITORINGMODEREQUEST, MU_ID_SETMONITORINGMODERESPONSE, true}, handle_set_monitoring_mode},
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    {{MU_ID_SETTRIGGERINGREQUEST, MU_ID_SETTRIGGERINGRESPONSE, true}, handle_set_triggering},
#endif
    {{MU_ID_DELETEMONITOREDITEMSREQUEST, MU_ID_DELETEMONITOREDITEMSRESPONSE, true}, handle_delete_monitored_items},
    {{MU_ID_CREATESUBSCRIPTIONREQUEST, MU_ID_CREATESUBSCRIPTIONRESPONSE, true}, handle_create_subscription},
    {{MU_ID_MODIFYSUBSCRIPTIONREQUEST, MU_ID_MODIFYSUBSCRIPTIONRESPONSE, true}, handle_modify_subscription},
    {{MU_ID_SETPUBLISHINGMODEREQUEST, MU_ID_SETPUBLISHINGMODERESPONSE, true}, handle_set_publishing_mode},
    {{MU_ID_DELETESUBSCRIPTIONSREQUEST, MU_ID_DELETESUBSCRIPTIONSRESPONSE, true}, handle_delete_subscriptions},
    {{MU_ID_PUBLISHREQUEST, MU_ID_PUBLISHRESPONSE, true}, handle_publish},
    {{MU_ID_REPUBLISHREQUEST, MU_ID_REPUBLISHRESPONSE, true}, handle_republish}
#endif
};

static const size_t g_num_supported_services = sizeof(g_supported_services) / sizeof(g_supported_services[0]);

static const mu_service_descriptor_t *find_service_descriptor(opcua_uint32_t request_id) {
    for (size_t i = 0; i < g_num_supported_services; ++i) {
        if (g_supported_services[i].service.request_id == request_id) {
            return &g_supported_services[i];
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
        return;
    }

    server->opn_pending_client_cert = *client_cert;
}

#ifdef MICRO_OPCUA_SECURITY
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
static opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w, opcua_uint32_t response_type_id,
                                                opcua_uint32_t request_handle, opcua_statuscode_t service_result) {
    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {response_type_id}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    return mu_response_header_encode(w, &rh);
}

static opcua_statuscode_t skip_extension_object_body(mu_binary_reader_t *r, size_t length) {
    if (length > 0u) {
        if (r->position > r->length || length > (r->length - r->position)) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        r->position += length;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t ensure_array_items_min_remaining(const mu_binary_reader_t *r, opcua_int32_t count,
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
    if (!buffer || !length)
        return MU_STATUS_BAD_INTERNALERROR;

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, *length);

    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_SERVICEFAULT}};
    opcua_statuscode_t s = mu_binary_write_nodeid(&w, &type);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    s = mu_response_header_encode(&w, &rh);
    if (s != MU_STATUS_GOOD)
        return s;

    *length = w.position;
    return MU_STATUS_GOOD;
}

/* OpenSecureChannel (OPC 10000-4 5.6.2.2): decode the request, open/renew the
   channel, and encode OpenSecureChannelResponse (ChannelSecurityToken + nonce). */
static opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t client_proto, request_type, security_mode, requested_lifetime;
    mu_bytestring_t client_nonce;
    mu_binary_read_uint32(r, &client_proto);
    mu_binary_read_uint32(r, &request_type);
    mu_binary_read_uint32(r, &security_mode);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
    s = mu_binary_read_bytestring(r, &client_nonce);
    if (s != MU_STATUS_GOOD)
        return s;
    mu_binary_read_uint32(r, &requested_lifetime);
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    opcua_uint32_t revised = 0;

#ifdef MICRO_OPCUA_SECURITY
    if (server->config.trust_list != NULL && current_opn_security_policy(server) != NULL) {
        /* TrustList validation is required when security is not None */
        mu_security_policy_id_t requested_policy = mu_security_policy_from_uri(
            current_opn_security_policy(server)->data, (size_t)current_opn_security_policy(server)->length);
        if (requested_policy != MU_SECURITY_POLICY_NONE_ID && requested_policy != MU_SECURITY_POLICY_INVALID_ID) {
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

    s = mu_secure_channel_open(&server_secure_channel, current_opn_security_policy(server),
                               (mu_message_security_mode_t)security_mode, requested_lifetime, &revised);
    if (s != MU_STATUS_GOOD)
        return s;

    s = write_response_prefix(w, MU_ID_OPENSECURECHANNELRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_datetime_t now = server->config.time_adapter.get_time
                               ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                               : 0;

    mu_binary_write_uint32(w, 0);                                /* ServerProtocolVersion */
    mu_binary_write_uint32(w, server_secure_channel.channel_id); /* SecurityToken.ChannelId */
    mu_binary_write_uint32(w, server_secure_channel.token_id);   /* SecurityToken.TokenId */
    mu_binary_write_int64(w, now);                               /* SecurityToken.CreatedAt */
    mu_binary_write_uint32(w, revised);                          /* SecurityToken.RevisedLifetime */
    if (w->status != MU_STATUS_GOOD)
        return w->status;

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};
    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD)
        return s;

    /* Record the negotiated MessageSecurityMode and, for a secured channel,
       derive the symmetric key sets from the nonces (OPC 10000-6 6.7.5). */
    server_secure_channel.mode = (mu_message_security_mode_t)security_mode;
#ifdef MICRO_OPCUA_SECURITY
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID && server->config.crypto_adapter != NULL) {
        const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
        size_t cn_len = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
        if (cn_len == 0)
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        /* Inbound (client->server) keys use ServerNonce as secret; outbound the reverse. */
        s = mu_sym_keys_derive(cr, server_secure_channel.policy, nonce_buf, sizeof(nonce_buf), client_nonce.data,
                               cn_len, &server_secure_channel.client_keys);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_sym_keys_derive(cr, server_secure_channel.policy, client_nonce.data, cn_len, nonce_buf,
                               sizeof(nonce_buf), &server_secure_channel.server_keys);
        if (s != MU_STATUS_GOOD)
            return s;
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

/* Best-effort decode of a CreateSessionRequest, capturing the ClientNonce,
   ClientCertificate, and RequestedSessionTimeout (the latter two are needed to
   compute the ServerSignature on a secured channel). Tolerates a truncated body
   (outputs are left null / *timeout 0, which mu_session_create bounds up). */
static void read_create_session_request(mu_binary_reader_t *r, opcua_uint64_t *timeout_bits,
                                        mu_bytestring_t *client_nonce, mu_bytestring_t *client_cert) {
    mu_string_t str;
    opcua_uint32_t u;
    opcua_int32_t n;
    opcua_byte_t mask;

    *timeout_bits = 0; /* below the minimum -> clamped up */
    client_nonce->length = -1;
    client_nonce->data = NULL;
    client_cert->length = -1;
    client_cert->data = NULL;

    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* ClientDescription.applicationUri */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* productUri */
    if (mu_binary_read_byte(r, &mask) != MU_STATUS_GOOD)
        return; /* applicationName LocalizedText mask */
    if ((mask & 0x01) && (mu_binary_read_string(r, &str) != MU_STATUS_GOOD))
        return; /* locale */
    if ((mask & 0x02) && (mu_binary_read_string(r, &str) != MU_STATUS_GOOD))
        return; /* text */
    if (mu_binary_read_uint32(r, &u) != MU_STATUS_GOOD)
        return; /* applicationType */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* gatewayServerUri */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* discoveryProfileUri */
    if (mu_binary_read_int32(r, &n) != MU_STATUS_GOOD)
        return; /* discoveryUrls count */
    for (opcua_int32_t i = 0; i < n; ++i) {
        if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
            return;
    }
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* ServerUri */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* EndpointUrl */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)
        return; /* SessionName */
    if (mu_binary_read_bytestring(r, client_nonce) != MU_STATUS_GOOD)
        return; /* ClientNonce */
    if (mu_binary_read_bytestring(r, client_cert) != MU_STATUS_GOOD)
        return; /* ClientCertificate */
    {
        /* RequestedSessionTimeout is a Duration (Double); keep its raw bits and
           clamp by integer compare to avoid soft-float on the no-FPU target. */
        opcua_uint64_t bits;
        if (mu_binary_read_uint64(r, &bits) == MU_STATUS_GOOD) {
            *timeout_bits = bits;
        }
    }
}

/* CreateSession (OPC 10000-4 5.7.2.2). Thin path: parse the RequestHeader, create
   the session, and emit a structurally valid CreateSessionResponse. Request params
   (ClientDescription, RequestedSessionTimeout, ...) and the heavy ServerEndpoints
   array are not yet populated. */
static opcua_statuscode_t handle_create_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint64_t requested_bits = 0;
    mu_bytestring_t client_nonce = {-1, NULL}, client_cert = {-1, NULL};
    read_create_session_request(r, &requested_bits, &client_nonce, &client_cert);

    opcua_uint64_t revised_bits = 0;
    opcua_uint32_t session_id = 0, auth_token = 0;
    mu_session_t *slot = mu_session_find_free(server->sessions, MU_MAX_SESSIONS);
    if (slot == NULL) {
        return MU_STATUS_BAD_TOOMANYSESSIONS;
    }

    s = mu_session_create(slot, requested_bits, &revised_bits, &session_id, &auth_token);
    if (s != MU_STATUS_GOOD)
        return s;

    size_t idx = (size_t)(slot - server->sessions);
    slot->session_id = (opcua_uint32_t)(idx + 1u);
    slot->auth_token = (opcua_uint32_t)(12345u + idx);

    s = write_response_prefix(w, MU_ID_CREATESESSIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {slot->session_id}};
    mu_nodeid_t tok = {0, MU_NODEID_NUMERIC, {slot->auth_token}};
    mu_bytestring_t null_bs = {-1, NULL};
    mu_string_t null_str = {-1, NULL};

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    memcpy(slot->server_nonce, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};

    s = mu_binary_write_nodeid(w, &sid);
    if (s != MU_STATUS_GOOD)
        return s; /* SessionId */
    s = mu_binary_write_nodeid(w, &tok);
    if (s != MU_STATUS_GOOD)
        return s; /* AuthenticationToken */
    s = mu_binary_write_uint64(w, revised_bits);
    if (s != MU_STATUS_GOOD)
        return s; /* RevisedSessionTimeout (Double bits) */
    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD)
        return s; /* ServerNonce */

    /* ServerCertificate: the server's own cert when a crypto adapter is present. */
    {
        mu_bytestring_t server_cert = null_bs;
        if (server->config.crypto_adapter != NULL && server->config.crypto_adapter->get_own_certificate != NULL) {
            const opcua_byte_t *c = NULL;
            size_t clen = 0;
            if (server->config.crypto_adapter->get_own_certificate(server->config.crypto_adapter->context, &c, &clen) ==
                MU_STATUS_GOOD) {
                server_cert.length = (opcua_int32_t)clen;
                server_cert.data = c;
            }
        }
        s = mu_binary_write_bytestring(w, &server_cert);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* ServerEndpoints: the same set a Client would get from GetEndpoints. */
    {
        mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
        size_t count = 0;
        s = mu_discovery_get_endpoints(&server->config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_write_int32(w, (opcua_int32_t)count);
        if (s != MU_STATUS_GOOD)
            return s; /* ServerEndpoints[] */
        for (size_t i = 0; i < count; ++i) {
            s = mu_endpoint_description_encode(w, &eps[i]);
            if (s != MU_STATUS_GOOD)
                return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s; /* ServerSoftwareCertificates[] */

    /* ServerSignature: on a secured channel, sign ClientCertificate || ClientNonce
       with the server's private key (RSA-PKCS1.5-SHA256). This proves the server
       holds the private key for its certificate (OPC 10000-4 5.6.2.2). On None it
       is the null/null signature. */
    {
        bool wrote_sig = false;
#ifdef MICRO_OPCUA_SECURITY
        static const char SIG_URI[] = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
        if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
            server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID && server->config.crypto_adapter != NULL &&
            server->config.crypto_adapter->rsa_sha256_sign != NULL && client_cert.length > 0) {
            const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
            size_t cc = (size_t)client_cert.length;
            size_t cn = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
            opcua_byte_t to_sign[1536];
            if (cc + cn <= sizeof(to_sign)) {
                memcpy(to_sign, client_cert.data, cc);
                if (cn > 0)
                    memcpy(to_sign + cc, client_nonce.data, cn);
                opcua_byte_t sig[512];
                size_t sig_len = sizeof(sig);
                if (cr->rsa_sha256_sign(cr->context, to_sign, cc + cn, sig, &sig_len) == MU_STATUS_GOOD) {
                    mu_string_t alg = {(opcua_int32_t)(sizeof(SIG_URI) - 1), (const opcua_byte_t *)SIG_URI};
                    mu_bytestring_t sig_bs = {(opcua_int32_t)sig_len, sig};
                    s = mu_binary_write_string(w, &alg);
                    if (s != MU_STATUS_GOOD)
                        return s;
                    s = mu_binary_write_bytestring(w, &sig_bs);
                    if (s != MU_STATUS_GOOD)
                        return s;
                    wrote_sig = true;
                }
            }
        }
#endif
        if (!wrote_sig) {
            s = mu_binary_write_string(w, &null_str);
            if (s != MU_STATUS_GOOD)
                return s; /* ServerSignature.algorithm */
            s = mu_binary_write_bytestring(w, &null_bs);
            if (s != MU_STATUS_GOOD)
                return s; /* ServerSignature.signature */
        }
    }

    s = mu_binary_write_uint32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s; /* MaxRequestMessageSize */

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* ActivateSession (OPC-10000-4 §5.7.3.2). The UserIdentityToken's ExtensionObject
   typeId identifies the token type; only Anonymous (i=321) is accepted. A service
   failure is reported in the ResponseHeader.serviceResult, not as a transport error. */
static opcua_statuscode_t handle_activate_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                  size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    /* ClientSignature: SignatureData { algorithm(String), signature(ByteString) } */
    mu_string_t algorithm;
    mu_bytestring_t signature;
    s = mu_binary_read_string(r, &algorithm);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_read_bytestring(r, &signature);
    if (s != MU_STATUS_GOOD)
        return s;

    /* OPC-10000-4 §5.7.3.2: clientSoftwareCertificates[] is reserved for
       future use, but every SignedSoftwareCertificate must still be consumed. */
    opcua_int32_t cert_count;
    s = mu_binary_read_int32(r, &cert_count);
    if (s != MU_STATUS_GOOD)
        return s;
    s = ensure_array_items_min_remaining(r, cert_count, 8u);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < cert_count; ++i) {
        mu_bytestring_t certificate_data;
        mu_bytestring_t certificate_signature;
        s = mu_binary_read_bytestring(r, &certificate_data);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_read_bytestring(r, &certificate_signature);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* OPC-10000-4 §5.7.3.2: localeIds[] follows clientSoftwareCertificates[]. */
    opcua_int32_t locale_count;
    s = mu_binary_read_int32(r, &locale_count);
    if (s != MU_STATUS_GOOD)
        return s;
    s = ensure_array_items_min_remaining(r, locale_count, 4u);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < locale_count; ++i) {
        mu_string_t locale;
        s = mu_binary_read_string(r, &locale);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* OPC-10000-4 §5.7.3.2: userIdentityToken ExtensionObject precedes
       userTokenSignature; consume the body at its encoded length for alignment. */
    mu_nodeid_t token_type;
    size_t token_body_len;
    s = mu_binary_read_extension_object_header(r, &token_type, &token_body_len);
    if (s != MU_STATUS_GOOD)
        return s;
    bool token_type_is_ns0_numeric =
        token_type.identifier_type == MU_NODEID_NUMERIC && token_type.namespace_index == 0u;
#ifdef MICRO_OPCUA_USER_AUTH
    mu_username_identity_token_t user_token = {{-1, NULL}, {-1, NULL}, {-1, NULL}, {-1, NULL}};
#endif
#ifdef MICRO_OPCUA_SECURITY
    mu_certificate_identity_token_t cert_token = {{-1, NULL}, {-1, NULL}};
#endif
    mu_string_t anon_policy_id = {-1, NULL};
    mu_string_t user_token_algorithm = {-1, NULL};
    mu_bytestring_t user_token_signature = {-1, NULL};

    if (token_type_is_ns0_numeric && token_type.identifier.numeric == 321) {
        /* Anonymous: body is just policyId (String) */
        if (token_body_len > 0) {
            mu_binary_reader_t sub;
            mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
            s = mu_binary_read_string(&sub, &anon_policy_id);
            if (s != MU_STATUS_GOOD)
                return s;
        }
        r->position += token_body_len;
    } else if (token_type_is_ns0_numeric && token_type.identifier.numeric == 324) {
#ifdef MICRO_OPCUA_USER_AUTH
        if (token_body_len > 0) {
            mu_binary_reader_t sub;
            mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
            s = mu_binary_read_username_identity_token(&sub, &user_token);
            if (s != MU_STATUS_GOOD)
                return s;
        }
        r->position += token_body_len;
#else
        s = skip_extension_object_body(r, token_body_len);
        if (s != MU_STATUS_GOOD)
            return s;
#endif
    } else if (token_type_is_ns0_numeric && token_type.identifier.numeric == 327) {
#ifdef MICRO_OPCUA_SECURITY
        if (token_body_len > 0) {
            mu_binary_reader_t sub;
            mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
            s = mu_binary_read_certificate_identity_token(&sub, &cert_token);
            if (s != MU_STATUS_GOOD)
                return s;
        }
        r->position += token_body_len;
#else
        s = skip_extension_object_body(r, token_body_len);
        if (s != MU_STATUS_GOOD)
            return s;
#endif
    } else {
        s = skip_extension_object_body(r, token_body_len);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* UserTokenSignature: SignatureData { algorithm(String), signature(ByteString) }.
       OPC-10000-4 §5.7.3.2 places it last, so omitted trailing bytes do not
       misalign any following field. If bytes remain, they must still decode. */
    if (r->position < r->length) {
        s = mu_binary_read_string(r, &user_token_algorithm);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_read_bytestring(r, &user_token_signature);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    opcua_uint32_t auth_token = 0u;
    opcua_statuscode_t activate_result = MU_STATUS_BAD_SESSIONIDINVALID;
    if (req.authentication_token.identifier_type == MU_NODEID_NUMERIC &&
        req.authentication_token.namespace_index == 0u) {
        auth_token = req.authentication_token.identifier.numeric;
        mu_session_t *slot = mu_session_find_by_token(server->sessions, MU_MAX_SESSIONS, auth_token);
        if (slot != NULL) {
            if (!token_type_is_ns0_numeric) {
                activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
            } else {
                if (token_type.identifier.numeric == 321) {
                    /* Anonymous validation */
                    if (server->config.user_auth_handler != NULL) {
                        activate_result = server->config.user_auth_handler(server->config.user_auth_handler_handle,
                                                                           NULL, NULL, &anon_policy_id);
                    } else {
                        activate_result = MU_STATUS_GOOD;
                    }
                } else if (token_type.identifier.numeric == 324) {
#ifdef MICRO_OPCUA_USER_AUTH
                    mu_bytestring_t decrypted_password = user_token.password;
                    opcua_byte_t decrypt_buf[256];

                    if (user_token.encryption_algorithm.length > 0 && user_token.password.length > 0) {
                        if (server->config.crypto_adapter != NULL &&
                            server->config.crypto_adapter->rsa_oaep_decrypt != NULL) {
                            size_t out_len = sizeof(decrypt_buf);
                            opcua_statuscode_t ds = server->config.crypto_adapter->rsa_oaep_decrypt(
                                server->config.crypto_adapter->context, user_token.password.data,
                                (size_t)user_token.password.length, decrypt_buf, &out_len);
                            if (ds == MU_STATUS_GOOD) {
                                if (out_len < 4) {
                                    activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                                    goto activate_done;
                                }
                                opcua_int32_t pw_len =
                                    (opcua_int32_t)decrypt_buf[0] | ((opcua_int32_t)decrypt_buf[1] << 8) |
                                    ((opcua_int32_t)decrypt_buf[2] << 16) | ((opcua_int32_t)decrypt_buf[3] << 24);
                                if (pw_len < -1 || (pw_len > 0 && (size_t)pw_len > out_len - 4)) {
                                    activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                                    goto activate_done;
                                }
                                if (pw_len == -1) {
                                    decrypted_password.length = -1;
                                    decrypted_password.data = NULL;
                                } else {
                                    decrypted_password.length = pw_len;
                                    decrypted_password.data = decrypt_buf + 4;
                                }

                                /* Verify server nonce (prevent replay attacks, OPC-10000-4 §5.6.3.2) */
                                size_t actual_pw_len = (pw_len > 0) ? (size_t)pw_len : 0;
                                size_t nonce_offset = 4 + actual_pw_len;
                                size_t nonce_len = out_len - nonce_offset;
                                if (nonce_len != 32 ||
                                    memcmp(decrypt_buf + nonce_offset, slot->server_nonce, 32) != 0) {
                                    activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                                    goto activate_done;
                                }
                            } else {
                                activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                                goto activate_done;
                            }
                        } else {
                            activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                            goto activate_done;
                        }
                    }

                    if (server->config.user_auth_handler != NULL) {
                        activate_result = server->config.user_auth_handler(server->config.user_auth_handler_handle,
                                                                           &user_token.username, &decrypted_password,
                                                                           &user_token.policy_id);
                    } else {
                        /* Secure by default: reject if username token type accepted but no handler configured */
                        activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                    }
                activate_done:;
#else
                    activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
#endif
                } else if (token_type.identifier.numeric == 327) {
                    /* Certificate user authentication */
#ifdef MICRO_OPCUA_SECURITY
                    if (cert_token.certificate_data.length <= 0 || user_token_signature.length <= 0) {
                        activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                    } else {
                        const opcua_byte_t *sc_data = NULL;
                        size_t sc_len = 0;
                        if (server->config.crypto_adapter != NULL &&
                            server->config.crypto_adapter->get_own_certificate != NULL &&
                            server->config.crypto_adapter->rsa_sha256_verify != NULL) {

                            opcua_statuscode_t gcs = server->config.crypto_adapter->get_own_certificate(
                                server->config.crypto_adapter->context, &sc_data, &sc_len);
                            if (gcs == MU_STATUS_GOOD) {
                                opcua_byte_t verify_buf[1536];
                                if (sc_len + 32 <= sizeof(verify_buf)) {
                                    memcpy(verify_buf, sc_data, sc_len);
                                    memcpy(verify_buf + sc_len, slot->server_nonce, 32);

                                    opcua_statuscode_t vs = server->config.crypto_adapter->rsa_sha256_verify(
                                        server->config.crypto_adapter->context, verify_buf, sc_len + 32,
                                        user_token_signature.data, (size_t)user_token_signature.length,
                                        cert_token.certificate_data.data, (size_t)cert_token.certificate_data.length);
                                    if (vs == MU_STATUS_GOOD) {
                                        if (server->config.user_auth_handler != NULL) {
                                            activate_result = server->config.user_auth_handler(
                                                server->config.user_auth_handler_handle, NULL,
                                                &cert_token.certificate_data, &cert_token.policy_id);
                                        } else {
                                            activate_result = MU_STATUS_GOOD;
                                        }
                                    } else {
                                        activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                                    }
                                } else {
                                    activate_result = MU_STATUS_BAD_INTERNALERROR;
                                }
                            } else {
                                activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                            }
                        } else {
                            activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                        }
                    }
#else
                    activate_result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
#endif
                } else {
                    activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
                }

                if (activate_result == MU_STATUS_GOOD) {
#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
                    slot->secure_channel_id = server_secure_channel.channel_id;
#endif
                    activate_result = mu_session_activate(slot, auth_token, token_type.identifier.numeric);
                }
            }
        }
    }

    s = write_response_prefix(w, MU_ID_ACTIVATESESSIONRESPONSE, req.request_handle, activate_result);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};
    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD)
        return s;                /* ServerNonce */
    mu_binary_write_int32(w, 0); /* Results[] */
    mu_binary_write_int32(w, 0); /* DiagnosticInfos[] */
    if (w->status != MU_STATUS_GOOD)
        return w->status;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* CloseSession (OPC 10000-4 5.7.4.2). */
static opcua_statuscode_t handle_close_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_boolean_t delete_subscriptions;
    s = mu_binary_read_boolean(r, &delete_subscriptions);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_statuscode_t close_result = MU_STATUS_BAD_SESSIONIDINVALID;
    if (server->active_session != NULL && req.authentication_token.identifier_type == MU_NODEID_NUMERIC) {
        close_result =
            mu_session_close(server->active_session, req.authentication_token.identifier.numeric, delete_subscriptions);
#if MICRO_OPCUA_SUBSCRIPTIONS
        if (close_result == MU_STATUS_GOOD) {
            opcua_uint32_t session_id = server->active_session->session_id;
            for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
                mu_subscription_t *sub = &server->subs.subscriptions[i];
                if (sub->in_use && sub->session_id == session_id) {
                    (void)mu_subscription_delete(&server->subs, session_id, sub->subscription_id);
                }
            }
        }
#endif
        if (close_result == MU_STATUS_GOOD) {
            server->active_session = NULL;
        }
    }

    s = write_response_prefix(w, MU_ID_CLOSESESSIONRESPONSE, req.request_handle, close_result);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#ifdef MICRO_OPCUA_SERVICE_DISCOVERY
/* GetEndpoints (OPC 10000-4 5.5.2): return the server's single endpoint. The
   request's EndpointUrl/LocaleIds/ProfileUris filters are not applied (thin path). */
static opcua_statuscode_t handle_get_endpoints(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
    size_t count = 0;
    s = mu_discovery_get_endpoints(&server->config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    s = write_response_prefix(w, MU_ID_GETENDPOINTSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, (opcua_int32_t)count); /* Endpoints[] */
    if (s != MU_STATUS_GOOD)
        return s;
    for (size_t i = 0; i < count; ++i) {
        s = mu_endpoint_description_encode(w, &eps[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* FindServers (OPC 10000-4 5.5.2): return this server's ApplicationDescription. */
static opcua_statuscode_t handle_find_servers(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_application_description_t app;
    s = mu_discovery_get_application_description(&server->config, &app);
    if (s != MU_STATUS_GOOD)
        return s;

    s = write_response_prefix(w, MU_ID_FINDSERVERSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, 1); /* Servers[] */
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_application_description_encode(w, &app);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#endif /* MICRO_OPCUA_SERVICE_DISCOVERY */

/* Per-request operation bounds (bounded, stack-allocated). Sized so a standards
   client's connect-time batch reads (e.g. the .NET stack reading the ~12
   ServerCapabilities/OperationLimits properties at once) are accepted rather than
   rejected with Bad_TooManyOperations; missing nodes return per-node
   Bad_NodeIdUnknown, which clients tolerate. */
#define MU_DISPATCH_MAX_READ_NODES 32
#define MU_DISPATCH_MAX_BROWSE_NODES 8
#define MU_DISPATCH_MAX_BROWSE_REFS 32
#define MU_DISPATCH_MAX_BROWSE_CONTINUATION_POINTS 32
#define MU_DISPATCH_MAX_REGISTER_NODES 32
#define MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS 16
#define MU_DISPATCH_MAX_TRANSLATE_ELEMENTS 8
#define MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS 32
#define MU_DISPATCH_MAX_CALL_METHODS 8
#define MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS 4

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
#ifndef MU_ID_SETTRIGGERINGREQUEST
#define MU_ID_SETTRIGGERINGREQUEST 773
#endif
#ifndef MU_ID_SETTRIGGERINGRESPONSE
#define MU_ID_SETTRIGGERINGRESPONSE 776
#endif
#endif

#if MICRO_OPCUA_SUBSCRIPTIONS
#define MU_DISPATCH_MAX_PUBLISH_ACKS MU_MAX_PUBLISH_ACKS
#define MU_DOUBLE_SIGN_BIT 0x8000000000000000ULL
#define MU_PUBLISHING_INTERVAL_MIN_BITS 0x4049000000000000ULL /* 50.0 */
#define MU_PUBLISHING_INTERVAL_MAX_BITS 0x40ed4c0000000000ULL /* 60000.0 */
#define MU_MONITORED_VALUE_ATTRIBUTE_ID 13u
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
/* OPC-10000-4 §7.22.2 DataChangeFilter binary encoding NodeId. */
#define MU_ID_DATACHANGEFILTER_ENCODING_DEFAULTBINARY 724u
#ifndef MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED
/* OPC-10000-4 §5.13.2.4 / §7.38.2. */
#define MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED ((opcua_statuscode_t)0x80440000)
#endif
#endif

#if MU_DISPATCH_CALL_ENABLED
#define MU_ID_SERVER_OBJECT 2253u
#define MU_ID_SERVER_GETMONITOREDITEMS 11492u
#define MU_ID_SERVER_RESENDDATA 12873u
#endif

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    opcua_uint32_t monitoring_mode;
    opcua_uint32_t client_handle;
    opcua_uint64_t sampling_interval_bits;
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    mu_datachange_trigger_t trigger;
    mu_deadband_type_t deadband_type;
    opcua_double_t deadband_value;
    opcua_statuscode_t filter_result;
#endif
    opcua_uint32_t queue_size;
    opcua_boolean_t discard_oldest;
#ifdef MICRO_OPCUA_EVENTS
    opcua_byte_t select_clauses[8];
    opcua_byte_t select_clauses_count;
#endif
} mu_monitored_item_create_body_t;

typedef opcua_statuscode_t (*mu_subscription_id_result_fn)(mu_server_t *server, opcua_uint32_t id, void *context);

typedef struct {
    opcua_boolean_t publishing_enabled;
} mu_set_publishing_mode_context_t;

typedef struct {
    opcua_uint32_t subscription_id;
    opcua_uint32_t monitoring_mode;
} mu_set_monitoring_mode_context_t;

typedef struct {
    opcua_uint32_t subscription_id;
} mu_monitored_item_id_context_t;

static opcua_uint32_t publishing_interval_bits_to_ms(opcua_uint64_t bits) {
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

static opcua_uint64_t publishing_interval_ms_to_bits(opcua_uint32_t interval_ms) {
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

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static bool is_datachange_filter_binary_type(const mu_nodeid_t *type_id) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == MU_ID_DATACHANGEFILTER_ENCODING_DEFAULTBINARY;
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
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
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

    /* OPC-10000-4 §7.22.2: DataChangeFilter is trigger, deadbandType,
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
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        break;
    default:
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        break;
    }

    return MU_STATUS_GOOD;
}
#endif

#ifdef MICRO_OPCUA_EVENTS
#define MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY 727u

static opcua_statuscode_t read_event_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                                 mu_monitored_item_create_body_t *body) {
    (void)filter_length;
    body->select_clauses_count = 0;
    memset(body->select_clauses, 0, sizeof(body->select_clauses));
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    body->filter_result = MU_STATUS_GOOD;
#endif

    opcua_int32_t select_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &select_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (select_count < 0)
        return MU_STATUS_BAD_DECODINGERROR;

    for (opcua_int32_t i = 0; i < select_count; ++i) {
        mu_nodeid_t type_id;
        s = mu_binary_read_nodeid(r, &type_id);
        if (s != MU_STATUS_GOOD)
            return s;

        opcua_int32_t bp_count;
        s = mu_binary_read_int32(r, &bp_count);
        if (s != MU_STATUS_GOOD)
            return s;
        if (bp_count < 0)
            return MU_STATUS_BAD_DECODINGERROR;

        mu_string_t last_name = {-1, NULL};
        for (opcua_int32_t j = 0; j < bp_count; ++j) {
            opcua_uint16_t ns;
            mu_string_t name;
            mu_binary_read_uint16(r, &ns);
            s = mu_binary_read_string(r, &name);
            if (s != MU_STATUS_GOOD)
                return s;
            last_name = name;
        }

        opcua_uint32_t attr_id;
        mu_binary_read_uint32(r, &attr_id);
        mu_string_t idx_range;
        s = mu_binary_read_string(r, &idx_range);
        if (s != MU_STATUS_GOOD)
            return s;

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
    if (s != MU_STATUS_GOOD)
        return s;
    if (element_count < 0)
        return MU_STATUS_BAD_DECODINGERROR;
    for (opcua_int32_t i = 0; i < element_count; ++i) {
        opcua_uint32_t op_type;
        mu_binary_read_uint32(r, &op_type);
        opcua_int32_t arg_count;
        mu_binary_read_int32(r, &arg_count);
        if (arg_count < 0)
            return MU_STATUS_BAD_DECODINGERROR;
        for (opcua_int32_t j = 0; j < arg_count; ++j) {
            mu_nodeid_t ext_id;
            size_t ext_len;
            s = mu_binary_read_extension_object_header(r, &ext_id, &ext_len);
            if (s != MU_STATUS_GOOD)
                return s;
            s = skip_extension_object_body(r, ext_len);
            if (s != MU_STATUS_GOOD)
                return s;
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

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    body->deadband_type = MU_DEADBAND_TYPE_NONE;
    body->deadband_value = 0.0;
    body->filter_result = MU_STATUS_GOOD;
#endif

    s = mu_binary_read_nodeid(r, &body->node_id);
    if (s != MU_STATUS_GOOD)
        return s;
    mu_binary_read_uint32(r, &body->attribute_id);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
    s = mu_binary_read_string(r, &index_range);
    if (s != MU_STATUS_GOOD)
        return s;
    mu_binary_read_uint16(r, &data_encoding_namespace_index);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
    s = mu_binary_read_string(r, &data_encoding_name);
    if (s != MU_STATUS_GOOD)
        return s;
    mu_binary_read_uint32(r, &body->monitoring_mode);
    mu_binary_read_uint32(r, &body->client_handle);
    mu_binary_read_uint64(r, &body->sampling_interval_bits);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
    s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
    if (s != MU_STATUS_GOOD)
        return s;
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    if (is_datachange_filter_binary_type(&filter_type)) {
        s = read_datachange_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD)
            return s;
#ifdef MICRO_OPCUA_EVENTS
    } else if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
               filter_type.identifier.numeric == MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY) {
        s = read_event_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD)
            return s;
#endif
    } else {
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD)
            return s;
    }
#else
#ifdef MICRO_OPCUA_EVENTS
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
    if (w->status != MU_STATUS_GOOD)
        return w->status;
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

static opcua_statuscode_t drive_subscription_id_status_array(
    mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w, size_t *response_length,
    opcua_uint32_t response_type_id, opcua_uint32_t request_handle, bool validate_subscription_id,
    opcua_uint32_t subscription_id, mu_subscription_id_result_fn item_result, void *context) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;

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
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t id;
        s = mu_binary_read_uint32(r, &id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_write_statuscode(w, item_result(server, id, context));
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t set_publishing_mode_result(mu_server_t *server, opcua_uint32_t subscription_id,
                                                     void *context) {
    const mu_set_publishing_mode_context_t *mode = (const mu_set_publishing_mode_context_t *)context;
    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    sub->publishing_enabled = mode->publishing_enabled;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t set_monitoring_mode_result(mu_server_t *server, opcua_uint32_t monitored_item_id,
                                                     void *context) {
    const mu_set_monitoring_mode_context_t *mode = (const mu_set_monitoring_mode_context_t *)context;
    mu_monitored_item_t *item = find_monitored_item(server, mode->subscription_id, monitored_item_id);
    if (item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    item->monitoring_mode = (opcua_byte_t)mode->monitoring_mode;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t delete_monitored_item_result(mu_server_t *server, opcua_uint32_t monitored_item_id,
                                                       void *context) {
    const mu_monitored_item_id_context_t *item_context = (const mu_monitored_item_id_context_t *)context;
    return mu_monitored_item_delete(&server->subs, item_context->subscription_id, monitored_item_id);
}

static opcua_statuscode_t delete_subscription_result(mu_server_t *server, opcua_uint32_t subscription_id,
                                                     void *context) {
    (void)context;
    return mu_subscription_delete(&server->subs, server->active_session->session_id, subscription_id);
}

/* CreateSubscription (OPC 10000-4 5.14.2): decode the request parameters, let the
   fixed-size engine revise counts, and echo the revised values. */
static opcua_statuscode_t handle_create_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint64_t requested_interval_bits;
    opcua_uint32_t requested_lifetime_count;
    opcua_uint32_t requested_max_keep_alive_count;
    opcua_uint32_t max_notifications_per_publish;
    opcua_boolean_t publishing_enabled;
    opcua_byte_t priority;

    mu_binary_read_uint64(r, &requested_interval_bits);
    mu_binary_read_uint32(r, &requested_lifetime_count);
    mu_binary_read_uint32(r, &requested_max_keep_alive_count);
    mu_binary_read_uint32(r, &max_notifications_per_publish);
    mu_binary_read_boolean(r, &publishing_enabled);
    mu_binary_read_byte(r, &priority);
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    opcua_uint32_t publishing_interval_ms = publishing_interval_bits_to_ms(requested_interval_bits);
    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    mu_subscription_t *sub = NULL;
    s = mu_subscription_create(&server->subs, server->active_session->session_id, publishing_interval_ms,
                               requested_lifetime_count, requested_max_keep_alive_count, max_notifications_per_publish,
                               priority, publishing_enabled, now_ms, &sub);
    if (s != MU_STATUS_GOOD)
        return s;

    s = write_response_prefix(w, MU_ID_CREATESUBSCRIPTIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    mu_binary_write_uint32(w, sub->subscription_id);
    mu_binary_write_uint64(w, publishing_interval_ms_to_bits(sub->publishing_interval_ms));
    mu_binary_write_uint32(w, sub->lifetime_count);
    mu_binary_write_uint32(w, sub->max_keep_alive_count);
    if (w->status != MU_STATUS_GOOD)
        return w->status;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* ModifySubscription (OPC 10000-4 5.14.3): update a session-owned Subscription and
   return the revised interval/lifetime/keep-alive values. */
static opcua_statuscode_t handle_modify_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t subscription_id;
    opcua_uint64_t requested_interval_bits;
    opcua_uint32_t requested_lifetime_count;
    opcua_uint32_t requested_max_keep_alive_count;
    opcua_uint32_t max_notifications_per_publish;
    opcua_byte_t priority;

    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint64(r, &requested_interval_bits);
    mu_binary_read_uint32(r, &requested_lifetime_count);
    mu_binary_read_uint32(r, &requested_max_keep_alive_count);
    mu_binary_read_uint32(r, &max_notifications_per_publish);
    mu_binary_read_byte(r, &priority);
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    mu_subscription_apply_parameters(sub, publishing_interval_bits_to_ms(requested_interval_bits),
                                     requested_lifetime_count, requested_max_keep_alive_count,
                                     max_notifications_per_publish, priority);

    s = write_response_prefix(w, MU_ID_MODIFYSUBSCRIPTIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    mu_binary_write_uint64(w, publishing_interval_ms_to_bits(sub->publishing_interval_ms));
    mu_binary_write_uint32(w, sub->lifetime_count);
    mu_binary_write_uint32(w, sub->max_keep_alive_count);
    if (w->status != MU_STATUS_GOOD)
        return w->status;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* SetPublishingMode (OPC 10000-4 5.14.4): toggle publishing per Subscription id and
   report per-id StatusCode results. */
static opcua_statuscode_t handle_set_publishing_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_set_publishing_mode_context_t context = {false};
    mu_binary_read_boolean(r, &context.publishing_enabled);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_SETPUBLISHINGMODERESPONSE,
                                              req.request_handle, false, 0u, set_publishing_mode_result, &context);
}

/* CreateMonitoredItems (OPC 10000-4 5.13.2): data-change monitoring for Value
   Attributes only, backed by the fixed-size subscription engine. */
static opcua_statuscode_t handle_create_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t subscription_id;
    opcua_uint32_t timestamps_to_return;
    opcua_int32_t items_to_create;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &timestamps_to_return);
    mu_binary_read_int32(r, &items_to_create);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
    (void)timestamps_to_return;

    if (items_to_create > 0 && (size_t)items_to_create > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (items_to_create <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, MU_ID_CREATEMONITOREDITEMSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, items_to_create);
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

        bool attr_ok = (body.attribute_id == MU_MONITORED_VALUE_ATTRIBUTE_ID);
#ifdef MICRO_OPCUA_EVENTS
        if (body.attribute_id == 12u) {
            attr_ok = true;
        }
#endif
        if (!attr_ok) {
            s = write_monitored_item_create_result(w, MU_STATUS_BAD_ATTRIBUTEIDINVALID, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
        if (body.filter_result != MU_STATUS_GOOD) {
            s = write_monitored_item_create_result(w, body.filter_result, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }
        if (body.deadband_type == MU_DEADBAND_TYPE_ABSOLUTE && !monitored_node_has_numeric_static_value(node)) {
            s = write_monitored_item_create_result(w, MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }
#endif

        mu_monitored_item_t *item = NULL;
        opcua_statuscode_t result = mu_monitored_item_alloc(&server->subs, subscription_id, &item);
        if (result != MU_STATUS_GOOD) {
            s = write_monitored_item_create_result(w, result, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD)
                return s;
            continue;
        }

        copy_monitored_node_id(item, &body.node_id);
        item->resolved_node = node;
        item->attribute_id = body.attribute_id;
        item->client_handle = body.client_handle;
        item->monitoring_mode = (opcua_byte_t)body.monitoring_mode;
        item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        item->sampling_interval_ms = publishing_interval_bits_to_ms(body.sampling_interval_bits);
        item->next_sample_ms = now_ms + item->sampling_interval_ms;
        item->has_value = false;
        item->pending = false;
#ifdef MICRO_OPCUA_EVENTS
        item->select_clauses_count = body.select_clauses_count;
        memcpy(item->select_clauses, body.select_clauses, sizeof(item->select_clauses));
#endif
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
        item->trigger = body.trigger;
        item->deadband_type = body.deadband_type;
        item->deadband_value = body.deadband_value;
        item->queue_size = body.queue_size;
        if (item->queue_size == 0u) {
            item->queue_size = 1u;
        }
        if (item->queue_size > MU_MONITORED_QUEUE_DEPTH) {
            item->queue_size = MU_MONITORED_QUEUE_DEPTH;
        }
        item->queue_head = 0u;
        item->queue_tail = 0u;
        item->queue_count = 0u;
        item->discard_oldest = body.discard_oldest;
        item->queue_overflow = false;
#endif
        if (body.attribute_id != 12u && node->value != NULL) {
            item->last_status = mu_value_source_read(node->value, &body.node_id, &item->last_value);
            if (item->last_status == MU_STATUS_GOOD) {
                item->has_value = true;
                item->pending = true;
            }
        }
        (void)body.queue_size;
        (void)body.discard_oldest;

        s = write_monitored_item_create_result(w, MU_STATUS_GOOD, item->monitored_item_id, item->sampling_interval_ms,
                                               1u);
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
                                                             opcua_uint32_t revised_sampling_interval_ms) {
    opcua_uint64_t sampling_bits = 0u;
    mu_binary_write_statuscode(w, result);
    if (result == MU_STATUS_GOOD) {
        sampling_bits = publishing_interval_ms_to_bits(revised_sampling_interval_ms);
    }
    mu_binary_write_uint64(w, sampling_bits);
    mu_binary_write_uint32(w, 1u);
    if (w->status != MU_STATUS_GOOD)
        return w->status;
    return write_null_extension_object(w);
}

/* ModifyMonitoredItems (OPC 10000-4 5.13.3): update sampling interval/client handle
   for each MonitoredItem under the session-owned Subscription. */
static opcua_statuscode_t handle_modify_monitored_items(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t subscription_id;
    opcua_uint32_t timestamps_to_return;
    opcua_int32_t count;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &timestamps_to_return);
    mu_binary_read_int32(r, &count);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
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
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t monitored_item_id;
        opcua_uint32_t client_handle;
        opcua_uint64_t sampling_interval_bits;
        mu_nodeid_t filter_type;
        size_t filter_length;
        opcua_uint32_t queue_size;
        opcua_boolean_t discard_oldest;

        mu_binary_read_uint32(r, &monitored_item_id);
        mu_binary_read_uint32(r, &client_handle);
        mu_binary_read_uint64(r, &sampling_interval_bits);
        if (r->status != MU_STATUS_GOOD)
            return r->status;
        s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
        if (s != MU_STATUS_GOOD)
            return s;
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD)
            return s;
        mu_binary_read_uint32(r, &queue_size);
        mu_binary_read_boolean(r, &discard_oldest);
        if (r->status != MU_STATUS_GOOD)
            return r->status;
        (void)filter_type;
        (void)queue_size;
        (void)discard_oldest;

        mu_monitored_item_t *item = find_monitored_item(server, subscription_id, monitored_item_id);
        opcua_statuscode_t result = MU_STATUS_BAD_MONITOREDITEMIDINVALID;
        opcua_uint32_t revised_sampling_interval_ms = 0u;
        if (item != NULL) {
            revised_sampling_interval_ms = publishing_interval_bits_to_ms(sampling_interval_bits);
            item->sampling_interval_ms = revised_sampling_interval_ms;
            item->client_handle = client_handle;
            result = MU_STATUS_GOOD;
        }

        s = write_monitored_item_modify_result(w, result, revised_sampling_interval_ms);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* SetMonitoringMode (OPC 10000-4 5.13.4): update MonitoringMode per MonitoredItem id
   and report per-id StatusCode results. */
static opcua_statuscode_t handle_set_monitoring_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                     size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_set_monitoring_mode_context_t context = {0u, 0u};
    mu_binary_read_uint32(r, &context.subscription_id);
    mu_binary_read_uint32(r, &context.monitoring_mode);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_SETMONITORINGMODERESPONSE,
                                              req.request_handle, true, context.subscription_id,
                                              set_monitoring_mode_result, &context);
}

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static opcua_statuscode_t read_set_triggering_uint32_array(mu_binary_reader_t *r, opcua_uint32_t *items,
                                                           opcua_int32_t *count) {
    opcua_int32_t decoded_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &decoded_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (decoded_count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (decoded_count > 0 && (size_t)decoded_count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = ensure_array_items_min_remaining(r, decoded_count, 4u);
    if (s != MU_STATUS_GOOD)
        return s;
    if (decoded_count <= 0) {
        *count = 0;
        return MU_STATUS_GOOD;
    }

    for (opcua_int32_t i = 0; i < decoded_count; ++i) {
        s = mu_binary_read_uint32(r, &items[i]);
        if (s != MU_STATUS_GOOD)
            return s;
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
    if (s != MU_STATUS_GOOD)
        return s;

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
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    s = read_set_triggering_uint32_array(r, links_to_add, &add_count);
    if (s != MU_STATUS_GOOD)
        return s;
    s = read_set_triggering_uint32_array(r, links_to_remove, &remove_count);
    if (s != MU_STATUS_GOOD)
        return s;
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
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_write_int32(w, add_count);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < add_count; ++i) {
        s = mu_binary_write_statuscode(w, add_results[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_write_int32(w, remove_count);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < remove_count; ++i) {
        s = mu_binary_write_statuscode(w, remove_results[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

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
    if (s != MU_STATUS_GOOD)
        return s;

    mu_monitored_item_id_context_t context = {0u};
    mu_binary_read_uint32(r, &context.subscription_id);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_DELETEMONITOREDITEMSRESPONSE,
                                              req.request_handle, true, context.subscription_id,
                                              delete_monitored_item_result, &context);
}

/* DeleteSubscriptions (OPC 10000-4 5.14.8): service-level Good with per-id
   StatusCode results. DiagnosticInfos is empty, matching other array handlers. */
static opcua_statuscode_t handle_delete_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                      size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_DELETESUBSCRIPTIONSRESPONSE,
                                              req.request_handle, false, 0u, delete_subscription_result, NULL);
}

/* Publish (OPC 10000-4 5.14.5): decode RequestHeader and acknowledgements, then
   park the request. The publishing timer emits the PublishResponse asynchronously. */
static opcua_statuscode_t handle_publish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    (void)w;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_int32_t ack_count;
    s = mu_binary_read_int32(r, &ack_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (ack_count < 0) {
        ack_count = 0;
    }
    if ((size_t)ack_count > MU_DISPATCH_MAX_PUBLISH_ACKS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    opcua_uint32_t stored_ack_count = 0u;
    opcua_statuscode_t ack_results[MU_DISPATCH_MAX_PUBLISH_ACKS];
    for (opcua_int32_t i = 0; i < ack_count; ++i) {
        opcua_uint32_t subscription_id;
        opcua_uint32_t sequence_number;
        mu_binary_read_uint32(r, &subscription_id);
        mu_binary_read_uint32(r, &sequence_number);
        if (r->status != MU_STATUS_GOOD)
            return r->status;
        opcua_statuscode_t ack_result = mu_subscription_acknowledge(&server->subs, server->active_session->session_id,
                                                                    subscription_id, sequence_number);
        if (stored_ack_count < MU_DISPATCH_MAX_PUBLISH_ACKS) {
            ack_results[stored_ack_count] = ack_result;
            ++stored_ack_count;
        }
    }

    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
    mu_publish_request_t *parked = NULL;
    s = mu_publish_request_enqueue(&server->subs, server->active_session->session_id, server->current_request_id,
                                   req.request_handle, now_ms, &parked);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (parked != NULL) {
        parked->ack_count = stored_ack_count;
        for (opcua_uint32_t i = 0u; i < stored_ack_count; ++i) {
            parked->ack_results[i] = ack_results[i];
        }
    }

    *response_length = 0;
    return MU_STATUS_GOOD;
}

/* Republish (OPC 10000-4 5.14.6): return a retained NotificationMessage body for
   the requested subscription sequence number. */
static opcua_statuscode_t handle_republish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t subscription_id;
    opcua_uint32_t sequence_number;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &sequence_number);
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    const opcua_byte_t *message = NULL;
    size_t message_len = 0u;
    s = mu_subscription_republish(&server->subs, server->active_session->session_id, subscription_id, sequence_number,
                                  &message, &message_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_REPUBLISHRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    if (message_len > w->length - w->position) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    if (message_len > 0u) {
        if (message == NULL) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        memcpy(w->buffer + w->position, message, message_len);
        w->position += message_len;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#if MU_DISPATCH_CALL_ENABLED
static bool nodeid_is_ns0_numeric(const mu_nodeid_t *node_id, opcua_uint32_t numeric_id) {
    return node_id->identifier_type == MU_NODEID_NUMERIC && node_id->namespace_index == 0u &&
           node_id->identifier.numeric == numeric_id;
}

static opcua_statuscode_t write_call_method_result(mu_binary_writer_t *w, opcua_statuscode_t status,
                                                   opcua_int32_t input_argument_result_count,
                                                   const opcua_statuscode_t *input_argument_results,
                                                   opcua_int32_t output_argument_count,
                                                   const mu_variant_t *output_arguments) {
    opcua_statuscode_t s = mu_binary_write_statuscode(w, status);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_write_int32(w, input_argument_result_count);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < input_argument_result_count; ++i) {
        s = mu_binary_write_statuscode(w, input_argument_results[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_write_int32(w, output_argument_count);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < output_argument_count; ++i) {
        s = mu_binary_write_variant(w, &output_arguments[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_call_input_arguments(mu_binary_reader_t *r, mu_variant_t *args,
                                                    opcua_int32_t *arg_count) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (count > MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    if (count <= 0) {
        *arg_count = 0;
        return MU_STATUS_GOOD;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        memset(&args[i], 0, sizeof(args[i]));
        s = mu_binary_read_variant(r, &args[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    *arg_count = count;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t write_get_monitored_items_result(mu_server_t *server, mu_binary_writer_t *w,
                                                           opcua_uint32_t subscription_id) {
    opcua_uint32_t server_handles[MU_MAX_MONITORED_ITEMS];
    opcua_uint32_t client_handles[MU_MAX_MONITORED_ITEMS];
    size_t handle_count = 0u;
    opcua_statuscode_t result =
        mu_subscription_get_monitored_items(&server->subs, server->active_session->session_id, subscription_id,
                                            server_handles, client_handles, MU_MAX_MONITORED_ITEMS, &handle_count);
    if (result != MU_STATUS_GOOD) {
        return write_call_method_result(w, result, 0, NULL, 0, NULL);
    }

    mu_variant_t outputs[2];
    memset(outputs, 0, sizeof(outputs));
    outputs[0].type = MU_TYPE_UINT32;
    outputs[0].is_array = true;
    outputs[0].array_length = (opcua_int32_t)handle_count;
    outputs[0].value.array = server_handles;
    outputs[1].type = MU_TYPE_UINT32;
    outputs[1].is_array = true;
    outputs[1].array_length = (opcua_int32_t)handle_count;
    outputs[1].value.array = client_handles;

    return write_call_method_result(w, MU_STATUS_GOOD, 0, NULL, 2, outputs);
}

static opcua_statuscode_t write_resend_data_result(mu_server_t *server, mu_binary_writer_t *w,
                                                   opcua_uint32_t subscription_id) {
    opcua_statuscode_t result =
        mu_subscription_request_resend_data(&server->subs, server->active_session->session_id, subscription_id);
    return write_call_method_result(w, result, 0, NULL, 0, NULL);
}

static opcua_statuscode_t write_single_call_method_result(mu_server_t *server, mu_binary_writer_t *w,
                                                          const mu_nodeid_t *object_id, const mu_nodeid_t *method_id,
                                                          const mu_variant_t *args, opcua_int32_t arg_count) {
    opcua_statuscode_t input_result = MU_STATUS_BAD_INVALIDARGUMENT;

    if (nodeid_is_ns0_numeric(method_id, MU_ID_SERVER_GETMONITOREDITEMS) ||
        nodeid_is_ns0_numeric(method_id, MU_ID_SERVER_RESENDDATA)) {
        if (!nodeid_is_ns0_numeric(object_id, MU_ID_SERVER_OBJECT)) {
            return write_call_method_result(w, MU_STATUS_BAD_NODEIDINVALID, 0, NULL, 0, NULL);
        }
        if (arg_count <= 0) {
            return write_call_method_result(w, MU_STATUS_BAD_ARGUMENTSMISSING, 0, NULL, 0, NULL);
        }
        if (arg_count > 1) {
            return write_call_method_result(w, MU_STATUS_BAD_TOOMANYARGUMENTS, 0, NULL, 0, NULL);
        }
        if (args[0].is_array || args[0].type != MU_TYPE_UINT32) {
            return write_call_method_result(w, MU_STATUS_BAD_INVALIDARGUMENT, 1, &input_result, 0, NULL);
        }

        if (nodeid_is_ns0_numeric(method_id, MU_ID_SERVER_GETMONITOREDITEMS)) {
            return write_get_monitored_items_result(server, w, args[0].value.ui32);
        }

        return write_resend_data_result(server, w, args[0].value.ui32);
    }

#ifdef MICRO_OPCUA_CUSTOM_METHODS
    /* Verify object exists in address space */
    const mu_node_t *obj_node =
        mu_address_space_find_node(server->config.address_space, &server->user_address_space_index, object_id);
    if (obj_node == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_NODEIDINVALID, 0, NULL, 0, NULL);
    }

    /* Verify method node exists in address space */
    const mu_node_t *method_node =
        mu_address_space_find_node(server->config.address_space, &server->user_address_space_index, method_id);
    if (method_node == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
    }

    /* Lookup registered callback */
    mu_method_callback_t callback = NULL;
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (mu_nodeid_equal(&server->registered_methods[i].method_id, method_id)) {
            callback = server->registered_methods[i].callback;
            break;
        }
    }

    if (callback == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
    }

    /* Invoke the custom callback */
    mu_variant_t output_args[8];
    size_t output_args_count = 8;
    memset(output_args, 0, sizeof(output_args));
    opcua_statuscode_t handler_status =
        callback(server, object_id, method_id, args, (size_t)arg_count, output_args, &output_args_count);
    if (handler_status != MU_STATUS_GOOD) {
        return write_call_method_result(w, handler_status, 0, NULL, 0, NULL);
    }

    return write_call_method_result(w, MU_STATUS_GOOD, 0, NULL, (opcua_int32_t)output_args_count, output_args);
#else
    return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
#endif
}

/* Call (OPC-10000-4 §5.12.2.2), limited to OPC-10000-5 Base Info methods
   GetMonitoredItems (§9.1) and ResendData (§9.2) on the Server object. */
static opcua_statuscode_t handle_call(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_int32_t method_count;
    s = mu_binary_read_int32(r, &method_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (method_count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (method_count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }
    if ((size_t)method_count > MU_DISPATCH_MAX_CALL_METHODS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = write_response_prefix(w, MU_ID_CALLRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, method_count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (opcua_int32_t i = 0; i < method_count; ++i) {
        mu_nodeid_t object_id;
        mu_nodeid_t method_id;
        mu_variant_t args[MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS];
        opcua_int32_t arg_count = 0;

        s = mu_binary_read_nodeid(r, &object_id);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_read_nodeid(r, &method_id);
        if (s != MU_STATUS_GOOD)
            return s;
        s = read_call_input_arguments(r, args, &arg_count);
        if (s != MU_STATUS_GOOD)
            return s;

        s = write_single_call_method_result(server, w, &object_id, &method_id, args, arg_count);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif
#endif /* MICRO_OPCUA_SUBSCRIPTIONS */

#ifdef MICRO_OPCUA_SERVICE_REGISTER_NODES
/* RegisterNodes (OPC 10000-4 5.9.5): this server has no alternate optimized
   handles, so registeredNodeIds is the identity mapping of nodesToRegister. */
static opcua_statuscode_t handle_register_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;
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
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = write_response_prefix(w, MU_ID_REGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD)
        return s;
    for (size_t i = 0; i < node_count; ++i) {
        s = mu_binary_write_nodeid(w, &nodes[i]);
        if (s != MU_STATUS_GOOD)
            return s;
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
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;
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
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = write_response_prefix(w, MU_ID_UNREGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_REGISTER_NODES */

#ifdef MICRO_OPCUA_SERVICE_BROWSE
static opcua_boolean_t browse_name_equals(const mu_string_t *left, const mu_string_t *right) {
    if (left->length != right->length) {
        return false;
    }
    if (left->length <= 0) {
        return left->length == 0;
    }
    if (left->data == NULL || right->data == NULL) {
        return false;
    }
    return memcmp(left->data, right->data, (size_t)left->length) == 0;
}

/* TranslateBrowsePathsToNodeIds (OPC 10000-4 5.9.4): resolve each RelativePath
   over the static address space and encode BrowsePathResult[] directly. */
static opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server, mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w, size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_int32_t browse_path_count;
    s = mu_binary_read_int32(r, &browse_path_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (browse_path_count < 0) {
        browse_path_count = 0;
    }
    if ((size_t)browse_path_count > MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = write_response_prefix(w, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(w, browse_path_count);
    if (s != MU_STATUS_GOOD)
        return s;

    const mu_address_space_t *address_space = server->config.address_space;
    size_t path_count = (size_t)browse_path_count;
    for (size_t path_index = 0; path_index < path_count; ++path_index) {
        mu_nodeid_t starting_node;
        s = mu_binary_read_nodeid(r, &starting_node);
        if (s != MU_STATUS_GOOD)
            return s;

        opcua_int32_t element_count;
        s = mu_binary_read_int32(r, &element_count);
        if (s != MU_STATUS_GOOD)
            return s;
        if (element_count < 0) {
            element_count = 0;
        }
        if ((size_t)element_count > MU_DISPATCH_MAX_TRANSLATE_ELEMENTS) {
            return MU_STATUS_BAD_TOOMANYOPERATIONS;
        }

        const mu_node_t *current = NULL;
        if (address_space != NULL) {
            current = mu_address_space_find_node(address_space, &server->user_address_space_index, &starting_node);
        }

        size_t element_total = (size_t)element_count;
        for (size_t element_index = 0; element_index < element_total; ++element_index) {
            mu_nodeid_t reference_type_id;
            opcua_boolean_t is_inverse;
            opcua_boolean_t include_subtypes;
            opcua_uint16_t target_namespace_index;
            mu_string_t target_name;

            s = mu_binary_read_nodeid(r, &reference_type_id);
            if (s != MU_STATUS_GOOD)
                return s;
            mu_binary_read_boolean(r, &is_inverse);
            mu_binary_read_boolean(r, &include_subtypes);
            mu_binary_read_uint16(r, &target_namespace_index);
            if (r->status != MU_STATUS_GOOD)
                return r->status;
            s = mu_binary_read_string(r, &target_name);
            if (s != MU_STATUS_GOOD)
                return s;
            (void)target_namespace_index; /* Nano model stores BrowseName name only. */

            if (current != NULL) {
                const mu_node_t *next = NULL;
                for (size_t ref_index = 0; ref_index < current->reference_count; ++ref_index) {
                    const mu_reference_t *ref = &current->references[ref_index];
                    opcua_boolean_t direction_matches = is_inverse ? !ref->is_forward : ref->is_forward;
                    opcua_boolean_t type_matches;

                    if (!direction_matches) {
                        continue;
                    }

                    if (include_subtypes) {
                        type_matches = ref_type_is_subtype_of(&ref->reference_type_id, &reference_type_id);
                    } else {
                        type_matches = mu_nodeid_equal(&ref->reference_type_id, &reference_type_id);
                    }
                    if (!type_matches) {
                        continue;
                    }

                    const mu_node_t *target =
                        mu_address_space_find_node(address_space, &server->user_address_space_index, &ref->target_id);
                    if (target == NULL) {
                        continue;
                    }
                    if (!browse_name_equals(&target->browse_name, &target_name)) {
                        continue;
                    }

                    next = target;
                    break;
                }
                current = next;
            }
        }

        if (current != NULL) {
            mu_binary_write_statuscode(w, MU_STATUS_GOOD);
            mu_binary_write_int32(w, 1);
            if (w->status != MU_STATUS_GOOD)
                return w->status;
            s = mu_binary_write_nodeid(w, &current->node_id);
            if (s != MU_STATUS_GOOD)
                return s;
            mu_binary_write_uint32(w, 0xFFFFFFFFu);
            if (w->status != MU_STATUS_GOOD)
                return w->status;
        } else {
            mu_binary_write_statuscode(w, MU_STATUS_BAD_NOMATCH);
            mu_binary_write_int32(w, 0);
            if (w->status != MU_STATUS_GOOD)
                return w->status;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_BROWSE */

#ifdef MICRO_OPCUA_SERVICE_READ
/* Read (OPC 10000-4 5.11.2): decode the request after the RequestHeader, read each
   attribute from the address space, and encode the ReadResponse. */
static opcua_statuscode_t handle_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_read_request_t rreq;
    mu_read_value_id_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_request_decode(r, &rreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_read_response_t rresp;
    mu_datavalue_t results[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_process_with_user_index(server->config.address_space, &server->user_address_space_index,
                                        &server->runtime_base.space, &rreq, &rresp, results,
                                        MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD)
        return s;

    s = write_response_prefix(w, MU_ID_READRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_read_response_encode(w, &rresp);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_READ */

#ifdef MICRO_OPCUA_SERVICE_WRITE
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_write_request_t wreq;
    mu_write_value_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_write_request_decode(r, &wreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_write_response_t wresp;
    opcua_statuscode_t results[MU_DISPATCH_MAX_READ_NODES];
    wresp.num_results = wreq.num_nodes_to_write;
    wresp.results = results;

    for (size_t i = 0; i < wreq.num_nodes_to_write; ++i) {
        mu_write_value_t *write_val = &wreq.nodes_to_write[i];
        results[i] = MU_STATUS_GOOD;

        const mu_node_t *node = mu_resolve_node(server->config.address_space, &server->user_address_space_index,
                                                &server->runtime_base.space, &write_val->node_id);
        if (!node) {
            results[i] = MU_STATUS_BAD_NODEIDUNKNOWN;
            continue;
        }

        if (write_val->attribute_id != MU_ATTRIBUTEID_VALUE) {
            results[i] = MU_STATUS_BAD_NOTWRITABLE;
            continue;
        }

        if (node->node_class != MU_NODECLASS_VARIABLE) {
            results[i] = MU_STATUS_BAD_NOTWRITABLE;
            continue;
        }

        /* Check value type matches if the variable has a current value */
        if (node->value) {
            mu_variant_t current_val;
            s = mu_value_source_read(node->value, &node->node_id, &current_val);
            if (s == MU_STATUS_GOOD) {
                if (current_val.type != write_val->value.value.type) {
                    results[i] = MU_STATUS_BAD_TYPEMISMATCH;
                    continue;
                }
            }
        }

        /* Apply write callback if configured */
        if (server->config.write_handler) {
            results[i] = server->config.write_handler(server->config.write_handler_handle, &write_val->node_id,
                                                      (opcua_uint32_t)write_val->attribute_id, &write_val->value.value);
        } else {
            results[i] = MU_STATUS_BAD_WRITENOTSUPPORTED;
        }
    }

    s = write_response_prefix(w, MU_ID_WRITERESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_write_response_encode(w, &wresp);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_WRITE */

#ifdef MICRO_OPCUA_SERVICE_BROWSE
/* Browse (OPC 10000-4 5.9.2): decode the request after the RequestHeader, traverse
   references in the address space, and encode the BrowseResponse. */
static opcua_statuscode_t handle_browse(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_browse_request_t breq;
    mu_browse_description_t descs[MU_DISPATCH_MAX_BROWSE_NODES];
    s = mu_browse_request_decode(r, &breq, descs, MU_DISPATCH_MAX_BROWSE_NODES);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_browse_result_t results[MU_DISPATCH_MAX_BROWSE_NODES];
    mu_reference_description_t ref_pool[MU_DISPATCH_MAX_BROWSE_REFS];
    s = mu_browse_process_with_user_index(server->config.address_space, &server->user_address_space_index,
                                          &server->runtime_base.space, &breq, results, MU_DISPATCH_MAX_BROWSE_NODES,
                                          ref_pool, MU_DISPATCH_MAX_BROWSE_REFS);
    if (s != MU_STATUS_GOOD)
        return s;

    s = write_response_prefix(w, MU_ID_BROWSERESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_browse_response_t bresp;
    bresp.results = results;
    bresp.num_results = breq.num_nodes_to_browse;
    s = mu_browse_response_encode(w, &bresp);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* BrowseNext (OPC 10000-4 5.9.3, 7.9): this server never creates
   ContinuationPoints, so every supplied point is unknown. */
static opcua_statuscode_t handle_browse_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                             size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_boolean_t release_continuation_points;
    mu_binary_read_boolean(r, &release_continuation_points);

    opcua_int32_t count;
    mu_binary_read_int32(r, &count);
    if (r->status != MU_STATUS_GOOD)
        return r->status;
    (void)release_continuation_points;
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > MU_DISPATCH_MAX_BROWSE_CONTINUATION_POINTS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_bytestring_t continuation_point;
        s = mu_binary_read_bytestring(r, &continuation_point);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = write_response_prefix(w, MU_ID_BROWSENEXTRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_bytestring_t null_continuation_point = {-1, NULL};

        s = mu_binary_write_statuscode(w, MU_STATUS_BAD_CONTINUATIONPOINTINVALID);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_write_bytestring(w, &null_continuation_point);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_write_int32(w, 0);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_BROWSE */

opcua_statuscode_t mu_service_dispatch(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *request_body,
                                       size_t request_length, opcua_byte_t *response_body, size_t *response_length) {
    if (!server || !request_body || !response_body || !response_length)
        return MU_STATUS_BAD_INTERNALERROR;

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
        mu_session_t *session = mu_session_find_by_token(server->sessions, MU_MAX_SESSIONS, auth_token);
        if (session == NULL || session->state != MU_SESSION_STATE_ACTIVATED) {
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
        if (session->secure_channel_id != server_secure_channel.channel_id) {
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
#endif
        server->active_session = session;
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
