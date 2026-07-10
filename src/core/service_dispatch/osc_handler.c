#include "common.h"
#ifdef MUC_OPCUA_ECC
#include "../../security/asym_ecc.h"
#endif

#ifdef MUC_OPCUA_TIME_SYNC
bool mu_opn_time_sync_allows(opcua_datetime_t server_time, opcua_datetime_t client_time) {
    /* FR-003: skip when either peer reports no time (0 = unknown / clockless). */
    if (server_time == 0 || client_time == 0) {
        return true;
    }
    /* opcua_datetime_t is 100ns ticks; convert the ms window to ticks (*10000).
     * Realistic timestamps (~1.3e17 ticks) keep the difference well inside int64. */
    const opcua_int64_t max_ticks = (opcua_int64_t)MU_TIME_SYNC_MAX_CLOCK_SKEW_MS * 10000LL;
    opcua_int64_t diff = server_time - client_time;
    if (diff < 0) {
        diff = -diff;
    }
    return diff <= max_ticks;
}
#endif

static opcua_statuscode_t validate_client_nonce(const mu_bytestring_t *client_nonce) {
    if (client_nonce->length > 0 && (client_nonce->length < 32 || client_nonce->length > 128)) {
        return MU_STATUS_BAD_NONCEINVALID;
    }
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_SECURITY
static opcua_statuscode_t enforce_osc_application_auth(const mu_server_t *server) {
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID) {
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
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t encode_osc_security_token(mu_binary_writer_t *w, opcua_uint32_t request_handle,
                                                    opcua_uint32_t revised, const opcua_byte_t *nonce_buf,
                                                    size_t nonce_len, const mu_server_t *server) {
    opcua_statuscode_t s = write_response_prefix(w, MU_ID_OPENSECURECHANNELRESPONSE, request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                                                 ,
                                                 server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_datetime_t now = server->config.time_adapter.get_time
                               ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                               : 0;

    mu_binary_write_uint32(w, MU_OPC_UA_TCP_PROTOCOL_VERSION);
    mu_binary_write_uint32(w, server_secure_channel.channel_id);
    mu_binary_write_uint32(w, server_secure_channel.token_id);
    mu_binary_write_int64(w, now);
    mu_binary_write_uint32(w, revised);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }

    mu_bytestring_t server_nonce = {(opcua_int32_t)nonce_len, (opcua_byte_t *)nonce_buf};
    return mu_binary_write_bytestring(w, &server_nonce);
}

opcua_statuscode_t handle_open_secure_channel(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

#ifdef MUC_OPCUA_TIME_SYNC
    /* Spec 055: reject channel establishment when the client's UTC timestamp
     * (RequestHeader.timestamp, OPC-10000-4 §7.28) drifts beyond the configured
     * clock skew from server time -- anti-replay / freshness. */
    {
        opcua_datetime_t server_time = server->config.time_adapter.get_time
                                           ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                                           : 0;
        if (!mu_opn_time_sync_allows(server_time, req.timestamp)) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
    }
#endif

    opcua_uint32_t client_proto, request_type, security_mode, requested_lifetime;
    mu_bytestring_t client_nonce;
    mu_binary_read_uint32(r, &client_proto);
    mu_binary_read_uint32(r, &request_type);
    mu_binary_read_uint32(r, &security_mode);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    if (request_type > 1) {
        return MU_STATUS_BAD_REQUESTTYPEINVALID;
    }
    s = mu_binary_read_bytestring(r, &client_nonce);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = validate_client_nonce(&client_nonce);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint32(r, &requested_lifetime);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    opcua_uint32_t revised = 0;

    s = mu_secure_channel_open(&server_secure_channel, current_opn_security_policy(server),
                               (mu_message_security_mode_t)security_mode, requested_lifetime,
                               &server->config.entropy_adapter, &revised);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

#ifdef MUC_OPCUA_SECURITY
    s = enforce_osc_application_auth(server);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#endif

    /* ServerNonce: 32 random bytes for None/RSA; for the ECC SecurityPolicies
       (spec 059) it is the server's ephemeral public key (<= 64 B) — the peers'
       ephemeral ECDH pubkeys travel as the Client/ServerNonce (OPC-10000-6 §6.8.1). */
#ifdef MUC_OPCUA_ECC
    opcua_byte_t nonce_buf[MU_ECC_MAX_PUBKEY_LENGTH];
    opcua_byte_t ecc_keypair_ctx[MU_ECC_KEYPAIR_CTX_SIZE];
    mu_ecc_curve_t ecc_curve = MU_ECC_CURVE_25519;
    const bool ecc_channel = server->config.crypto_adapter != NULL &&
                             mu_security_policy_asym_family(server_secure_channel.policy) == MU_ASYM_FAMILY_ECC;
#else
    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
#endif
    size_t nonce_len = MU_SERVER_NONCE_LENGTH;

#ifdef MUC_OPCUA_ECC
    if (ecc_channel) {
        const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
        if (cr->ecc_generate_ephemeral == NULL || cr->ecc_ecdh_derive == NULL ||
            !mu_security_policy_ecc_curve(server_secure_channel.policy, &ecc_curve)) {
            return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
        }
        nonce_len = sizeof(nonce_buf);
        s = cr->ecc_generate_ephemeral(cr->context, ecc_curve, nonce_buf, &nonce_len, ecc_keypair_ctx);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        /* The ClientNonce must be the peer's ephemeral pubkey at the curve's exact size. */
        if (nonce_len != mu_security_policy_nonce_length(server_secure_channel.policy) ||
            client_nonce.length != (opcua_int32_t)nonce_len) {
            return MU_STATUS_BAD_NONCEINVALID;
        }
    } else
#endif
    {
        nonce_len = MU_SERVER_NONCE_LENGTH;
        if (server->config.entropy_adapter.generate_random == NULL ||
            server->config.entropy_adapter.generate_random(server->config.entropy_adapter.context, nonce_buf,
                                                           nonce_len) != MU_STATUS_GOOD) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
    }

    s = encode_osc_security_token(w, req.request_handle, revised, nonce_buf, nonce_len, server);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    server_secure_channel.mode = (mu_message_security_mode_t)security_mode;
#ifdef MUC_OPCUA_SECURITY
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID && server->config.crypto_adapter != NULL) {
        const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
        size_t cn_len = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
        if (cn_len == 0) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
#ifdef MUC_OPCUA_ECC
        if (ecc_channel) {
            /* ECDH(server ephemeral private, client ephemeral pubkey) -> shared
               secret; HKDF the per-direction channel keys from it (§6.8.1). */
            opcua_byte_t shared[MU_ECC_MAX_PUBKEY_LENGTH];
            size_t shared_len = sizeof(shared);
            s = cr->ecc_ecdh_derive(cr->context, ecc_curve, ecc_keypair_ctx, client_nonce.data, cn_len, shared,
                                    &shared_len);
            if (cr->ecc_keypair_free != NULL) {
                cr->ecc_keypair_free(cr->context, ecc_keypair_ctx);
            }
            if (s != MU_STATUS_GOOD) {
                mu_secure_zero(shared, sizeof(shared));
                return s;
            }
            /* server_keys protect Server->Client (ServerSalt, is_server_direction=1);
               client_keys protect the inbound Client->Server (ClientSalt, =0). */
            s = mu_ecc_derive_channel_keys(cr, server_secure_channel.policy, shared, shared_len, 1, nonce_buf,
                                           nonce_len, client_nonce.data, cn_len, &server_secure_channel.server_keys);
            if (s == MU_STATUS_GOOD) {
                s = mu_ecc_derive_channel_keys(cr, server_secure_channel.policy, shared, shared_len, 0, nonce_buf,
                                               nonce_len, client_nonce.data, cn_len,
                                               &server_secure_channel.client_keys);
            }
            mu_secure_zero(shared, sizeof(shared));
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        } else
#endif
        {
            s = mu_sym_keys_derive(cr, server_secure_channel.policy, nonce_buf, nonce_len, client_nonce.data, cn_len,
                                   &server_secure_channel.client_keys);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            s = mu_sym_keys_derive(cr, server_secure_channel.policy, client_nonce.data, cn_len, nonce_buf, nonce_len,
                                   &server_secure_channel.server_keys);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
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

    s = write_response_prefix(w, MU_ID_CLOSESECURECHANNELRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
