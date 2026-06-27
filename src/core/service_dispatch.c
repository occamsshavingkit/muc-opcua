/* src/core/service_dispatch.c */
#include "service_dispatch.h"
#include "server_internal.h"
#include "micro_opcua/address_space.h"
#include "micro_opcua/encoding.h"
#ifdef MICRO_OPCUA_SERVICE_BROWSE
#include "../services/browse.h"
#endif
#include "../services/discovery.h"
#include "../services/session.h"
#include "../services/secure_channel.h"
#ifdef MICRO_OPCUA_SERVICE_READ
#include "../services/read.h"
#endif
#include "../services/service_header.h"
#ifdef MICRO_OPCUA_SECURITY
#include "../security/sym_chunk.h"
#endif
#include <stddef.h>
#include <string.h>

#define MU_SERVER_NONCE_LENGTH 32

/* Fill a ServerNonce from the entropy adapter (zeros if unavailable). */
static void fill_server_nonce(mu_server_t *server, opcua_byte_t *nonce, size_t len) {
    if (server->config.entropy_adapter.generate_random != NULL &&
        server->config.entropy_adapter.generate_random(
            server->config.entropy_adapter.context, nonce, len) == MU_STATUS_GOOD) {
        return;
    }
    memset(nonce, 0, len);
}

static const mu_service_handler_t g_supported_services[] = {
#ifdef MICRO_OPCUA_SERVICE_DISCOVERY
    { MU_ID_FINDSERVERSREQUEST,        MU_ID_FINDSERVERSRESPONSE,        false },
    { MU_ID_GETENDPOINTSREQUEST,       MU_ID_GETENDPOINTSRESPONSE,       false },
#endif
    { MU_ID_OPENSECURECHANNELREQUEST,  MU_ID_OPENSECURECHANNELRESPONSE,  false },
    { MU_ID_CLOSESECURECHANNELREQUEST, MU_ID_CLOSESECURECHANNELRESPONSE, false },
    { MU_ID_CREATESESSIONREQUEST,      MU_ID_CREATESESSIONRESPONSE,      false }, /* Does not require an activated session */
    { MU_ID_ACTIVATESESSIONREQUEST,    MU_ID_ACTIVATESESSIONRESPONSE,    false }, /* Does not require an activated session to activate */
    { MU_ID_CLOSESESSIONREQUEST,       MU_ID_CLOSESESSIONRESPONSE,       true  },
#ifdef MICRO_OPCUA_SERVICE_REGISTER_NODES
    { MU_ID_REGISTERNODESREQUEST,      MU_ID_REGISTERNODESRESPONSE,      true  },
    { MU_ID_UNREGISTERNODESREQUEST,    MU_ID_UNREGISTERNODESRESPONSE,    true  },
#endif
#ifdef MICRO_OPCUA_SERVICE_BROWSE
    { MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, true },
    { MU_ID_BROWSEREQUEST,             MU_ID_BROWSERESPONSE,             true  },
    { MU_ID_BROWSENEXTREQUEST,         MU_ID_BROWSENEXTRESPONSE,         true  },
#endif
#ifdef MICRO_OPCUA_SERVICE_READ
    { MU_ID_READREQUEST,               MU_ID_READRESPONSE,               true  },
#endif
#if MICRO_OPCUA_SUBSCRIPTIONS
    { MU_ID_CREATEMONITOREDITEMSREQUEST, MU_ID_CREATEMONITOREDITEMSRESPONSE, true  },
    { MU_ID_DELETEMONITOREDITEMSREQUEST, MU_ID_DELETEMONITOREDITEMSRESPONSE, true  },
    { MU_ID_CREATESUBSCRIPTIONREQUEST, MU_ID_CREATESUBSCRIPTIONRESPONSE, true  },
    { MU_ID_DELETESUBSCRIPTIONSREQUEST, MU_ID_DELETESUBSCRIPTIONSRESPONSE, true },
    { MU_ID_PUBLISHREQUEST,             MU_ID_PUBLISHRESPONSE,             true  },
    { MU_ID_REPUBLISHREQUEST,           MU_ID_REPUBLISHRESPONSE,           true  }
#endif
};

static const size_t g_num_supported_services = sizeof(g_supported_services) / sizeof(g_supported_services[0]);

const mu_service_handler_t* mu_get_service_handler(opcua_uint32_t request_id) {
    for (size_t i = 0; i < g_num_supported_services; ++i) {
        if (g_supported_services[i].request_id == request_id) {
            return &g_supported_services[i];
        }
    }
    return NULL;
}

/* Write the common response prefix: response-type NodeId + ResponseHeader. */
static opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w,
                                                opcua_uint32_t response_type_id,
                                                opcua_uint32_t request_handle,
                                                opcua_statuscode_t service_result)
{
    mu_nodeid_t type = { 0, MU_NODEID_NUMERIC, { response_type_id } };
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) return s;

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    return mu_response_header_encode(w, &rh);
}

opcua_statuscode_t mu_write_service_fault(opcua_byte_t *buffer, size_t *length,
                                          opcua_uint32_t request_handle,
                                          opcua_statuscode_t service_result)
{
    if (!buffer || !length) return MU_STATUS_BAD_INTERNALERROR;

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, *length);

    mu_nodeid_t type = { 0, MU_NODEID_NUMERIC, { MU_ID_SERVICEFAULT } };
    opcua_statuscode_t s = mu_binary_write_nodeid(&w, &type);
    if (s != MU_STATUS_GOOD) return s;

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    s = mu_response_header_encode(&w, &rh);
    if (s != MU_STATUS_GOOD) return s;

    *length = w.position;
    return MU_STATUS_GOOD;
}

/* OpenSecureChannel (OPC 10000-4 5.6.2.2): decode the request, open/renew the
   channel, and encode OpenSecureChannelResponse (ChannelSecurityToken + nonce). */
static opcua_statuscode_t handle_open_secure_channel(mu_server_t *server,
                                                     mu_binary_reader_t *r,
                                                     mu_binary_writer_t *w,
                                                     size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint32_t client_proto, request_type, security_mode, requested_lifetime;
    mu_bytestring_t client_nonce;
    s = mu_binary_read_uint32(r, &client_proto);       if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &request_type);       if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &security_mode);      if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_bytestring(r, &client_nonce);   if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &requested_lifetime); if (s != MU_STATUS_GOOD) return s;

    opcua_uint32_t revised = 0;
    s = mu_secure_channel_open(&server->secure_channel, NULL, requested_lifetime, &revised);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_OPENSECURECHANNELRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;

    opcua_datetime_t now = server->config.time_adapter.get_time
        ? server->config.time_adapter.get_time(server->config.time_adapter.context) : 0;

    s = mu_binary_write_uint32(w, 0);                              if (s != MU_STATUS_GOOD) return s; /* ServerProtocolVersion */
    s = mu_binary_write_uint32(w, server->secure_channel.channel_id); if (s != MU_STATUS_GOOD) return s; /* SecurityToken.ChannelId */
    s = mu_binary_write_uint32(w, server->secure_channel.token_id);   if (s != MU_STATUS_GOOD) return s; /* SecurityToken.TokenId */
    s = mu_binary_write_int64(w, now);                            if (s != MU_STATUS_GOOD) return s; /* SecurityToken.CreatedAt */
    s = mu_binary_write_uint32(w, revised);                       if (s != MU_STATUS_GOOD) return s; /* SecurityToken.RevisedLifetime */

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = { (opcua_int32_t)sizeof(nonce_buf), nonce_buf };
    s = mu_binary_write_bytestring(w, &server_nonce);             if (s != MU_STATUS_GOOD) return s;

    /* Record the negotiated MessageSecurityMode and, for a secured channel,
       derive the symmetric key sets from the nonces (OPC 10000-6 6.7.5). */
    server->secure_channel.mode = (mu_message_security_mode_t)security_mode;
#ifdef MICRO_OPCUA_SECURITY
    if (server->secure_channel.policy == MU_SECURITY_POLICY_BASIC256SHA256_ID &&
        server->config.crypto_adapter != NULL) {
        const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
        size_t cn_len = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
        if (cn_len == 0) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        /* Inbound (client->server) keys use ServerNonce as secret; outbound the reverse. */
        s = mu_sym_keys_derive(cr, nonce_buf, sizeof(nonce_buf), client_nonce.data, cn_len,
                               &server->secure_channel.client_keys);
        if (s != MU_STATUS_GOOD) return s;
        s = mu_sym_keys_derive(cr, client_nonce.data, cn_len, nonce_buf, sizeof(nonce_buf),
                               &server->secure_channel.server_keys);
        if (s != MU_STATUS_GOOD) return s;
        server->secure_channel.keys_valid = true;
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
    client_nonce->length = -1; client_nonce->data = NULL;
    client_cert->length = -1;  client_cert->data = NULL;

    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* ClientDescription.applicationUri */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* productUri */
    if (mu_binary_read_byte(r, &mask) != MU_STATUS_GOOD) return;       /* applicationName LocalizedText mask */
    if ((mask & 0x01) && (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)) return; /* locale */
    if ((mask & 0x02) && (mu_binary_read_string(r, &str) != MU_STATUS_GOOD)) return; /* text */
    if (mu_binary_read_uint32(r, &u) != MU_STATUS_GOOD) return;        /* applicationType */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* gatewayServerUri */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* discoveryProfileUri */
    if (mu_binary_read_int32(r, &n) != MU_STATUS_GOOD) return;         /* discoveryUrls count */
    for (opcua_int32_t i = 0; i < n; ++i) {
        if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;
    }
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* ServerUri */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* EndpointUrl */
    if (mu_binary_read_string(r, &str) != MU_STATUS_GOOD) return;      /* SessionName */
    if (mu_binary_read_bytestring(r, client_nonce) != MU_STATUS_GOOD) return;  /* ClientNonce */
    if (mu_binary_read_bytestring(r, client_cert) != MU_STATUS_GOOD) return;   /* ClientCertificate */
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
static opcua_statuscode_t handle_create_session(mu_server_t *server,
                                                mu_binary_reader_t *r,
                                                mu_binary_writer_t *w,
                                                size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint64_t requested_bits = 0;
    mu_bytestring_t client_nonce = { -1, NULL }, client_cert = { -1, NULL };
    read_create_session_request(r, &requested_bits, &client_nonce, &client_cert);

    opcua_uint64_t revised_bits = 0;
    opcua_uint32_t session_id = 0, auth_token = 0;
    s = mu_session_create(&server->session, requested_bits, &revised_bits, &session_id, &auth_token);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_CREATESESSIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;

    mu_nodeid_t sid = { 0, MU_NODEID_NUMERIC, { session_id } };
    mu_nodeid_t tok = { 0, MU_NODEID_NUMERIC, { auth_token } };
    mu_bytestring_t null_bs = { -1, NULL };
    mu_string_t null_str = { -1, NULL };

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = { (opcua_int32_t)sizeof(nonce_buf), nonce_buf };

    s = mu_binary_write_nodeid(w, &sid);              if (s != MU_STATUS_GOOD) return s; /* SessionId */
    s = mu_binary_write_nodeid(w, &tok);              if (s != MU_STATUS_GOOD) return s; /* AuthenticationToken */
    s = mu_binary_write_uint64(w, revised_bits);      if (s != MU_STATUS_GOOD) return s; /* RevisedSessionTimeout (Double bits) */
    s = mu_binary_write_bytestring(w, &server_nonce); if (s != MU_STATUS_GOOD) return s; /* ServerNonce */

    /* ServerCertificate: the server's own cert when a crypto adapter is present. */
    {
        mu_bytestring_t server_cert = null_bs;
        if (server->config.crypto_adapter != NULL &&
            server->config.crypto_adapter->get_own_certificate != NULL) {
            const opcua_byte_t *c = NULL; size_t clen = 0;
            if (server->config.crypto_adapter->get_own_certificate(
                    server->config.crypto_adapter->context, &c, &clen) == MU_STATUS_GOOD) {
                server_cert.length = (opcua_int32_t)clen;
                server_cert.data = c;
            }
        }
        s = mu_binary_write_bytestring(w, &server_cert); if (s != MU_STATUS_GOOD) return s;
    }

    /* ServerEndpoints: the same set a Client would get from GetEndpoints. */
    {
        mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
        size_t count = 0;
        s = mu_discovery_get_endpoints(&server->config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
        if (s != MU_STATUS_GOOD) return s;
        s = mu_binary_write_int32(w, (opcua_int32_t)count); if (s != MU_STATUS_GOOD) return s; /* ServerEndpoints[] */
        for (size_t i = 0; i < count; ++i) {
            s = mu_endpoint_description_encode(w, &eps[i]); if (s != MU_STATUS_GOOD) return s;
        }
    }

    s = mu_binary_write_int32(w, 0);              if (s != MU_STATUS_GOOD) return s; /* ServerSoftwareCertificates[] */

    /* ServerSignature: on a secured channel, sign ClientCertificate || ClientNonce
       with the server's private key (RSA-PKCS1.5-SHA256). This proves the server
       holds the private key for its certificate (OPC 10000-4 5.6.2.2). On None it
       is the null/null signature. */
    {
        bool wrote_sig = false;
#ifdef MICRO_OPCUA_SECURITY
        static const char SIG_URI[] = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
        if (server->secure_channel.policy == MU_SECURITY_POLICY_BASIC256SHA256_ID &&
            server->config.crypto_adapter != NULL &&
            server->config.crypto_adapter->rsa_sha256_sign != NULL &&
            client_cert.length > 0) {
            const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
            size_t cc = (size_t)client_cert.length;
            size_t cn = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
            opcua_byte_t to_sign[1536];
            if (cc + cn <= sizeof(to_sign)) {
                memcpy(to_sign, client_cert.data, cc);
                if (cn > 0) memcpy(to_sign + cc, client_nonce.data, cn);
                opcua_byte_t sig[512];
                size_t sig_len = sizeof(sig);
                if (cr->rsa_sha256_sign(cr->context, to_sign, cc + cn, sig, &sig_len) == MU_STATUS_GOOD) {
                    mu_string_t alg = { (opcua_int32_t)(sizeof(SIG_URI) - 1), (const opcua_byte_t *)SIG_URI };
                    mu_bytestring_t sig_bs = { (opcua_int32_t)sig_len, sig };
                    s = mu_binary_write_string(w, &alg);     if (s != MU_STATUS_GOOD) return s;
                    s = mu_binary_write_bytestring(w, &sig_bs); if (s != MU_STATUS_GOOD) return s;
                    wrote_sig = true;
                }
            }
        }
#endif
        if (!wrote_sig) {
            s = mu_binary_write_string(w, &null_str);    if (s != MU_STATUS_GOOD) return s; /* ServerSignature.algorithm */
            s = mu_binary_write_bytestring(w, &null_bs); if (s != MU_STATUS_GOOD) return s; /* ServerSignature.signature */
        }
    }

    s = mu_binary_write_uint32(w, 0);             if (s != MU_STATUS_GOOD) return s; /* MaxRequestMessageSize */

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* ActivateSession (OPC 10000-4 5.7.3.2). The UserIdentityToken's ExtensionObject
   typeId identifies the token type; only Anonymous (i=321) is accepted. A service
   failure is reported in the ResponseHeader.serviceResult, not as a transport error. */
static opcua_statuscode_t handle_activate_session(mu_server_t *server,
                                                  mu_binary_reader_t *r,
                                                  mu_binary_writer_t *w,
                                                  size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    /* ClientSignature: SignatureData { algorithm(String), signature(ByteString) } */
    mu_string_t algorithm;
    mu_bytestring_t signature;
    s = mu_binary_read_string(r, &algorithm);    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_bytestring(r, &signature); if (s != MU_STATUS_GOOD) return s;

    /* ClientSoftwareCertificates: array (thin path assumes empty) */
    opcua_int32_t cert_count;
    s = mu_binary_read_int32(r, &cert_count);    if (s != MU_STATUS_GOOD) return s;

    /* LocaleIds: String[] */
    opcua_int32_t locale_count;
    s = mu_binary_read_int32(r, &locale_count);  if (s != MU_STATUS_GOOD) return s;
    for (opcua_int32_t i = 0; i < locale_count; ++i) {
        mu_string_t locale;
        s = mu_binary_read_string(r, &locale);   if (s != MU_STATUS_GOOD) return s;
    }

    /* UserIdentityToken: ExtensionObject - typeId identifies the token type */
    mu_nodeid_t token_type;
    size_t token_body_len;
    s = mu_binary_read_extension_object_header(r, &token_type, &token_body_len);
    if (s != MU_STATUS_GOOD) return s;

    opcua_statuscode_t activate_result =
        mu_session_activate(&server->session,
                            req.authentication_token.identifier.numeric,
                            token_type.identifier.numeric);

    s = write_response_prefix(w, MU_ID_ACTIVATESESSIONRESPONSE, req.request_handle, activate_result);
    if (s != MU_STATUS_GOOD) return s;

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = { (opcua_int32_t)sizeof(nonce_buf), nonce_buf };
    s = mu_binary_write_bytestring(w, &server_nonce); if (s != MU_STATUS_GOOD) return s; /* ServerNonce */
    s = mu_binary_write_int32(w, 0);                  if (s != MU_STATUS_GOOD) return s; /* Results[] */
    s = mu_binary_write_int32(w, 0);              if (s != MU_STATUS_GOOD) return s; /* DiagnosticInfos[] */

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* CloseSession (OPC 10000-4 5.7.4.2). */
static opcua_statuscode_t handle_close_session(mu_server_t *server,
                                               mu_binary_reader_t *r,
                                               mu_binary_writer_t *w,
                                               size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_boolean_t delete_subscriptions;
    s = mu_binary_read_boolean(r, &delete_subscriptions);
    if (s != MU_STATUS_GOOD) return s;

    opcua_statuscode_t close_result =
        mu_session_close(&server->session,
                         req.authentication_token.identifier.numeric,
                         delete_subscriptions);

    s = write_response_prefix(w, MU_ID_CLOSESESSIONRESPONSE, req.request_handle, close_result);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#ifdef MICRO_OPCUA_SERVICE_DISCOVERY
/* GetEndpoints (OPC 10000-4 5.5.2): return the server's single endpoint. The
   request's EndpointUrl/LocaleIds/ProfileUris filters are not applied (thin path). */
static opcua_statuscode_t handle_get_endpoints(mu_server_t *server,
                                               mu_binary_reader_t *r,
                                               mu_binary_writer_t *w,
                                               size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
    size_t count = 0;
    s = mu_discovery_get_endpoints(&server->config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_GETENDPOINTSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, (opcua_int32_t)count); /* Endpoints[] */
    if (s != MU_STATUS_GOOD) return s;
    for (size_t i = 0; i < count; ++i) {
        s = mu_endpoint_description_encode(w, &eps[i]);
        if (s != MU_STATUS_GOOD) return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* FindServers (OPC 10000-4 5.5.2): return this server's ApplicationDescription. */
static opcua_statuscode_t handle_find_servers(mu_server_t *server,
                                              mu_binary_reader_t *r,
                                              mu_binary_writer_t *w,
                                              size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    mu_application_description_t app;
    s = mu_discovery_get_application_description(&server->config, &app);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_FINDSERVERSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, 1); /* Servers[] */
    if (s != MU_STATUS_GOOD) return s;
    s = mu_application_description_encode(w, &app);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#endif /* MICRO_OPCUA_SERVICE_DISCOVERY */

/* Per-request operation bounds (bounded, stack-allocated). Sized so a standards
   client's connect-time batch reads (e.g. the .NET stack reading the ~12
   ServerCapabilities/OperationLimits properties at once) are accepted rather than
   rejected with Bad_TooManyOperations; missing nodes return per-node
   Bad_NodeIdUnknown, which clients tolerate. */
#define MU_DISPATCH_MAX_READ_NODES   32
#define MU_DISPATCH_MAX_BROWSE_NODES 8
#define MU_DISPATCH_MAX_BROWSE_REFS  32
#define MU_DISPATCH_MAX_BROWSE_CONTINUATION_POINTS 32
#define MU_DISPATCH_MAX_REGISTER_NODES 32
#define MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS 16
#define MU_DISPATCH_MAX_TRANSLATE_ELEMENTS 8

#if MICRO_OPCUA_SUBSCRIPTIONS
#define MU_DOUBLE_SIGN_BIT 0x8000000000000000ULL
#define MU_PUBLISHING_INTERVAL_MIN_BITS 0x4049000000000000ULL  /* 50.0 */
#define MU_PUBLISHING_INTERVAL_MAX_BITS 0x40ed4c0000000000ULL  /* 60000.0 */
#define MU_MONITORED_VALUE_ATTRIBUTE_ID 13u

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    opcua_uint32_t monitoring_mode;
    opcua_uint32_t client_handle;
    opcua_uint64_t sampling_interval_bits;
    opcua_uint32_t queue_size;
    opcua_boolean_t discard_oldest;
} mu_monitored_item_create_body_t;

static opcua_uint32_t publishing_interval_bits_to_ms(opcua_uint64_t bits)
{
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

static opcua_uint64_t publishing_interval_ms_to_bits(opcua_uint32_t interval_ms)
{
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

static opcua_statuscode_t skip_extension_object_body(mu_binary_reader_t *r, size_t length)
{
    if (length > 0u) {
        if (r->position > r->length || length > (r->length - r->position)) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        r->position += length;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_monitored_item_create_body(
    mu_binary_reader_t *r,
    mu_monitored_item_create_body_t *body)
{
    opcua_statuscode_t s;
    mu_string_t index_range;
    opcua_uint16_t data_encoding_namespace_index;
    mu_string_t data_encoding_name;
    mu_nodeid_t filter_type;
    size_t filter_length;

    s = mu_binary_read_nodeid(r, &body->node_id); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &body->attribute_id); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_string(r, &index_range); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint16(r, &data_encoding_namespace_index); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_string(r, &data_encoding_name); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &body->monitoring_mode); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &body->client_handle); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint64(r, &body->sampling_interval_bits); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
    if (s != MU_STATUS_GOOD) return s;
    s = skip_extension_object_body(r, filter_length); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &body->queue_size); if (s != MU_STATUS_GOOD) return s;
    return mu_binary_read_boolean(r, &body->discard_oldest);
}

static void copy_monitored_node_id(mu_monitored_item_t *item, const mu_nodeid_t *node_id)
{
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

static opcua_statuscode_t write_null_extension_object(mu_binary_writer_t *w)
{
    mu_nodeid_t null_id = { 0, MU_NODEID_NUMERIC, { 0 } };
    return mu_binary_write_extension_object_header(w, &null_id, 0);
}

static opcua_statuscode_t write_monitored_item_create_result(
    mu_binary_writer_t *w,
    opcua_statuscode_t result,
    opcua_uint32_t monitored_item_id,
    opcua_uint32_t sampling_interval_ms,
    opcua_uint32_t revised_queue_size)
{
    opcua_uint64_t sampling_bits = 0u;
    opcua_statuscode_t s = mu_binary_write_statuscode(w, result);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_uint32(w, monitored_item_id);
    if (s != MU_STATUS_GOOD) return s;
    if (result == MU_STATUS_GOOD) {
        sampling_bits = publishing_interval_ms_to_bits(sampling_interval_ms);
    }
    s = mu_binary_write_uint64(w, sampling_bits);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_uint32(w, revised_queue_size);
    if (s != MU_STATUS_GOOD) return s;
    return write_null_extension_object(w);
}

/* CreateSubscription (OPC 10000-4 5.14.2): decode the request parameters, let the
   fixed-size engine revise counts, and echo the revised values. */
static opcua_statuscode_t handle_create_subscription(mu_server_t *server,
                                                     mu_binary_reader_t *r,
                                                     mu_binary_writer_t *w,
                                                     size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint64_t requested_interval_bits;
    opcua_uint32_t requested_lifetime_count;
    opcua_uint32_t requested_max_keep_alive_count;
    opcua_uint32_t max_notifications_per_publish;
    opcua_boolean_t publishing_enabled;
    opcua_byte_t priority;

    s = mu_binary_read_uint64(r, &requested_interval_bits);       if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &requested_lifetime_count);      if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &requested_max_keep_alive_count); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &max_notifications_per_publish); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_boolean(r, &publishing_enabled);           if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_byte(r, &priority);                        if (s != MU_STATUS_GOOD) return s;

    opcua_uint32_t publishing_interval_ms =
        publishing_interval_bits_to_ms(requested_interval_bits);
    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    mu_subscription_t *sub = NULL;
    s = mu_subscription_create(&server->subs,
                               server->session.session_id,
                               publishing_interval_ms,
                               requested_lifetime_count,
                               requested_max_keep_alive_count,
                               max_notifications_per_publish,
                               priority,
                               publishing_enabled,
                               now_ms,
                               &sub);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_CREATESUBSCRIPTIONRESPONSE,
                              req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_uint32(w, sub->subscription_id); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_uint64(w, publishing_interval_ms_to_bits(sub->publishing_interval_ms));
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_uint32(w, sub->lifetime_count); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_uint32(w, sub->max_keep_alive_count); if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* CreateMonitoredItems (OPC 10000-4 5.13.2): data-change monitoring for Value
   Attributes only, backed by the fixed-size subscription engine. */
static opcua_statuscode_t handle_create_monitored_items(mu_server_t *server,
                                                        mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w,
                                                        size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint32_t subscription_id;
    opcua_uint32_t timestamps_to_return;
    opcua_int32_t items_to_create;
    s = mu_binary_read_uint32(r, &subscription_id); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &timestamps_to_return); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_int32(r, &items_to_create); if (s != MU_STATUS_GOOD) return s;
    (void)timestamps_to_return;

    mu_subscription_t *sub =
        mu_subscription_find(&server->subs, server->session.session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (items_to_create <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, MU_ID_CREATEMONITOREDITEMSRESPONSE,
                              req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, items_to_create);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint64_t now_ms =
        server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    for (opcua_int32_t i = 0; i < items_to_create; ++i) {
        mu_monitored_item_create_body_t body;
        s = read_monitored_item_create_body(r, &body);
        if (s != MU_STATUS_GOOD) return s;

        const mu_node_t *node =
            mu_resolve_node(server->config.address_space, &server->runtime_base.space, &body.node_id);
        if (node == NULL) {
            s = write_monitored_item_create_result(w, MU_STATUS_BAD_NODEIDUNKNOWN, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD) return s;
            continue;
        }

        if (body.attribute_id != MU_MONITORED_VALUE_ATTRIBUTE_ID) {
            s = write_monitored_item_create_result(w, MU_STATUS_BAD_ATTRIBUTEIDINVALID, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD) return s;
            continue;
        }

        mu_monitored_item_t *item = NULL;
        opcua_statuscode_t result =
            mu_monitored_item_alloc(&server->subs, subscription_id, &item);
        if (result != MU_STATUS_GOOD) {
            s = write_monitored_item_create_result(w, result, 0u, 0u, 0u);
            if (s != MU_STATUS_GOOD) return s;
            continue;
        }

        copy_monitored_node_id(item, &body.node_id);
        item->attribute_id = body.attribute_id;
        item->client_handle = body.client_handle;
        item->monitoring_mode = (mu_monitoring_mode_t)body.monitoring_mode;
        item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        item->sampling_interval_ms = publishing_interval_bits_to_ms(body.sampling_interval_bits);
        item->next_sample_ms = now_ms + item->sampling_interval_ms;
        item->has_value = false;
        item->pending = false;
        if (node->value != NULL) {
            item->last_status = mu_value_source_read(node->value, &body.node_id, &item->last_value);
            if (item->last_status == MU_STATUS_GOOD) {
                item->has_value = true;
                item->pending = true;
            }
        }
        (void)body.queue_size;
        (void)body.discard_oldest;

        s = write_monitored_item_create_result(w, MU_STATUS_GOOD,
                                               item->monitored_item_id,
                                               item->sampling_interval_ms,
                                               1u);
        if (s != MU_STATUS_GOOD) return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* DeleteMonitoredItems (OPC 10000-4 5.13.6): service-level Good with per-id
   StatusCode results for the subscription-owned MonitoredItems. */
static opcua_statuscode_t handle_delete_monitored_items(mu_server_t *server,
                                                        mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w,
                                                        size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint32_t subscription_id;
    opcua_int32_t count;
    s = mu_binary_read_uint32(r, &subscription_id); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_int32(r, &count); if (s != MU_STATUS_GOOD) return s;

    mu_subscription_t *sub =
        mu_subscription_find(&server->subs, server->session.session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, MU_ID_DELETEMONITOREDITEMSRESPONSE,
                              req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) return s;

    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t monitored_item_id;
        s = mu_binary_read_uint32(r, &monitored_item_id);
        if (s != MU_STATUS_GOOD) return s;

        opcua_statuscode_t result =
            mu_monitored_item_delete(&server->subs, subscription_id, monitored_item_id);
        s = mu_binary_write_statuscode(w, result);
        if (s != MU_STATUS_GOOD) return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* DeleteSubscriptions (OPC 10000-4 5.14.8): service-level Good with per-id
   StatusCode results. DiagnosticInfos is empty, matching other array handlers. */
static opcua_statuscode_t handle_delete_subscriptions(mu_server_t *server,
                                                      mu_binary_reader_t *r,
                                                      mu_binary_writer_t *w,
                                                      size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) return s;
    if (count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, MU_ID_DELETESUBSCRIPTIONSRESPONSE,
                              req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) return s;

    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t subscription_id;
        s = mu_binary_read_uint32(r, &subscription_id);
        if (s != MU_STATUS_GOOD) return s;

        opcua_statuscode_t result =
            mu_subscription_delete(&server->subs, server->session.session_id, subscription_id);
        s = mu_binary_write_statuscode(w, result);
        if (s != MU_STATUS_GOOD) return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* Publish (OPC 10000-4 5.14.5): decode RequestHeader and acknowledgements, then
   park the request. The publishing timer emits the PublishResponse asynchronously. */
static opcua_statuscode_t handle_publish(mu_server_t *server,
                                         mu_binary_reader_t *r,
                                         mu_binary_writer_t *w,
                                         size_t *response_length)
{
    (void)w;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_int32_t ack_count;
    s = mu_binary_read_int32(r, &ack_count);
    if (s != MU_STATUS_GOOD) return s;
    if (ack_count < 0) {
        ack_count = 0;
    }

    opcua_uint32_t stored_ack_count = 0u;
    opcua_statuscode_t ack_results[MU_MAX_PUBLISH_ACKS];
    for (opcua_int32_t i = 0; i < ack_count; ++i) {
        opcua_uint32_t subscription_id;
        opcua_uint32_t sequence_number;
        s = mu_binary_read_uint32(r, &subscription_id); if (s != MU_STATUS_GOOD) return s;
        s = mu_binary_read_uint32(r, &sequence_number); if (s != MU_STATUS_GOOD) return s;
        opcua_statuscode_t ack_result =
            mu_subscription_acknowledge(&server->subs,
                                        server->session.session_id,
                                        subscription_id,
                                        sequence_number);
        if (stored_ack_count < MU_MAX_PUBLISH_ACKS) {
            ack_results[stored_ack_count] = ack_result;
            ++stored_ack_count;
        }
    }

    opcua_uint64_t now_ms =
        server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
    mu_publish_request_t *parked = NULL;
    s = mu_publish_request_enqueue(&server->subs,
                                   server->session.session_id,
                                   server->current_request_id,
                                   req.request_handle,
                                   now_ms,
                                   &parked);
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
static opcua_statuscode_t handle_republish(mu_server_t *server,
                                           mu_binary_reader_t *r,
                                           mu_binary_writer_t *w,
                                           size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_uint32_t subscription_id;
    opcua_uint32_t sequence_number;
    s = mu_binary_read_uint32(r, &subscription_id); if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_read_uint32(r, &sequence_number); if (s != MU_STATUS_GOOD) return s;

    const opcua_byte_t *message = NULL;
    size_t message_len = 0u;
    s = mu_subscription_republish(&server->subs,
                                  server->session.session_id,
                                  subscription_id,
                                  sequence_number,
                                  &message,
                                  &message_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_REPUBLISHRESPONSE,
                              req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
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
#endif /* MICRO_OPCUA_SUBSCRIPTIONS */

#ifdef MICRO_OPCUA_SERVICE_REGISTER_NODES
/* RegisterNodes (OPC 10000-4 5.9.5): this server has no alternate optimized
   handles, so registeredNodeIds is the identity mapping of nodesToRegister. */
static opcua_statuscode_t handle_register_nodes(mu_server_t *server,
                                                mu_binary_reader_t *r,
                                                mu_binary_writer_t *w,
                                                size_t *response_length)
{
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) return s;
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
        if (s != MU_STATUS_GOOD) return s;
    }

    s = write_response_prefix(w, MU_ID_REGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) return s;
    for (size_t i = 0; i < node_count; ++i) {
        s = mu_binary_write_nodeid(w, &nodes[i]);
        if (s != MU_STATUS_GOOD) return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* UnregisterNodes (OPC 10000-4 5.9.6): consume the NodeIds and return Good. */
static opcua_statuscode_t handle_unregister_nodes(mu_server_t *server,
                                                  mu_binary_reader_t *r,
                                                  mu_binary_writer_t *w,
                                                  size_t *response_length)
{
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) return s;
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
        if (s != MU_STATUS_GOOD) return s;
    }

    s = write_response_prefix(w, MU_ID_UNREGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_REGISTER_NODES */

#ifdef MICRO_OPCUA_SERVICE_BROWSE
static opcua_boolean_t browse_name_equals(const mu_string_t *left, const mu_string_t *right)
{
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
static opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server,
                                                        mu_binary_reader_t *r,
                                                        mu_binary_writer_t *w,
                                                        size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_int32_t browse_path_count;
    s = mu_binary_read_int32(r, &browse_path_count);
    if (s != MU_STATUS_GOOD) return s;
    if (browse_path_count < 0) {
        browse_path_count = 0;
    }
    if ((size_t)browse_path_count > MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = write_response_prefix(w, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE,
                              req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(w, browse_path_count);
    if (s != MU_STATUS_GOOD) return s;

    const mu_address_space_t *address_space = server->config.address_space;
    size_t path_count = (size_t)browse_path_count;
    for (size_t path_index = 0; path_index < path_count; ++path_index) {
        mu_nodeid_t starting_node;
        s = mu_binary_read_nodeid(r, &starting_node);
        if (s != MU_STATUS_GOOD) return s;

        opcua_int32_t element_count;
        s = mu_binary_read_int32(r, &element_count);
        if (s != MU_STATUS_GOOD) return s;
        if (element_count < 0) {
            element_count = 0;
        }
        if ((size_t)element_count > MU_DISPATCH_MAX_TRANSLATE_ELEMENTS) {
            return MU_STATUS_BAD_TOOMANYOPERATIONS;
        }

        const mu_node_t *current = NULL;
        if (address_space != NULL) {
            current = mu_address_space_find_node(address_space, &starting_node);
        }

        size_t element_total = (size_t)element_count;
        for (size_t element_index = 0; element_index < element_total; ++element_index) {
            mu_nodeid_t reference_type_id;
            opcua_boolean_t is_inverse;
            opcua_boolean_t include_subtypes;
            opcua_uint16_t target_namespace_index;
            mu_string_t target_name;

            s = mu_binary_read_nodeid(r, &reference_type_id);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_read_boolean(r, &is_inverse);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_read_boolean(r, &include_subtypes);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_read_uint16(r, &target_namespace_index);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_read_string(r, &target_name);
            if (s != MU_STATUS_GOOD) return s;
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

                    const mu_node_t *target = mu_address_space_find_node(address_space, &ref->target_id);
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
            s = mu_binary_write_statuscode(w, MU_STATUS_GOOD);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_write_int32(w, 1);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_write_nodeid(w, &current->node_id);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_write_uint32(w, 0xFFFFFFFFu);
            if (s != MU_STATUS_GOOD) return s;
        } else {
            s = mu_binary_write_statuscode(w, MU_STATUS_BAD_NOMATCH);
            if (s != MU_STATUS_GOOD) return s;
            s = mu_binary_write_int32(w, 0);
            if (s != MU_STATUS_GOOD) return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_BROWSE */

#ifdef MICRO_OPCUA_SERVICE_READ
/* Read (OPC 10000-4 5.11.2): decode the request after the RequestHeader, read each
   attribute from the address space, and encode the ReadResponse. */
static opcua_statuscode_t handle_read(mu_server_t *server,
                                      mu_binary_reader_t *r,
                                      mu_binary_writer_t *w,
                                      size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    mu_read_request_t rreq;
    mu_read_value_id_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_request_decode(r, &rreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) return s;

    mu_read_response_t rresp;
    mu_datavalue_t results[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_process(server->config.address_space, &server->runtime_base.space,
                        &rreq, &rresp, results, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_READRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_read_response_encode(w, &rresp);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_READ */

#ifdef MICRO_OPCUA_SERVICE_BROWSE
/* Browse (OPC 10000-4 5.9.2): decode the request after the RequestHeader, traverse
   references in the address space, and encode the BrowseResponse. */
static opcua_statuscode_t handle_browse(mu_server_t *server,
                                        mu_binary_reader_t *r,
                                        mu_binary_writer_t *w,
                                        size_t *response_length)
{
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    mu_browse_request_t breq;
    mu_browse_description_t descs[MU_DISPATCH_MAX_BROWSE_NODES];
    s = mu_browse_request_decode(r, &breq, descs, MU_DISPATCH_MAX_BROWSE_NODES);
    if (s != MU_STATUS_GOOD) return s;

    mu_browse_result_t results[MU_DISPATCH_MAX_BROWSE_NODES];
    mu_reference_description_t ref_pool[MU_DISPATCH_MAX_BROWSE_REFS];
    s = mu_browse_process(server->config.address_space, &server->runtime_base.space, &breq,
                          results, MU_DISPATCH_MAX_BROWSE_NODES,
                          ref_pool, MU_DISPATCH_MAX_BROWSE_REFS);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_BROWSERESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;

    mu_browse_response_t bresp;
    bresp.results = results;
    bresp.num_results = breq.num_nodes_to_browse;
    s = mu_browse_response_encode(w, &bresp);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* BrowseNext (OPC 10000-4 5.9.3, 7.9): this server never creates
   ContinuationPoints, so every supplied point is unknown. */
static opcua_statuscode_t handle_browse_next(mu_server_t *server,
                                             mu_binary_reader_t *r,
                                             mu_binary_writer_t *w,
                                             size_t *response_length)
{
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) return s;

    opcua_boolean_t release_continuation_points;
    s = mu_binary_read_boolean(r, &release_continuation_points);
    if (s != MU_STATUS_GOOD) return s;
    (void)release_continuation_points;

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) return s;
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > MU_DISPATCH_MAX_BROWSE_CONTINUATION_POINTS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_bytestring_t continuation_point;
        s = mu_binary_read_bytestring(r, &continuation_point);
        if (s != MU_STATUS_GOOD) return s;
    }

    s = write_response_prefix(w, MU_ID_BROWSENEXTRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;

    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) return s;
    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_bytestring_t null_continuation_point = { -1, NULL };

        s = mu_binary_write_statuscode(w, MU_STATUS_BAD_CONTINUATIONPOINTINVALID);
        if (s != MU_STATUS_GOOD) return s;
        s = mu_binary_write_bytestring(w, &null_continuation_point);
        if (s != MU_STATUS_GOOD) return s;
        s = mu_binary_write_int32(w, 0);
        if (s != MU_STATUS_GOOD) return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MICRO_OPCUA_SERVICE_BROWSE */

opcua_statuscode_t mu_service_dispatch(
    mu_server_t *server,
    opcua_uint32_t request_id,
    const opcua_byte_t *request_body,
    size_t request_length,
    opcua_byte_t *response_body,
    size_t *response_length)
{
    if (!server || !request_body || !response_body || !response_length) return MU_STATUS_BAD_INTERNALERROR;

    const mu_service_handler_t *handler = mu_get_service_handler(request_id);
    if (handler == NULL) {
        return MU_STATUS_BAD_SERVICEUNSUPPORTED;
    }

    if (request_id != MU_ID_OPENSECURECHANNELREQUEST) {
        if (!server->secure_channel.is_open) {
            /* If we haven't even opened a secure channel, we can't process requests over it */
            /* OPC UA Part 4, 7.38.2: Bad_SecureChannelIdInvalid */
            return MU_STATUS_BAD_SECURECHANNELIDINVALID; 
        }
    }

    if (handler->requires_session) {
        if (server->session.state != MU_SESSION_STATE_ACTIVATED) {
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
    }

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, request_body, request_length);
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, response_body, *response_length);

    switch (request_id) {
        case MU_ID_OPENSECURECHANNELREQUEST:
            return handle_open_secure_channel(server, &reader, &writer, response_length);
#ifdef MICRO_OPCUA_SERVICE_DISCOVERY
        case MU_ID_GETENDPOINTSREQUEST:
            return handle_get_endpoints(server, &reader, &writer, response_length);
        case MU_ID_FINDSERVERSREQUEST:
            return handle_find_servers(server, &reader, &writer, response_length);
#endif
        case MU_ID_CREATESESSIONREQUEST:
            return handle_create_session(server, &reader, &writer, response_length);
        case MU_ID_ACTIVATESESSIONREQUEST:
            return handle_activate_session(server, &reader, &writer, response_length);
        case MU_ID_CLOSESESSIONREQUEST:
            return handle_close_session(server, &reader, &writer, response_length);
#ifdef MICRO_OPCUA_SERVICE_REGISTER_NODES
        case MU_ID_REGISTERNODESREQUEST:
            return handle_register_nodes(server, &reader, &writer, response_length);
        case MU_ID_UNREGISTERNODESREQUEST:
            return handle_unregister_nodes(server, &reader, &writer, response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_BROWSE
        case MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST:
            return handle_translate_browse_paths(server, &reader, &writer, response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_READ
        case MU_ID_READREQUEST:
            return handle_read(server, &reader, &writer, response_length);
#endif
#if MICRO_OPCUA_SUBSCRIPTIONS
        case MU_ID_CREATEMONITOREDITEMSREQUEST:
            return handle_create_monitored_items(server, &reader, &writer, response_length);
        case MU_ID_DELETEMONITOREDITEMSREQUEST:
            return handle_delete_monitored_items(server, &reader, &writer, response_length);
        case MU_ID_CREATESUBSCRIPTIONREQUEST:
            return handle_create_subscription(server, &reader, &writer, response_length);
        case MU_ID_DELETESUBSCRIPTIONSREQUEST:
            return handle_delete_subscriptions(server, &reader, &writer, response_length);
        case MU_ID_PUBLISHREQUEST:
            return handle_publish(server, &reader, &writer, response_length);
        case MU_ID_REPUBLISHREQUEST:
            return handle_republish(server, &reader, &writer, response_length);
#endif
#ifdef MICRO_OPCUA_SERVICE_BROWSE
        case MU_ID_BROWSEREQUEST:
            return handle_browse(server, &reader, &writer, response_length);
        case MU_ID_BROWSENEXTREQUEST:
            return handle_browse_next(server, &reader, &writer, response_length);
#endif
        default:
            /* Not yet wired (discovery/session/browse/read): succeed with no body. */
            *response_length = 0;
            return MU_STATUS_GOOD;
    }
}
