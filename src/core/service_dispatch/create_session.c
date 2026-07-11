/* service_dispatch/create_session.c - CreateSession service handler */
#include "common.h"

static opcua_statuscode_t decode_request_client_description(mu_binary_reader_t *r, mu_string_t *application_uri) {
    mu_string_t str;
    opcua_uint32_t u;
    opcua_int32_t n;
    opcua_byte_t mask;
    opcua_statuscode_t status;

    status = mu_binary_read_string(r, application_uri);
    if (status != MU_STATUS_GOOD) {
        return status; /* ClientDescription.applicationUri */
    }
    status = mu_binary_read_string(r, &str);
    if (status != MU_STATUS_GOOD) {
        return status; /* productUri */
    }
    status = mu_binary_read_byte(r, &mask);
    if (status != MU_STATUS_GOOD) {
        return status; /* applicationName LocalizedText mask */
    }
    if (mask & 0x01) {
        status = mu_binary_read_string(r, &str);
        if (status != MU_STATUS_GOOD) {
            return status; /* locale */
        }
    }
    if (mask & 0x02) {
        status = mu_binary_read_string(r, &str);
        if (status != MU_STATUS_GOOD) {
            return status; /* text */
        }
    }
    status = mu_binary_read_uint32(r, &u);
    if (status != MU_STATUS_GOOD) {
        return status; /* applicationType */
    }
    status = mu_binary_read_string(r, &str);
    if (status != MU_STATUS_GOOD) {
        return status; /* gatewayServerUri */
    }
    status = mu_binary_read_string(r, &str);
    if (status != MU_STATUS_GOOD) {
        return status; /* discoveryProfileUri */
    }
    status = mu_binary_read_int32(r, &n);
    if (status != MU_STATUS_GOOD) {
        return status; /* discoveryUrls count */
    }
    if (n < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    for (opcua_int32_t i = 0; i < n; ++i) {
        status = mu_binary_read_string(r, &str);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t decode_request_trailer(mu_binary_reader_t *r, opcua_uint64_t *timeout_bits,
                                                 mu_bytestring_t *client_nonce, mu_bytestring_t *client_cert) {
    mu_string_t str;
    opcua_uint32_t u;
    opcua_statuscode_t status;

    status = mu_binary_read_string(r, &str);
    if (status != MU_STATUS_GOOD) {
        return status; /* ServerUri */
    }
    status = mu_binary_read_string(r, &str);
    if (status != MU_STATUS_GOOD) {
        return status; /* EndpointUrl */
    }
    status = mu_binary_read_string(r, &str);
    if (status != MU_STATUS_GOOD) {
        return status; /* SessionName */
    }
    status = mu_binary_read_bytestring(r, client_nonce);
    if (status != MU_STATUS_GOOD) {
        return status; /* ClientNonce */
    }
    if (client_nonce->length > 0 && (client_nonce->length < 32 || client_nonce->length > 128)) {
        return MU_STATUS_BAD_NONCEINVALID;
    }
    status = mu_binary_read_bytestring(r, client_cert);
    if (status != MU_STATUS_GOOD) {
        return status; /* ClientCertificate */
    }
    {
        opcua_uint64_t bits;
        status = mu_binary_read_uint64(r, &bits);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        *timeout_bits = bits;
    }
    status = mu_binary_read_uint32(r, &u);
    if (status != MU_STATUS_GOOD) {
        return status; /* MaxResponseMessageSize */
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_create_session_request(mu_binary_reader_t *r, mu_string_t *application_uri,
                                                      opcua_uint64_t *timeout_bits, mu_bytestring_t *client_nonce,
                                                      mu_bytestring_t *client_cert) {
    opcua_statuscode_t status;

    *timeout_bits = 0;
    application_uri->length = -1;
    application_uri->data = NULL;
    client_nonce->length = -1;
    client_nonce->data = NULL;
    client_cert->length = -1;
    client_cert->data = NULL;

    status = decode_request_client_description(r, application_uri);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = decode_request_trailer(r, timeout_bits, client_nonce, client_cert);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_SECURITY
static opcua_statuscode_t validate_create_session_security(mu_server_t *server, const mu_bytestring_t *client_cert,
                                                           const mu_string_t *application_uri) {
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID) {
        if (client_cert->length <= 0 || (size_t)client_cert->length != server->channel_client_cert_len ||
            server->channel_client_cert_len == 0 ||
            memcmp(client_cert->data, server->channel_client_cert, server->channel_client_cert_len) != 0) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        opcua_statuscode_t au_status = mu_certificate_validate_application_uri(
            server->config.crypto_adapter, server_secure_channel.policy, client_cert->data, (size_t)client_cert->length,
            (const char *)application_uri->data, (size_t)(application_uri->length > 0 ? application_uri->length : 0));
        if (au_status != MU_STATUS_GOOD) {
            return au_status;
        }
    }
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t initialize_create_session(mu_server_t *server, mu_binary_writer_t *w,
                                                    const mu_request_header_t *req, opcua_uint64_t requested_bits,
                                                    opcua_uint32_t *session_id, opcua_uint32_t *auth_token,
                                                    opcua_uint64_t *revised_bits, mu_session_t **out_slot,
                                                    bool *session_created_out) {
    opcua_statuscode_t s;
    mu_session_t *slot = mu_session_find_free(server->sessions, MU_INTERN_MAX_SESSIONS);
    if (slot == NULL) {
        return MU_STATUS_BAD_TOOMANYSESSIONS;
    }
    s = mu_session_generate_session_id(server->sessions, MU_INTERN_MAX_SESSIONS, &server->config.entropy_adapter,
                                       session_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_session_generate_authentication_token(server->sessions, MU_INTERN_MAX_SESSIONS,
                                                 &server->config.entropy_adapter, auth_token);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = write_response_prefix(w, MU_ID_CREATESESSIONRESPONSE, req->request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_session_create_with_identifiers(slot, requested_bits, *session_id, *auth_token,
                                           server_secure_channel.channel_id, revised_bits, session_id, auth_token);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    *session_created_out = true;
    *out_slot = slot;
    if (server->config.time_adapter.get_tick_ms != NULL) {
        slot->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
#if defined(MUC_OPCUA_SESSION_TIMEOUT)
        server->next_session_timeout_ms = 0;
#endif
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t encode_session_routing_info(mu_binary_writer_t *w, const mu_nodeid_t *sid,
                                                      const mu_nodeid_t *tok, opcua_uint64_t revised_bits,
                                                      const mu_bytestring_t *server_nonce) {
    opcua_statuscode_t s;
    s = mu_binary_write_nodeid(w, sid);
    if (s != MU_STATUS_GOOD) {
        return s; /* SessionId */
    }
    s = mu_binary_write_nodeid(w, tok);
    if (s != MU_STATUS_GOOD) {
        return s; /* AuthenticationToken */
    }
    s = mu_binary_write_uint64(w, revised_bits);
    if (s != MU_STATUS_GOOD) {
        return s; /* RevisedSessionTimeout (Double bits) */
    }
    s = mu_binary_write_bytestring(w, server_nonce);
    if (s != MU_STATUS_GOOD) {
        return s; /* ServerNonce */
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t encode_server_certificate(mu_binary_writer_t *w, const mu_server_config_t *config,
                                                    const mu_bytestring_t *null_bs) {
    mu_bytestring_t server_cert = *null_bs;
    if (config->crypto_adapter != NULL && config->crypto_adapter->get_own_certificate != NULL) {
        const opcua_byte_t *c = NULL;
        size_t clen = 0;
        if (config->crypto_adapter->get_own_certificate(config->crypto_adapter->context, &c, &clen) == MU_STATUS_GOOD) {
            server_cert.length = (opcua_int32_t)clen;
            server_cert.data = c;
        }
    }
    return mu_binary_write_bytestring(w, &server_cert);
}

static opcua_statuscode_t encode_server_endpoints(mu_binary_writer_t *w, const mu_server_config_t *config) {
    mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
    size_t count = 0;
    opcua_statuscode_t s = mu_discovery_get_endpoints(config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD) {
        return s; /* ServerEndpoints[] */
    }
    for (size_t i = 0; i < count; ++i) {
        s = mu_endpoint_description_encode(w, &eps[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t encode_server_signature(mu_binary_writer_t *w, mu_server_t *server,
                                                  const mu_bytestring_t *client_cert,
                                                  const mu_bytestring_t *client_nonce, const mu_string_t *null_str,
                                                  const mu_bytestring_t *null_bs) {
    opcua_statuscode_t s;
    bool wrote_sig = false;
    (void)server;
    (void)client_cert;
    (void)client_nonce;
#ifdef MUC_OPCUA_SECURITY
    const char *sig_uri = mu_security_policy_asym_signature_uri(server_secure_channel.policy);
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID && server->config.crypto_adapter != NULL &&
        sig_uri != NULL && client_cert->length > 0) {
        const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
        size_t cc = (size_t)client_cert->length;
        size_t cn = client_nonce->length > 0 ? (size_t)client_nonce->length : 0;
        opcua_byte_t to_sign[1536];
        if (cc + cn <= sizeof(to_sign)) {
            memcpy(to_sign, client_cert->data, cc);
            if (cn > 0) {
                memcpy(to_sign + cc, client_nonce->data, cn);
            }
            opcua_byte_t sig[512];
            size_t sig_len = sizeof(sig);
            if (mu_asym_signature_sign(cr, server_secure_channel.policy, to_sign, cc + cn, sig, &sig_len) ==
                MU_STATUS_GOOD) {
                mu_string_t alg = {(opcua_int32_t)strlen(sig_uri), (const opcua_byte_t *)sig_uri};
                mu_bytestring_t sig_bs = {(opcua_int32_t)sig_len, sig};
                s = mu_binary_write_string(w, &alg);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
                s = mu_binary_write_bytestring(w, &sig_bs);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
                wrote_sig = true;
            }
        }
    }
#endif
    if (!wrote_sig) {
        s = mu_binary_write_string(w, null_str);
        if (s != MU_STATUS_GOOD) {
            return s; /* ServerSignature.algorithm */
        }
        s = mu_binary_write_bytestring(w, null_bs);
        if (s != MU_STATUS_GOOD) {
            return s; /* ServerSignature.signature */
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t handle_create_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint64_t requested_bits = 0;
    mu_bytestring_t client_nonce = {-1, NULL}, client_cert = {-1, NULL};
    mu_string_t application_uri = {-1, NULL};
    s = read_create_session_request(r, &application_uri, &requested_bits, &client_nonce, &client_cert);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (r->position != r->length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

#ifdef MUC_OPCUA_SECURITY
    s = validate_create_session_security(server, &client_cert, &application_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#endif

    opcua_uint64_t revised_bits = 0;
    opcua_uint32_t session_id = 0, auth_token = 0;
    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    s = mu_session_generate_server_nonce(&server->config.entropy_adapter, nonce_buf, sizeof(nonce_buf));
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_session_t *slot = NULL;
    bool session_created = false;
    s = initialize_create_session(server, w, &req, requested_bits, &session_id, &auth_token, &revised_bits, &slot,
                                  &session_created);
    if (s != MU_STATUS_GOOD) {
        /* Session could not be created (capacity/validation) -> rejectedSessionCount. */
        mu_diagnostics_session_rejected(server);
        return s;
    }

    mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {slot->session_id}};
    mu_nodeid_t tok = {0, MU_NODEID_NUMERIC, {slot->auth_token}};
    mu_bytestring_t null_bs = {-1, NULL};
    mu_string_t null_str = {-1, NULL};

    memcpy(slot->server_nonce, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};

    s = encode_session_routing_info(w, &sid, &tok, revised_bits, &server_nonce);
    if (s != MU_STATUS_GOOD) {
        goto cleanup;
    }
#ifdef MUC_OPCUA_SECURITY
    mu_secure_zero(nonce_buf, sizeof(nonce_buf));
#endif

    s = encode_server_certificate(w, &server->config, &null_bs);
    if (s != MU_STATUS_GOOD) {
        goto cleanup;
    }

    s = encode_server_endpoints(w, &server->config);
    if (s != MU_STATUS_GOOD) {
        goto cleanup;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* ServerSoftwareCertificates[] */
    }

    s = encode_server_signature(w, server, &client_cert, &client_nonce, &null_str, &null_bs);
    if (s != MU_STATUS_GOOD) {
        goto cleanup;
    }

    s = mu_binary_write_uint32(w, 0);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* MaxRequestMessageSize */
    }

    /* Session is fully created and the response is committed (no path to cleanup below):
       count it. Balanced by mu_diagnostics_session_closed at CloseSession / idle timeout.
       ServerDiagnosticsSummary current/cumulatedSessionCount (OPC-10000-5 §12.9). */
    mu_diagnostics_session_created(server);

    *response_length = w->position;
    return MU_STATUS_GOOD;

cleanup:
    if (session_created) {
        (void)mu_session_close(slot, auth_token, true);
    }
    return s;
}
