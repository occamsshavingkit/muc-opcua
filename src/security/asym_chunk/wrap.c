/* src/security/asym_chunk/wrap.c
 * mu_asym_chunk_wrap and its helpers. */
#include "common.h"

/* Write the cleartext OPN header: "OPNF", a zero MessageSize placeholder,
   SecureChannelId, then the AsymmetricAlgorithmSecurityHeader. */
static opcua_statuscode_t write_header(mu_binary_writer_t *w, mu_security_policy_id_t policy, opcua_uint32_t scid,
                                       const opcua_byte_t *sender_cert, size_t sender_cert_len,
                                       const opcua_byte_t *receiver_thumbprint, size_t thumb_len) {
    const char *uri = mu_security_policy_uri(policy);
    if (!uri) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }

    w->buffer[0] = 'O';
    w->buffer[1] = 'P';
    w->buffer[2] = 'N';
    w->buffer[3] = 'F';
    w->position = 4;
    opcua_statuscode_t s = mu_binary_write_uint32(w, 0); /* MessageSize placeholder */
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_uint32(w, scid);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t policy_uri = {mu_safe_int32_from_size_t(strlen(uri)), (const opcua_byte_t *)uri};
    s = mu_binary_write_string(w, &policy_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_bytestring_t cert = {
        sender_cert ? (opcua_int32_t)(sender_cert_len > (size_t)INT32_MAX ? INT32_MAX : sender_cert_len) : -1,
        sender_cert};
    s = mu_binary_write_bytestring(w, &cert);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_bytestring_t thumb = {
        receiver_thumbprint ? (opcua_int32_t)(thumb_len > (size_t)INT32_MAX ? INT32_MAX : thumb_len) : -1,
        receiver_thumbprint};
    return mu_binary_write_bytestring(w, &thumb);
}

/* --- None: cleartext framing --- */

static opcua_statuscode_t wrap_none(opcua_uint32_t scid, opcua_uint32_t seq, opcua_uint32_t rid,
                                    const opcua_byte_t *body, size_t body_len, opcua_byte_t *out, size_t out_cap,
                                    size_t *out_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, out_cap);
    opcua_statuscode_t s = write_header(&w, MU_SECURITY_POLICY_NONE_ID, scid, NULL, 0, NULL, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_uint32(&w, seq);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_uint32(&w, rid);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (!mu_checked_memcpy_off(out, out_cap, w.position, body, body_len)) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    size_t total = w.position + body_len;
    /* Patch MessageSize. */
    out[4] = (opcua_byte_t)(total);
    out[5] = (opcua_byte_t)(total >> 8);
    out[6] = (opcua_byte_t)(total >> 16);
    out[7] = (opcua_byte_t)(total >> 24);
    *out_len = total;
    return MU_STATUS_GOOD;
}

/* --- Helpers for mu_asym_chunk_wrap --- */

static opcua_statuscode_t validate_wrap_policy(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                               const opcua_byte_t *receiver_cert, size_t receiver_cert_len) {
    if (policy != MU_SECURITY_POLICY_BASIC256SHA256_ID && policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID &&
        policy != MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }
    if (!crypto || !receiver_cert || receiver_cert_len == 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t build_wrap_header(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                            opcua_uint32_t secure_channel_id, const opcua_byte_t *receiver_cert,
                                            size_t receiver_cert_len, opcua_byte_t *out, size_t out_cap,
                                            opcua_byte_t *thumb, size_t thumb_cap, size_t *hdr_len, size_t *sig_len,
                                            size_t *plain_block, size_t *cipher_block) {
    const opcua_byte_t *sender_cert = NULL;
    size_t sender_cert_len = 0;
    opcua_statuscode_t s = crypto->get_own_certificate(crypto->context, &sender_cert, &sender_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t recv_key_bytes = 0;
    s = key_bytes(crypto, sender_cert, sender_cert_len, sig_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = key_bytes(crypto, receiver_cert, receiver_cert_len, &recv_key_bytes);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    *plain_block = (policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) ? (recv_key_bytes - 66)
                                                                          : (recv_key_bytes - MU_OAEP_SHA1_OVERHEAD);
    *cipher_block = recv_key_bytes;

    s = mu_certificate_thumbprint(crypto, receiver_cert, receiver_cert_len, thumb);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, out_cap);
    s = write_header(&w, policy, secure_channel_id, sender_cert, sender_cert_len, thumb, thumb_cap);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    *hdr_len = w.position;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t compute_wrap_sizes(size_t body_len, size_t sig_len, size_t plain_block, size_t cipher_block,
                                             size_t hdr_len, size_t out_cap, size_t *presig_len, size_t *enc_len,
                                             size_t *cipher_len, size_t *total) {
    size_t seqbody_len = 8 + body_len;
    size_t base = seqbody_len + 1 + sig_len;
    size_t pad_count = (plain_block - (base % plain_block)) % plain_block;
    *enc_len = base + pad_count;
    *presig_len = seqbody_len + 1 + pad_count;
    *cipher_len = (*enc_len / plain_block) * cipher_block;
    *total = hdr_len + *cipher_len;

    if (*enc_len > MU_ASYM_MAX_PLAINTEXT) {
        return MU_STATUS_BAD_REQUESTTOOLARGE;
    }
    if (hdr_len + *presig_len > MU_ASYM_SIGNED_INPUT_MAX) {
        return MU_STATUS_BAD_REQUESTTOOLARGE;
    }
    if (*total > out_cap) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t sign_wrap_payload(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                            opcua_uint32_t sequence_number, opcua_uint32_t request_id,
                                            const opcua_byte_t *body, size_t body_len, opcua_byte_t *plain,
                                            size_t plain_cap, opcua_byte_t *out, size_t hdr_len, size_t presig_len,
                                            size_t sig_len) {
    mu_binary_writer_t pw;
    mu_binary_writer_init(&pw, plain, plain_cap);
    opcua_statuscode_t s = mu_binary_write_uint32(&pw, sequence_number);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(plain, plain_cap);
        return s;
    }
    s = mu_binary_write_uint32(&pw, request_id);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(plain, plain_cap);
        return s;
    }

    size_t seqbody_len = 8 + body_len;
    size_t pad_count = presig_len - seqbody_len - 1;
    (void)memcpy(plain + 8, body, body_len);
    plain[seqbody_len] = (opcua_byte_t)pad_count;
    (void)memset(plain + seqbody_len + 1, (int)pad_count, pad_count);

    if (hdr_len + presig_len > MU_ASYM_SIGNED_INPUT_MAX) {
        mu_secure_zero(plain, plain_cap);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_byte_t sig[MU_ASYM_SCRATCH];
    if (sig_len > sizeof(sig)) {
        mu_secure_zero(plain, plain_cap);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    (void)memcpy(out + hdr_len, plain, presig_len);
    size_t produced_sig = sizeof(sig);
    if (policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
        s = crypto->rsa_pss_sha256_sign(crypto->context, out, hdr_len + presig_len, sig, &produced_sig);
    } else {
        s = crypto->rsa_sha256_sign(crypto->context, out, hdr_len + presig_len, sig, &produced_sig);
    }
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(sig, sizeof(sig));
        mu_secure_zero(out + hdr_len, presig_len);
        mu_secure_zero(plain, plain_cap);
        return s;
    }
    if (produced_sig != sig_len) {
        mu_secure_zero(sig, sizeof(sig));
        mu_secure_zero(out + hdr_len, presig_len);
        mu_secure_zero(plain, plain_cap);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    (void)memcpy(plain + presig_len, sig, sig_len);
    mu_secure_zero(sig, sizeof(sig));
    mu_secure_zero(out + hdr_len, presig_len);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t encrypt_wrap_blocks(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                              const opcua_byte_t *receiver_cert, size_t receiver_cert_len,
                                              const opcua_byte_t *plain, size_t plain_block, size_t cipher_block,
                                              size_t enc_len, opcua_byte_t *out, size_t out_offset) {
    size_t blocks = enc_len / plain_block;
    size_t cipher_len = (enc_len / plain_block) * cipher_block;
    opcua_statuscode_t s;
    for (size_t i = 0; i < blocks; i++) {
        size_t out_block = cipher_block;
        if (policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
            s = crypto->rsa_oaep_sha256_encrypt(crypto->context, receiver_cert, receiver_cert_len,
                                                plain + i * plain_block, plain_block,
                                                out + out_offset + i * cipher_block, &out_block);
        } else {
            s = crypto->rsa_oaep_encrypt(crypto->context, receiver_cert, receiver_cert_len, plain + i * plain_block,
                                         plain_block, out + out_offset + i * cipher_block, &out_block);
        }
        if (s != MU_STATUS_GOOD) {
            mu_secure_zero(out + out_offset, cipher_len);
            return s;
        }
        if (out_block != cipher_block) {
            mu_secure_zero(out + out_offset, cipher_len);
            return MU_STATUS_BAD_INTERNALERROR;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_asym_chunk_wrap(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                      opcua_uint32_t secure_channel_id, opcua_uint32_t sequence_number,
                                      opcua_uint32_t request_id, const opcua_byte_t *receiver_cert,
                                      size_t receiver_cert_len, const opcua_byte_t *body, size_t body_len,
                                      opcua_byte_t *out, size_t out_cap, size_t *out_len) {
    if (!out || !out_len || (!body && body_len)) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (policy == MU_SECURITY_POLICY_NONE_ID) {
        return wrap_none(secure_channel_id, sequence_number, request_id, body, body_len, out, out_cap, out_len);
    }

    opcua_statuscode_t s = validate_wrap_policy(crypto, policy, receiver_cert, receiver_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_byte_t thumb[MU_THUMBPRINT_LENGTH];
    size_t hdr_len = 0, sig_len = 0, plain_block = 0, cipher_block = 0;
    s = build_wrap_header(crypto, policy, secure_channel_id, receiver_cert, receiver_cert_len, out, out_cap, thumb,
                          sizeof(thumb), &hdr_len, &sig_len, &plain_block, &cipher_block);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t presig_len = 0, enc_len = 0, cipher_len = 0, total = 0;
    s = compute_wrap_sizes(body_len, sig_len, plain_block, cipher_block, hdr_len, out_cap, &presig_len, &enc_len,
                           &cipher_len, &total);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    out[4] = (opcua_byte_t)(total);
    out[5] = (opcua_byte_t)(total >> 8);
    out[6] = (opcua_byte_t)(total >> 16);
    out[7] = (opcua_byte_t)(total >> 24);

    opcua_byte_t plain[MU_ASYM_MAX_PLAINTEXT];
    s = sign_wrap_payload(crypto, policy, sequence_number, request_id, body, body_len, plain, sizeof(plain), out,
                          hdr_len, presig_len, sig_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = encrypt_wrap_blocks(crypto, policy, receiver_cert, receiver_cert_len, plain, plain_block, cipher_block, enc_len,
                            out, hdr_len);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(plain, sizeof(plain));
        return s;
    }

    *out_len = total;
    mu_secure_zero(plain, sizeof(plain));
    return MU_STATUS_GOOD;
}
