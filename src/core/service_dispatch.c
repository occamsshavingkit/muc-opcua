/* src/core/service_dispatch.c */
#include "service_dispatch.h"
#include "server_internal.h"
#include "micro_opcua/encoding.h"
#include "../services/discovery.h"
#include "../services/session.h"
#include "../services/secure_channel.h"
#include "../services/browse.h"
#include "../services/read.h"
#include "../services/service_header.h"
#include <stddef.h>

static const mu_service_handler_t g_supported_services[] = {
    { MU_ID_FINDSERVERSREQUEST,        MU_ID_FINDSERVERSRESPONSE,        false },
    { MU_ID_GETENDPOINTSREQUEST,       MU_ID_GETENDPOINTSRESPONSE,       false },
    { MU_ID_OPENSECURECHANNELREQUEST,  MU_ID_OPENSECURECHANNELRESPONSE,  false },
    { MU_ID_CLOSESECURECHANNELREQUEST, MU_ID_CLOSESECURECHANNELRESPONSE, false },
    { MU_ID_CREATESESSIONREQUEST,      MU_ID_CREATESESSIONRESPONSE,      false }, /* Does not require an activated session */
    { MU_ID_ACTIVATESESSIONREQUEST,    MU_ID_ACTIVATESESSIONRESPONSE,    false }, /* Does not require an activated session to activate */
    { MU_ID_CLOSESESSIONREQUEST,       MU_ID_CLOSESESSIONRESPONSE,       true  },
    { MU_ID_BROWSEREQUEST,             MU_ID_BROWSERESPONSE,             true  },
    { MU_ID_READREQUEST,               MU_ID_READRESPONSE,               true  }
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

    mu_bytestring_t server_nonce = { 0, NULL }; /* empty for SecurityPolicy None */
    s = mu_binary_write_bytestring(w, &server_nonce);             if (s != MU_STATUS_GOOD) return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
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

    double revised = 0.0;
    opcua_uint32_t session_id = 0, auth_token = 0;
    s = mu_session_create(&server->session, 0.0, &revised, &session_id, &auth_token);
    if (s != MU_STATUS_GOOD) return s;

    s = write_response_prefix(w, MU_ID_CREATESESSIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) return s;

    mu_nodeid_t sid = { 0, MU_NODEID_NUMERIC, { session_id } };
    mu_nodeid_t tok = { 0, MU_NODEID_NUMERIC, { auth_token } };
    mu_bytestring_t empty_bs = { 0, NULL };
    mu_bytestring_t null_bs = { -1, NULL };
    mu_string_t null_str = { -1, NULL };

    s = mu_binary_write_nodeid(w, &sid);          if (s != MU_STATUS_GOOD) return s; /* SessionId */
    s = mu_binary_write_nodeid(w, &tok);          if (s != MU_STATUS_GOOD) return s; /* AuthenticationToken */
    s = mu_binary_write_double(w, revised);       if (s != MU_STATUS_GOOD) return s; /* RevisedSessionTimeout */
    s = mu_binary_write_bytestring(w, &empty_bs); if (s != MU_STATUS_GOOD) return s; /* ServerNonce */
    s = mu_binary_write_bytestring(w, &null_bs);  if (s != MU_STATUS_GOOD) return s; /* ServerCertificate */
    s = mu_binary_write_int32(w, 0);              if (s != MU_STATUS_GOOD) return s; /* ServerEndpoints[] */
    s = mu_binary_write_int32(w, 0);              if (s != MU_STATUS_GOOD) return s; /* ServerSoftwareCertificates[] */
    s = mu_binary_write_string(w, &null_str);     if (s != MU_STATUS_GOOD) return s; /* ServerSignature.algorithm */
    s = mu_binary_write_bytestring(w, &null_bs);  if (s != MU_STATUS_GOOD) return s; /* ServerSignature.signature */
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

    mu_bytestring_t empty_bs = { 0, NULL };
    s = mu_binary_write_bytestring(w, &empty_bs); if (s != MU_STATUS_GOOD) return s; /* ServerNonce */
    s = mu_binary_write_int32(w, 0);              if (s != MU_STATUS_GOOD) return s; /* Results[] */
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
        case MU_ID_CREATESESSIONREQUEST:
            return handle_create_session(server, &reader, &writer, response_length);
        case MU_ID_ACTIVATESESSIONREQUEST:
            return handle_activate_session(server, &reader, &writer, response_length);
        case MU_ID_CLOSESESSIONREQUEST:
            return handle_close_session(server, &reader, &writer, response_length);
        default:
            /* Not yet wired (discovery/session/browse/read): succeed with no body. */
            *response_length = 0;
            return MU_STATUS_GOOD;
    }
}
