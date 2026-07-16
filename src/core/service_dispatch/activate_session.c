/* service_dispatch/activate_session.c - ActivateSession service handler */
#include "common.h"

static opcua_statuscode_t fill_server_nonce(mu_server_t *server, opcua_byte_t *nonce, size_t len) {
    return mu_session_generate_server_nonce(&server->config.entropy_adapter, nonce, len);
}

#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
static opcua_statuscode_t verify_activate_client_signature(mu_server_t *server, const mu_session_t *slot,
                                                           const mu_string_t *algorithm,
                                                           const mu_bytestring_t *signature) {
    if (server_secure_channel.policy == MU_SECURITY_POLICY_NONE_ID ||
        server_secure_channel.policy == MU_SECURITY_POLICY_INVALID_ID) {
        return MU_STATUS_GOOD; /* None: no ClientSignature required */
    }
    const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
    if (cr == NULL || cr->get_own_certificate == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    if (signature == NULL || signature->length <= 0 || signature->data == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    mu_ecc_curve_t ecc_curve;
    if (mu_security_policy_ecc_curve(server_secure_channel.policy, &ecc_curve)) {
        /* ECC application ClientSignature (OPC-10000-4 §5.7.3): ECDSA/Ed25519 over
           serverEccCertificate || sessionServerNonce, verified against the client's
           ECC certificate captured from the OPN. The SignatureData.Algorithm for ECC
           is, per the .NET/open62541 interop convention, empty OR the SecurityPolicy
           URI itself — the dedicated xmldsig ecdsa URIs are not used. */
        if (cr->get_own_ecc_certificate == NULL || cr->ecc_verify == NULL) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        const char *policy_uri = mu_security_policy_uri(server_secure_channel.policy);
        size_t policy_uri_len = policy_uri ? strlen(policy_uri) : 0;
        bool alg_ok = (algorithm == NULL || algorithm->data == NULL || algorithm->length <= 0) ||
                      (policy_uri != NULL && (size_t)algorithm->length == policy_uri_len &&
                       memcmp(algorithm->data, policy_uri, policy_uri_len) == 0);
        if (!alg_ok) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        if (server->channel_client_cert_len == 0) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        const opcua_byte_t *ecc_own_cert = NULL;
        size_t ecc_own_cert_len = 0;
        if (cr->get_own_ecc_certificate(cr->context, ecc_curve, &ecc_own_cert, &ecc_own_cert_len) != MU_STATUS_GOOD) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        opcua_byte_t ecc_verify_buf[1536];
        if (ecc_own_cert_len + sizeof(slot->server_nonce) > sizeof(ecc_verify_buf)) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        memcpy(ecc_verify_buf, ecc_own_cert, ecc_own_cert_len);
        memcpy(ecc_verify_buf + ecc_own_cert_len, slot->server_nonce, sizeof(slot->server_nonce));
        opcua_statuscode_t evs = cr->ecc_verify(
            cr->context, ecc_curve, server->channel_client_cert, server->channel_client_cert_len, ecc_verify_buf,
            ecc_own_cert_len + sizeof(slot->server_nonce), signature->data, (size_t)signature->length);
        return (evs == MU_STATUS_GOOD) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
#endif
    const char *expected_uri = mu_security_policy_asym_signature_uri(server_secure_channel.policy);
    if (expected_uri == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    size_t expected_uri_len = strlen(expected_uri);
    if (algorithm == NULL || algorithm->data == NULL || algorithm->length < 0 ||
        (size_t)algorithm->length != expected_uri_len || memcmp(algorithm->data, expected_uri, expected_uri_len) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    if (server->channel_client_cert_len == 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    const opcua_byte_t *own_cert = NULL;
    size_t own_cert_len = 0;
    if (cr->get_own_certificate(cr->context, &own_cert, &own_cert_len) != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_byte_t verify_buf[1536];
    if (own_cert_len + sizeof(slot->server_nonce) > sizeof(verify_buf)) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    memcpy(verify_buf, own_cert, own_cert_len);
    memcpy(verify_buf + own_cert_len, slot->server_nonce, sizeof(slot->server_nonce));
    opcua_statuscode_t vs = mu_asym_signature_verify(
        cr, server_secure_channel.policy, server->channel_client_cert, server->channel_client_cert_len, verify_buf,
        own_cert_len + sizeof(slot->server_nonce), signature->data, (size_t)signature->length);
    return (vs == MU_STATUS_GOOD) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}
#endif

static opcua_statuscode_t handle_activate_anonymous(mu_server_t *server, const mu_string_t *anon_policy_id) {
    if (server->config.user_auth_handler != NULL) {
        return server->config.user_auth_handler(server->config.user_auth_handler_handle, NULL, NULL, anon_policy_id);
    }
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_CU_USER_AUTH
static opcua_statuscode_t handle_activate_username(mu_server_t *server, mu_session_t *slot,
                                                   mu_username_identity_token_t *user_token) {
    if (!mu_security_policy_allows_username_password_tokens(server_secure_channel.policy)) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    opcua_statuscode_t result = MU_STATUS_GOOD;
    opcua_byte_t decrypt_buf[512];
    mu_bytestring_t decrypted_password = user_token->password;

    if (user_token->encryption_algorithm.length > 0 && user_token->password.length > 0) {
        if (server->config.crypto_adapter != NULL && server->config.crypto_adapter->rsa_oaep_decrypt != NULL) {
            size_t out_len = sizeof(decrypt_buf);
            opcua_statuscode_t ds = server->config.crypto_adapter->rsa_oaep_decrypt(
                server->config.crypto_adapter->context, user_token->password.data, (size_t)user_token->password.length,
                decrypt_buf, &out_len);
            if (ds != MU_STATUS_GOOD) {
                result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                goto cleanup;
            }
            if (out_len < 4) {
                result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                goto cleanup;
            }
            opcua_int32_t secret_len = (opcua_int32_t)decrypt_buf[0] | ((opcua_int32_t)decrypt_buf[1] << 8) |
                                       ((opcua_int32_t)decrypt_buf[2] << 16) | ((opcua_int32_t)decrypt_buf[3] << 24);
            if (secret_len < 32 || (size_t)secret_len != (out_len - 4u)) {
                result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                goto cleanup;
            }
            size_t actual_pw_len = (size_t)secret_len - 32u;
            decrypted_password.length = (opcua_int32_t)actual_pw_len;
            decrypted_password.data = decrypt_buf + 4;

            size_t nonce_offset = 4u + actual_pw_len;
            size_t nonce_len = out_len - nonce_offset;
            if (nonce_len != 32 || memcmp(decrypt_buf + nonce_offset, slot->server_nonce, 32) != 0) {
                result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                goto cleanup;
            }
        } else {
            result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
            goto cleanup;
        }
    }

    if (server->config.user_auth_handler != NULL) {
        result = server->config.user_auth_handler(server->config.user_auth_handler_handle, &user_token->username,
                                                  &decrypted_password, &user_token->policy_id);
    } else {
        result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

cleanup:
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
    mu_secure_zero(decrypt_buf, sizeof(decrypt_buf));
#endif
    return result;
}
#endif /* MUC_OPCUA_CU_USER_AUTH */

#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
static opcua_statuscode_t handle_activate_certificate(mu_server_t *server, mu_session_t *slot,
                                                      const mu_certificate_identity_token_t *cert_token,
                                                      const mu_bytestring_t *user_token_signature) {
    if (cert_token->certificate_data.length <= 0 || user_token_signature->length <= 0) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    const opcua_byte_t *sc_data = NULL;
    size_t sc_len = 0;
    if (server->config.crypto_adapter == NULL || server->config.crypto_adapter->get_own_certificate == NULL ||
        server->config.crypto_adapter->rsa_sha256_verify == NULL) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    opcua_statuscode_t gcs =
        server->config.crypto_adapter->get_own_certificate(server->config.crypto_adapter->context, &sc_data, &sc_len);
    if (gcs != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    opcua_byte_t verify_buf[1536];
    if (sc_len + 32 > sizeof(verify_buf)) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    memcpy(verify_buf, sc_data, sc_len);
    memcpy(verify_buf + sc_len, slot->server_nonce, 32);

    opcua_statuscode_t vs = server->config.crypto_adapter->rsa_sha256_verify(
        server->config.crypto_adapter->context, cert_token->certificate_data.data,
        (size_t)cert_token->certificate_data.length, verify_buf, sc_len + 32, user_token_signature->data,
        (size_t)user_token_signature->length);
    if (vs != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    if (server->config.user_auth_handler != NULL) {
        return server->config.user_auth_handler(server->config.user_auth_handler_handle, NULL,
                                                &cert_token->certificate_data, &cert_token->policy_id);
    }
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SECURE_CHANNEL_CRYPTO */

static opcua_statuscode_t decode_client_software_certs_and_locales(mu_binary_reader_t *r) {
    opcua_int32_t cert_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &cert_count);
    if (s != MU_STATUS_GOOD)
        return s;
    s = ensure_array_items_min_remaining(r, cert_count, 8u);
    if (s != MU_STATUS_GOOD)
        return s;
    for (opcua_int32_t i = 0; i < cert_count; ++i) {
        mu_bytestring_t certificate_data, certificate_signature;
        s = mu_binary_read_bytestring(r, &certificate_data);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_read_bytestring(r, &certificate_signature);
        if (s != MU_STATUS_GOOD)
            return s;
    }
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
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t decode_anonymous_identity_body(mu_binary_reader_t *r, size_t token_body_len,
                                                         mu_string_t *anon_policy_id) {
    opcua_statuscode_t s = ensure_reader_bytes_remaining(r, token_body_len);
    if (s != MU_STATUS_GOOD)
        return s;
    if (token_body_len > 0) {
        mu_binary_reader_t sub;
        mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
        s = mu_binary_read_string(&sub, anon_policy_id);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    r->position += token_body_len;
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_CU_USER_AUTH
static opcua_statuscode_t decode_username_identity_body(mu_binary_reader_t *r, size_t token_body_len,
                                                        mu_username_identity_token_t *user_token) {
    opcua_statuscode_t s = ensure_reader_bytes_remaining(r, token_body_len);
    if (s != MU_STATUS_GOOD)
        return s;
    if (token_body_len > 0) {
        mu_binary_reader_t sub;
        mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
        s = mu_binary_read_username_identity_token(&sub, user_token);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    r->position += token_body_len;
    return MU_STATUS_GOOD;
}
#endif

#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
static opcua_statuscode_t decode_certificate_identity_body(mu_binary_reader_t *r, size_t token_body_len,
                                                           mu_certificate_identity_token_t *cert_token) {
    opcua_statuscode_t s = ensure_reader_bytes_remaining(r, token_body_len);
    if (s != MU_STATUS_GOOD)
        return s;
    if (token_body_len > 0) {
        mu_binary_reader_t sub;
        mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
        s = mu_binary_read_certificate_identity_token(&sub, cert_token);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    r->position += token_body_len;
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t decode_user_identity_token(mu_binary_reader_t *r, opcua_uint32_t *out_token_numeric,
                                                     mu_string_t *anon_policy_id
#ifdef MUC_OPCUA_CU_USER_AUTH
                                                     ,
                                                     mu_username_identity_token_t *user_token
#endif
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
                                                     ,
                                                     mu_certificate_identity_token_t *cert_token
#endif
) {
    mu_nodeid_t token_type;
    size_t token_body_len;
    opcua_statuscode_t s = mu_binary_read_extension_object_header(r, &token_type, &token_body_len);
    if (s != MU_STATUS_GOOD)
        return s;

    bool is_ns0_numeric = token_type.identifier_type == MU_NODEID_NUMERIC && token_type.namespace_index == 0u;
    opcua_uint32_t num = is_ns0_numeric ? token_type.identifier.numeric : 0u;
    *out_token_numeric = num;

    if (is_ns0_numeric && num == 321) {
        s = decode_anonymous_identity_body(r, token_body_len, anon_policy_id);
    } else if (is_ns0_numeric && num == 324) {
#ifdef MUC_OPCUA_CU_USER_AUTH
        s = decode_username_identity_body(r, token_body_len, user_token);
#else
        s = skip_extension_object_body(r, token_body_len);
#endif
    } else if (is_ns0_numeric && num == 327) {
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
        s = decode_certificate_identity_body(r, token_body_len, cert_token);
#else
        s = skip_extension_object_body(r, token_body_len);
#endif
    } else {
        s = skip_extension_object_body(r, token_body_len);
    }
    return s;
}

static opcua_statuscode_t verify_and_activate_session(mu_server_t *server, const mu_request_header_t *req,
                                                      const mu_string_t *algorithm, const mu_bytestring_t *signature,
                                                      opcua_uint32_t token_type_numeric,
                                                      const mu_string_t *anon_policy_id
#ifdef MUC_OPCUA_CU_USER_AUTH
                                                      ,
                                                      mu_username_identity_token_t *user_token
#endif
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
                                                      ,
                                                      const mu_certificate_identity_token_t *cert_token
#endif
                                                      ,
                                                      const mu_bytestring_t *user_token_signature) {
    opcua_statuscode_t activate_result = MU_STATUS_BAD_SESSIONIDINVALID;
    if (req->authentication_token.identifier_type != MU_NODEID_NUMERIC ||
        req->authentication_token.namespace_index != 0u) {
        return activate_result;
    }
    opcua_uint32_t auth_token = req->authentication_token.identifier.numeric;
    mu_session_t *slot = mu_session_find_by_token(server->sessions, MU_INTERN_MAX_SESSIONS, auth_token);
    if (slot == NULL)
        return activate_result;
    (void)auth_token;
    activate_result = mu_session_validate_secure_channel(slot, server_secure_channel.channel_id);
    if (activate_result != MU_STATUS_GOOD) {
        return activate_result;
    }
#ifndef MUC_OPCUA_CU_SESSION_CHANGE_USER
    if (slot->state == MU_SESSION_STATE_ACTIVATED) {
        /* OPC-10000-4 sections 5.7.2 and 5.7.3: initial activation remains
           available; replacing the user of an activated Session is opc_cu_2400. */
        return MU_STATUS_BAD_SERVICEUNSUPPORTED;
    }
#endif
    if (token_type_numeric == 0u) {
        return MU_STATUS_BAD_IDENTITYTOKENINVALID;
    }
    opcua_statuscode_t client_sig_result = MU_STATUS_GOOD;
    (void)algorithm;
    (void)signature;
    (void)user_token_signature;
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
    client_sig_result = verify_activate_client_signature(server, slot, algorithm, signature);
#endif
    if (client_sig_result != MU_STATUS_GOOD) {
        return client_sig_result;
    } else if (token_type_numeric == 321) {
        activate_result = handle_activate_anonymous(server, anon_policy_id);
    } else if (token_type_numeric == 324) {
#ifdef MUC_OPCUA_CU_USER_AUTH
        activate_result = handle_activate_username(server, slot, user_token);
#else
        activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
#endif
    } else if (token_type_numeric == 327) {
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
        activate_result = handle_activate_certificate(server, slot, cert_token, user_token_signature);
#else
        activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
#endif
    } else {
        activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
    }
    if (activate_result == MU_STATUS_GOOD) {
        activate_result = mu_session_activate(slot, auth_token, token_type_numeric);
#if MUC_OPCUA_CU_REDUNDANCY
        if (activate_result == MU_STATUS_GOOD) {
            /* Fingerprint the user for the TransferSubscriptions same-user check. Store a
               compact identity kind (1 anonymous, 2 username, 3 x509) that fits a byte. */
            slot->user_identity_kind = (token_type_numeric == 324u) ? 2u : (token_type_numeric == 327u) ? 3u : 1u;
            slot->user_identity_len = 0;
            const mu_bytestring_t *disc = NULL;
#ifdef MUC_OPCUA_CU_USER_AUTH
            mu_bytestring_t uname;
            if (token_type_numeric == 324u && user_token != NULL) {
                uname.length = user_token->username.length;
                uname.data = user_token->username.data;
                disc = &uname;
            }
#endif
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
            if (token_type_numeric == 327u && cert_token != NULL) {
                disc = &cert_token->certificate_data;
            }
#endif
            if (disc != NULL && disc->length > 0 && disc->data != NULL) {
                size_t n = (size_t)disc->length;
                if (n > sizeof(slot->user_identity)) {
                    n = sizeof(slot->user_identity);
                }
                (void)memcpy(slot->user_identity, disc->data, n);
                slot->user_identity_len = (opcua_byte_t)n;
            }
        }
#endif
        if (activate_result == MU_STATUS_GOOD && server->config.time_adapter.get_tick_ms != NULL) {
            slot->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
#if defined(MUC_OPCUA_CU_SESSION_TIMEOUT)
            server->next_session_timeout_ms = 0;
#endif
        }
    }
    return activate_result;
}

opcua_statuscode_t handle_activate_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t algorithm;
    mu_bytestring_t signature;
    s = mu_binary_read_string(r, &algorithm);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_read_bytestring(r, &signature);
    if (s != MU_STATUS_GOOD)
        return s;

    s = decode_client_software_certs_and_locales(r);
    if (s != MU_STATUS_GOOD)
        return s;

#ifdef MUC_OPCUA_CU_USER_AUTH
    mu_username_identity_token_t user_token = {{-1, NULL}, {-1, NULL}, {-1, NULL}, {-1, NULL}};
#endif
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
    mu_certificate_identity_token_t cert_token = {{-1, NULL}, {-1, NULL}};
#endif
    mu_string_t anon_policy_id = {-1, NULL};
    opcua_uint32_t token_type_numeric = 0u;

    s = decode_user_identity_token(r, &token_type_numeric, &anon_policy_id
#ifdef MUC_OPCUA_CU_USER_AUTH
                                   ,
                                   &user_token
#endif
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
                                   ,
                                   &cert_token
#endif
    );
    if (s != MU_STATUS_GOOD)
        return s;

    mu_string_t user_token_algorithm = {-1, NULL};
    mu_bytestring_t user_token_signature = {-1, NULL};
    if (r->position < r->length) {
        s = mu_binary_read_string(r, &user_token_algorithm);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_read_bytestring(r, &user_token_signature);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    opcua_statuscode_t activate_result =
        verify_and_activate_session(server, &req, &algorithm, &signature, token_type_numeric, &anon_policy_id
#ifdef MUC_OPCUA_CU_USER_AUTH
                                    ,
                                    &user_token
#endif
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
                                    ,
                                    &cert_token
#endif
                                    ,
                                    &user_token_signature);

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    s = fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    if (s != MU_STATUS_GOOD)
        return s;

    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};
    s = write_response_prefix(w, MU_ID_ACTIVATESESSIONRESPONSE, req.request_handle, activate_result
#ifdef MUC_OPCUA_CU_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD)
        goto cleanup_nonce;

    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD)
        goto cleanup_nonce;
    mu_binary_write_int32(w, 0);
    mu_binary_write_int32(w, 0);
    if (w->status != MU_STATUS_GOOD) {
        s = w->status;
        goto cleanup_nonce;
    }

    if (activate_result == MU_STATUS_GOOD && req.authentication_token.identifier_type == MU_NODEID_NUMERIC &&
        req.authentication_token.namespace_index == 0u) {
        mu_session_t *slot = mu_session_find_by_token(server->sessions, MU_INTERN_MAX_SESSIONS,
                                                      req.authentication_token.identifier.numeric);
        if (slot != NULL) {
            (void)memcpy(slot->server_nonce, nonce_buf, sizeof(slot->server_nonce));
        }
    }

    *response_length = w->position;
    s = MU_STATUS_GOOD;

#if MUC_OPCUA_CU_AUDITING
    /* spec 074: emit an AuditActivateSessionEvent (i=2075) for the successful
       ActivateSession (OPC-10000-5 §6.4.10). SessionId population + failure-path
       auditing (Status=false) are documented follow-ups. No-op unless auditing
       is enabled. */
    {
        mu_audit_event_t audit_ev;
        (void)memset(&audit_ev, 0, sizeof(audit_ev));
        audit_ev.event_type = MU_AUDIT_EVENT_ACTIVATE_SESSION;
        audit_ev.status = true;
        mu_raise_audit_event(server, &audit_ev);
    }
#endif

cleanup_nonce:
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
    mu_secure_zero(nonce_buf, sizeof(nonce_buf));
#endif
    return s;
}
