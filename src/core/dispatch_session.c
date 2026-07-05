/* src/core/dispatch_session.c
 * Session service handlers extracted from service_dispatch.c (T008).
 * Implements CreateSession (OPC-10000-4 5.7.2), ActivateSession (5.7.3),
 * and CloseSession (5.7.4). Session services are always compiled. */
#include "../services/discovery.h"
#include "../services/secure_channel.h"
#include "../services/service_header.h"
#include "../services/session.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_dispatch.h"
#ifdef MUC_OPCUA_SECURITY
#include "../security/certificate.h"
#include "../security/key_derivation.h"
#include "../security/security_policy.h"
#include "../security/sym_chunk.h"
#include "muc_opcua/security.h"
#endif
#include <stddef.h>
#include <string.h>

#define MU_SERVER_NONCE_LENGTH 32

/* Fill a ServerNonce from the entropy adapter. Fails closed: returns the
   error code on entropy failure so callers can reject the request. OPC-10000-4
   S5.7.3, S7.38.2. */
static opcua_statuscode_t fill_server_nonce(mu_server_t *server, opcua_byte_t *nonce, size_t len) {
    return mu_session_generate_server_nonce(&server->config.entropy_adapter, nonce, len);
}

#ifdef MUC_OPCUA_SECURITY
/* OPC-10000-4 §5.7.3 / §7.38.2: on a Sign or SignAndEncrypt SecureChannel the
   client MUST prove possession of its application-instance private key by signing
   (serverCertificate || serverNonce) with RSA-SHA256. The server verifies that
   ClientSignature against the certificate the client presented when it opened the
   SecureChannel, which binds activation to the fresh per-session ServerNonce
   (anti-replay). SecurityPolicy None carries a null signature and is exempt. */
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
    /* Feature 026 (FR-005): the ClientSignature's declared algorithm URI MUST match
       the negotiated policy's signature algorithm (OPC-10000-7). Exact length+bytes
       compare; a short/absent/mismatched URI is rejected before verification. */
    const char *expected_uri = mu_security_policy_asym_signature_uri(server_secure_channel.policy);
    if (expected_uri == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    size_t expected_uri_len = strlen(expected_uri);
    if (algorithm == NULL || algorithm->data == NULL || algorithm->length < 0 ||
        (size_t)algorithm->length != expected_uri_len || memcmp(algorithm->data, expected_uri, expected_uri_len) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    /* The client's application-instance certificate, persisted from OpenSecureChannel
       (the OPN sender cert points into transient receive-buffer memory). */
    if (server->channel_client_cert_len == 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    const opcua_byte_t *own_cert = NULL;
    size_t own_cert_len = 0;
    if (cr->get_own_certificate(cr->context, &own_cert, &own_cert_len) != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    /* verify_data = serverCertificate || serverNonce (sized as the existing
       user-token verify path: server cert plus the 32-byte nonce). */
    opcua_byte_t verify_buf[1536];
    if (own_cert_len + sizeof(slot->server_nonce) > sizeof(verify_buf)) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    memcpy(verify_buf, own_cert, own_cert_len);
    memcpy(verify_buf + own_cert_len, slot->server_nonce, sizeof(slot->server_nonce));
    /* Feature 026: verify under the policy's scheme (PKCS#1.5 or PSS). */
    opcua_statuscode_t vs = mu_asym_signature_verify(
        cr, server_secure_channel.policy, server->channel_client_cert, server->channel_client_cert_len, verify_buf,
        own_cert_len + sizeof(slot->server_nonce), signature->data, (size_t)signature->length);
    return (vs == MU_STATUS_GOOD) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}
#endif

/* Decode a CreateSessionRequest body, capturing the ApplicationUri (the first
   field of the ClientDescription), ClientNonce, ClientCertificate, and
   RequestedSessionTimeout (the latter two are needed to compute the
   ServerSignature on a secured channel). */
static opcua_statuscode_t read_create_session_request(mu_binary_reader_t *r, mu_string_t *application_uri,
                                                      opcua_uint64_t *timeout_bits, mu_bytestring_t *client_nonce,
                                                      mu_bytestring_t *client_cert) {
    mu_string_t str;
    opcua_uint32_t u;
    opcua_int32_t n;
    opcua_byte_t mask;
    opcua_statuscode_t status;

    *timeout_bits = 0; /* below the minimum -> clamped up */
    application_uri->length = -1;
    application_uri->data = NULL;
    client_nonce->length = -1;
    client_nonce->data = NULL;
    client_cert->length = -1;
    client_cert->data = NULL;

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
    /* OPC-10000-4 §5.7.2.3 Table 16: the ClientNonce is optional (null when
       the SecurityPolicy is None), but when present its length shall be in the
       32-128 byte range. A non-null nonce outside that range is rejected with
       Bad_NonceInvalid. A null (-1) or empty (0) nonce is permitted. */
    if (client_nonce->length > 0 && (client_nonce->length < 32 || client_nonce->length > 128)) {
        return MU_STATUS_BAD_NONCEINVALID;
    }
    status = mu_binary_read_bytestring(r, client_cert);
    if (status != MU_STATUS_GOOD) {
        return status; /* ClientCertificate */
    }
    {
        /* RequestedSessionTimeout is a Duration (Double); keep its raw bits and
           clamp by integer compare to avoid soft-float on the no-FPU target. */
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

/* CreateSession (OPC 10000-4 5.7.2.2): validate request parameters before
   creating session state, then emit a structurally valid CreateSessionResponse. */
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
    /* OPC-10000-4 section 7.38.2: Bad_DecodingError covers invalid stream data;
       reject incomplete mandatory service bodies before session state changes. */
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    /* OPC-10000-4 section 5.7.2: the complete CreateSession body is mandatory. */
    if (r->position != r->length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    /* NOTE: OPC-10000-4 §5.7.2.2 Table 15 states that the CreateSession serverUri
       is "no longer used ... the Server shall ignore any value provided" and the
       endpointUrl is "diagnostics" only ("The Server should return a suitable
       default URL if it does not recognize the HostName"). The Server is
       therefore REQUIRED to accept any serverUri/endpointUrl and must NOT reject
       on mismatch. Bad_ServerUriInvalid (defined in status.h at 0x804F0000)
       applies to the FindServers/RegisterServer services (§5.5.5.3), where
       serverUri identifies a registered server, not to CreateSession. T007
       originally requested CreateSession validation here, but the current spec
       text prohibits it; see TODO.md for the grounding discrepancy. */

#ifdef MUC_OPCUA_SECURITY
    /* OPC-10000-4 §5.6.2: on a secured channel the ClientCertificate presented in
       CreateSession MUST be the same application-instance certificate that opened
       the SecureChannel. Binding them lets ActivateSession verify the
       ClientSignature against the trusted channel certificate. */
    if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
        server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID) {
        if (client_cert.length <= 0 || (size_t)client_cert.length != server->channel_client_cert_len ||
            server->channel_client_cert_len == 0 ||
            memcmp(client_cert.data, server->channel_client_cert, server->channel_client_cert_len) != 0) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        /* OPC-10000-4 §5.7.2.1: the ApplicationUri in the ClientDescription MUST
           match an identity in the ClientCertificate (SubjectAltName URI, falling
           back to Subject CN). SecurityPolicy=None and a null/empty
           ApplicationUri or ClientCertificate skip the check (validated in the
           wrapper), preserving backward compatibility with test clients that
           omit the ApplicationUri. On mismatch CreateSession returns
           Bad_CertificateUriInvalid. */
        opcua_statuscode_t au_status = mu_certificate_validate_application_uri(
            server->config.crypto_adapter, server_secure_channel.policy, client_cert.data, (size_t)client_cert.length,
            (const char *)application_uri.data, (size_t)(application_uri.length > 0 ? application_uri.length : 0));
        if (au_status != MU_STATUS_GOOD) {
            return au_status;
        }
    }
#endif

    opcua_uint64_t revised_bits = 0;
    opcua_uint32_t session_id = 0, auth_token = 0;
    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];

    /* OPC-10000-4 sections 5.7.2.3 and 7.38.2: CreateSession ServerNonce
       freshness is a security check; fail closed before allocating session state. */
    s = mu_session_generate_server_nonce(&server->config.entropy_adapter, nonce_buf, sizeof(nonce_buf));
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_session_t *slot = mu_session_find_free(server->sessions, MU_MAX_SESSIONS);
    if (slot == NULL) {
        return MU_STATUS_BAD_TOOMANYSESSIONS;
    }

    s = mu_session_generate_session_id(server->sessions, MU_MAX_SESSIONS, &server->config.entropy_adapter, &session_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_session_generate_authentication_token(server->sessions, MU_MAX_SESSIONS, &server->config.entropy_adapter,
                                                 &auth_token);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* OPC-10000-4 §5.7.3: emit the response prefix BEFORE allocating session
       state. A prefix encode failure returns before any session is created,
       so no cleanup is required. Body-field encode failures after the session
       slot has been transitioned to MU_SESSION_STATE_CREATED are routed to
       the cleanup path at the end of this function so the slot is not leaked
       (§5.7.3 — response encoding failure must not leak a session slot). */
    s = write_response_prefix(w, MU_ID_CREATESESSIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool session_created = false;
    s = mu_session_create_with_identifiers(slot, requested_bits, session_id, auth_token,
                                           server_secure_channel.channel_id, &revised_bits, &session_id, &auth_token);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    session_created = true;

    /* OPC-10000-4 section 5.7.2.1: seed last_activity_ms at creation so the
       SessionTimeout idle window starts from CreateSession, not from the first
       subsequent service request. Guard get_tick_ms for white-box tests that
       dispatch without a full mu_server_init. */
    if (server->config.time_adapter.get_tick_ms != NULL) {
        slot->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
#if defined(MUC_OPCUA_SESSION_TIMEOUT)
        server->next_session_timeout_ms = 0;
#endif
    }

    mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {slot->session_id}};
    mu_nodeid_t tok = {0, MU_NODEID_NUMERIC, {slot->auth_token}};
    mu_bytestring_t null_bs = {-1, NULL};
    mu_string_t null_str = {-1, NULL};

    memcpy(slot->server_nonce, nonce_buf, sizeof(nonce_buf));
    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};

    s = mu_binary_write_nodeid(w, &sid);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* SessionId */
    }
    s = mu_binary_write_nodeid(w, &tok);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* AuthenticationToken */
    }
    s = mu_binary_write_uint64(w, revised_bits);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* RevisedSessionTimeout (Double bits) */
    }
    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* ServerNonce */
    }
#ifdef MUC_OPCUA_SECURITY
    mu_secure_zero(nonce_buf, sizeof(nonce_buf));
#endif

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
        if (s != MU_STATUS_GOOD) {
            goto cleanup;
        }
    }

    /* ServerEndpoints: the same set a Client would get from GetEndpoints. */
    {
        mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
        size_t count = 0;
        s = mu_discovery_get_endpoints(&server->config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
        if (s != MU_STATUS_GOOD) {
            goto cleanup;
        }
        s = mu_binary_write_int32(w, (opcua_int32_t)count);
        if (s != MU_STATUS_GOOD) {
            goto cleanup; /* ServerEndpoints[] */
        }
        for (size_t i = 0; i < count; ++i) {
            s = mu_endpoint_description_encode(w, &eps[i]);
            if (s != MU_STATUS_GOOD) {
                goto cleanup;
            }
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* ServerSoftwareCertificates[] */
    }

    /* ServerSignature: on a secured channel, sign ClientCertificate || ClientNonce
       with the server's private key (RSA-PKCS1.5-SHA256). This proves the server
       holds the private key for its certificate (OPC 10000-4 5.6.2.2). On None it
       is the null/null signature. */
    {
        bool wrote_sig = false;
#ifdef MUC_OPCUA_SECURITY
        /* Feature 026: the signature scheme and SignatureData.algorithm URI follow
           the negotiated SecurityPolicy (PKCS#1.5 for Basic256Sha256/Aes128,
           RSA-PSS for Aes256_Sha256_RsaPss). mu_asym_signature_sign dispatches and
           fails closed if the policy's primitive is absent. */
        const char *sig_uri = mu_security_policy_asym_signature_uri(server_secure_channel.policy);
        if (server_secure_channel.policy != MU_SECURITY_POLICY_NONE_ID &&
            server_secure_channel.policy != MU_SECURITY_POLICY_INVALID_ID && server->config.crypto_adapter != NULL &&
            sig_uri != NULL && client_cert.length > 0) {
            const mu_crypto_adapter_t *cr = server->config.crypto_adapter;
            size_t cc = (size_t)client_cert.length;
            size_t cn = client_nonce.length > 0 ? (size_t)client_nonce.length : 0;
            opcua_byte_t to_sign[1536];
            if (cc + cn <= sizeof(to_sign)) {
                memcpy(to_sign, client_cert.data, cc);
                if (cn > 0) {
                    memcpy(to_sign + cc, client_nonce.data, cn);
                }
                opcua_byte_t sig[512];
                size_t sig_len = sizeof(sig);
                if (mu_asym_signature_sign(cr, server_secure_channel.policy, to_sign, cc + cn, sig, &sig_len) ==
                    MU_STATUS_GOOD) {
                    mu_string_t alg = {(opcua_int32_t)strlen(sig_uri), (const opcua_byte_t *)sig_uri};
                    mu_bytestring_t sig_bs = {(opcua_int32_t)sig_len, sig};
                    s = mu_binary_write_string(w, &alg);
                    if (s != MU_STATUS_GOOD) {
                        goto cleanup;
                    }
                    s = mu_binary_write_bytestring(w, &sig_bs);
                    if (s != MU_STATUS_GOOD) {
                        goto cleanup;
                    }
                    wrote_sig = true;
                }
            }
        }
#endif
        if (!wrote_sig) {
            s = mu_binary_write_string(w, &null_str);
            if (s != MU_STATUS_GOOD) {
                goto cleanup; /* ServerSignature.algorithm */
            }
            s = mu_binary_write_bytestring(w, &null_bs);
            if (s != MU_STATUS_GOOD) {
                goto cleanup; /* ServerSignature.signature */
            }
        }
    }

    s = mu_binary_write_uint32(w, 0);
    if (s != MU_STATUS_GOOD) {
        goto cleanup; /* MaxRequestMessageSize */
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;

cleanup:
    /* OPC-10000-4 §5.7.3: a response-body encode failure after the session
       slot was allocated must not leak the slot. Guarded by session_created so
       a prefix encode failure (which returns before session creation) does not
       attempt to close a slot that was never transitioned to CREATED, avoiding
       an invalid/double close. */
    if (session_created) {
        (void)mu_session_close(slot, auth_token, true);
    }
    return s;
}

static opcua_statuscode_t handle_activate_anonymous(mu_server_t *server, const mu_string_t *anon_policy_id) {
    if (server->config.user_auth_handler != NULL) {
        return server->config.user_auth_handler(server->config.user_auth_handler_handle, NULL, NULL, anon_policy_id);
    }
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_USER_AUTH
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
            /* OPC-10000-4 §7.40.2.2 Table 182: the legacy
               encrypted token Length covers the password
               bytes plus the 32-byte ServerNonce, excluding
               the Length field itself. */
            if (secret_len < 32 || (size_t)secret_len != (out_len - 4u)) {
                result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
                goto cleanup;
            }
            size_t actual_pw_len = (size_t)secret_len - 32u;
            decrypted_password.length = (opcua_int32_t)actual_pw_len;
            decrypted_password.data = decrypt_buf + 4;

            /* Verify server nonce (prevent replay attacks, OPC-10000-4 §5.6.3.2) */
            size_t nonce_offset = 4u + actual_pw_len;
            size_t nonce_len = out_len - nonce_offset;
            /* Compare the returned ServerNonce (feature 025 F10
               anti-replay). memcmp is appropriate here: the
               ServerNonce is sent in the clear in the
               CreateSession response, so it is not a secret and
               needs no constant-time compare (unlike the MAC/key
               comparisons that use mu_secure_memeq). Using
               mu_secure_memeq would also pull security-only code
               into the USER_AUTH-without-SECURITY (micro) build. */
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
        /* Secure by default: reject if username token type accepted but no handler configured */
        result = MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

cleanup:
#ifdef MUC_OPCUA_SECURITY
    mu_secure_zero(decrypt_buf, sizeof(decrypt_buf));
#endif
    return result;
}
#endif /* MUC_OPCUA_USER_AUTH */

#ifdef MUC_OPCUA_SECURITY
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
#endif /* MUC_OPCUA_SECURITY */

/* ActivateSession (OPC-10000-4 section 5.7.3.2). The UserIdentityToken's ExtensionObject
   typeId identifies the token type; only Anonymous (i=321) is accepted. A service
   failure is reported in the ResponseHeader.serviceResult, not as a transport error. */
opcua_statuscode_t handle_activate_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* ClientSignature: SignatureData { algorithm(String), signature(ByteString) } */
    mu_string_t algorithm;
    mu_bytestring_t signature;
    s = mu_binary_read_string(r, &algorithm);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_read_bytestring(r, &signature);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* OPC-10000-4 section 5.7.3.2: clientSoftwareCertificates[] is reserved for
       future use, but every SignedSoftwareCertificate must still be consumed. */
    opcua_int32_t cert_count;
    s = mu_binary_read_int32(r, &cert_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = ensure_array_items_min_remaining(r, cert_count, 8u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < cert_count; ++i) {
        mu_bytestring_t certificate_data;
        mu_bytestring_t certificate_signature;
        s = mu_binary_read_bytestring(r, &certificate_data);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_read_bytestring(r, &certificate_signature);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    /* OPC-10000-4 section 5.7.3.2: localeIds[] follows clientSoftwareCertificates[]. */
    opcua_int32_t locale_count;
    s = mu_binary_read_int32(r, &locale_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = ensure_array_items_min_remaining(r, locale_count, 4u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < locale_count; ++i) {
        mu_string_t locale;
        s = mu_binary_read_string(r, &locale);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    /* OPC-10000-4 section 5.7.3.2: userIdentityToken ExtensionObject precedes
       userTokenSignature; consume the body at its encoded length for alignment. */
    mu_nodeid_t token_type;
    size_t token_body_len;
    s = mu_binary_read_extension_object_header(r, &token_type, &token_body_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    bool token_type_is_ns0_numeric =
        token_type.identifier_type == MU_NODEID_NUMERIC && token_type.namespace_index == 0u;
#ifdef MUC_OPCUA_USER_AUTH
    mu_username_identity_token_t user_token = {{-1, NULL}, {-1, NULL}, {-1, NULL}, {-1, NULL}};
#endif
#ifdef MUC_OPCUA_SECURITY
    mu_certificate_identity_token_t cert_token = {{-1, NULL}, {-1, NULL}};
#endif
    mu_string_t anon_policy_id = {-1, NULL};
    mu_string_t user_token_algorithm = {-1, NULL};
    mu_bytestring_t user_token_signature = {-1, NULL};

    if (token_type_is_ns0_numeric && token_type.identifier.numeric == 321) {
        /* Anonymous: body is just policyId (String) */
        s = ensure_reader_bytes_remaining(r, token_body_len);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (token_body_len > 0) {
            mu_binary_reader_t sub;
            mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
            /* OPC-10000-4 section 5.7.3 and OPC-10000-6 section 5.2.2.15:
               decode only inside the userIdentityToken ExtensionObject body. */
            s = mu_binary_read_string(&sub, &anon_policy_id);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        r->position += token_body_len;
    } else if (token_type_is_ns0_numeric && token_type.identifier.numeric == 324) {
#ifdef MUC_OPCUA_USER_AUTH
        s = ensure_reader_bytes_remaining(r, token_body_len);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (token_body_len > 0) {
            mu_binary_reader_t sub;
            mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
            /* OPC-10000-4 section 5.7.3 and OPC-10000-6 section 5.2.2.15:
               decode only inside the userIdentityToken ExtensionObject body. */
            s = mu_binary_read_username_identity_token(&sub, &user_token);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        r->position += token_body_len;
#else
        s = skip_extension_object_body(r, token_body_len);
        if (s != MU_STATUS_GOOD)
            return s;
#endif
#ifdef MUC_OPCUA_SECURITY
    } else if (token_type_is_ns0_numeric && token_type.identifier.numeric == 327) {
        s = ensure_reader_bytes_remaining(r, token_body_len);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (token_body_len > 0) {
            mu_binary_reader_t sub;
            mu_binary_reader_init(&sub, r->buffer + r->position, token_body_len);
            /* OPC-10000-4 section 5.7.3 and OPC-10000-6 section 5.2.2.15:
               decode only inside the userIdentityToken ExtensionObject body. */
            s = mu_binary_read_certificate_identity_token(&sub, &cert_token);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        r->position += token_body_len;
#endif
    } else {
        s = skip_extension_object_body(r, token_body_len);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    /* UserTokenSignature: SignatureData { algorithm(String), signature(ByteString) }.
       OPC-10000-4 section 5.7.3.2 places it last, so omitted trailing bytes do not
       misalign any following field. If bytes remain, they must still decode. */
    if (r->position < r->length) {
        s = mu_binary_read_string(r, &user_token_algorithm);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_read_bytestring(r, &user_token_signature);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    opcua_uint32_t auth_token = 0u;
    opcua_statuscode_t activate_result = MU_STATUS_BAD_SESSIONIDINVALID;
    if (req.authentication_token.identifier_type == MU_NODEID_NUMERIC &&
        req.authentication_token.namespace_index == 0u) {
        auth_token = req.authentication_token.identifier.numeric;
        mu_session_t *slot = mu_session_find_by_token(server->sessions, MU_MAX_SESSIONS, auth_token);
        if (slot != NULL) {
            activate_result = mu_session_validate_secure_channel(slot, server_secure_channel.channel_id);
            if (activate_result != MU_STATUS_GOOD) {
                /* Keep the Session in CREATED state when activation arrives on
                   the wrong SecureChannel. */
            } else if (!token_type_is_ns0_numeric) {
                activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
            } else {
                opcua_statuscode_t client_sig_result = MU_STATUS_GOOD;
#ifdef MUC_OPCUA_SECURITY
                /* OPC-10000-4 §5.7.3/§7.38.2: verify the application ClientSignature
                   over (serverCertificate || serverNonce) before honoring any
                   identity token. Rejected activations leave the Session in CREATED. */
                client_sig_result = verify_activate_client_signature(server, slot, &algorithm, &signature);
#endif
                if (client_sig_result != MU_STATUS_GOOD) {
                    activate_result = client_sig_result;
                } else if (token_type.identifier.numeric == 321) {
                    activate_result = handle_activate_anonymous(server, &anon_policy_id);
                } else if (token_type.identifier.numeric == 324) {
#ifdef MUC_OPCUA_USER_AUTH
                    activate_result = handle_activate_username(server, slot, &user_token);
#else
                    activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
#endif
#ifdef MUC_OPCUA_SECURITY
                } else if (token_type.identifier.numeric == 327) {
                    activate_result = handle_activate_certificate(server, slot, &cert_token, &user_token_signature);
#endif
                } else {
                    activate_result = MU_STATUS_BAD_IDENTITYTOKENINVALID;
                }

                if (activate_result == MU_STATUS_GOOD) {
                    activate_result = mu_session_activate(slot, auth_token, token_type.identifier.numeric);
                    /* OPC-10000-4 section 5.7.2.1: ActivateSession is a service
                       request on the Session, so refresh the idle timer. */
                    if (activate_result == MU_STATUS_GOOD && server->config.time_adapter.get_tick_ms != NULL) {
                        slot->last_activity_ms =
                            server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
#if defined(MUC_OPCUA_SESSION_TIMEOUT)
                        server->next_session_timeout_ms = 0;
#endif
                    }
                }
            }
        }
    }

    opcua_byte_t nonce_buf[MU_SERVER_NONCE_LENGTH];
    s = fill_server_nonce(server, nonce_buf, sizeof(nonce_buf));
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_bytestring_t server_nonce = {(opcua_int32_t)sizeof(nonce_buf), nonce_buf};

    s = write_response_prefix(w, MU_ID_ACTIVATESESSIONRESPONSE, req.request_handle, activate_result);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_bytestring(w, &server_nonce);
    if (s != MU_STATUS_GOOD) {
        return s; /* ServerNonce */
    }
#ifdef MUC_OPCUA_SECURITY
    mu_secure_zero(nonce_buf, sizeof(nonce_buf));
#endif
    mu_binary_write_int32(w, 0); /* Results[] */
    mu_binary_write_int32(w, 0); /* DiagnosticInfos[] */
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* CloseSession (OPC 10000-4 5.7.4.2). */
opcua_statuscode_t handle_close_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_boolean_t delete_subscriptions;
    s = mu_binary_read_boolean(r, &delete_subscriptions);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_statuscode_t close_result = MU_STATUS_BAD_SESSIONIDINVALID;
    if (server->active_session != NULL && req.authentication_token.identifier_type == MU_NODEID_NUMERIC) {
        close_result =
            mu_session_close(server->active_session, req.authentication_token.identifier.numeric, delete_subscriptions);
#if MUC_OPCUA_SUBSCRIPTIONS
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
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
