/* src/security/asym_chunk/unwrap.c
 * mu_asym_chunk_unwrap and its helpers. */
#include "common.h"

/* --- Helpers for mu_asym_chunk_unwrap --- */

static opcua_statuscode_t decrypt_unwrap_blocks(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                                const opcua_byte_t *chunk, size_t hdr_len, size_t key_bytes_size,
                                                size_t cipher_len, opcua_byte_t *plain, size_t *plain_len) {
    opcua_statuscode_t s;
    size_t blocks = cipher_len / key_bytes_size;
    *plain_len = 0;
    for (size_t i = 0; i < blocks; i++) {
        size_t out_block = MU_ASYM_MAX_PLAINTEXT - *plain_len;
        if (policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
            s = crypto->rsa_oaep_sha256_decrypt(crypto->context, chunk + hdr_len + i * key_bytes_size, key_bytes_size,
                                                plain + *plain_len, &out_block);
        } else {
            s = crypto->rsa_oaep_decrypt(crypto->context, chunk + hdr_len + i * key_bytes_size, key_bytes_size,
                                         plain + *plain_len, &out_block);
        }
        if (s != MU_STATUS_GOOD) {
            mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        *plain_len += out_block;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t verify_unwrap_signature(const mu_crypto_adapter_t *crypto, const mu_asym_chunk_info_t *info,
                                                  const opcua_byte_t *chunk, size_t hdr_len, opcua_byte_t *plain,
                                                  size_t signed_len, const opcua_byte_t *signature, size_t sig_len,
                                                  opcua_byte_t *scratch) {
    if (hdr_len + signed_len > MU_ASYM_SIGNED_INPUT_MAX) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_byte_t *verify_buf = scratch + MU_ASYM_MAX_PLAINTEXT;
    (void)memcpy(verify_buf, chunk, hdr_len);
    (void)memcpy(verify_buf + hdr_len, plain, signed_len);
    opcua_statuscode_t s;
    if (info->policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
        s = crypto->rsa_pss_sha256_verify(crypto->context, info->sender_cert, info->sender_cert_len, verify_buf,
                                          hdr_len + signed_len, signature, sig_len);
    } else {
        s = crypto->rsa_sha256_verify(crypto->context, info->sender_cert, info->sender_cert_len, verify_buf,
                                      hdr_len + signed_len, signature, sig_len);
    }
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(verify_buf, MU_ASYM_SIGNED_INPUT_MAX);
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    mu_secure_zero(verify_buf, MU_ASYM_SIGNED_INPUT_MAX);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t extract_unwrap_payload(opcua_byte_t *plain, size_t signed_len, opcua_byte_t *out_body,
                                                 size_t out_cap, size_t *out_body_len, mu_asym_chunk_info_t *info) {
    size_t pad_count = plain[signed_len - 1];
    if (signed_len < pad_count + 1 + 8) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    for (size_t i = 1; i <= pad_count; i++) {
        if (plain[signed_len - 1 - i] != pad_count) {
            mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
    }
    size_t seqbody_len = signed_len - 1 - pad_count;

    info->sequence_number = (opcua_uint32_t)plain[0] | ((opcua_uint32_t)plain[1] << 8) |
                            ((opcua_uint32_t)plain[2] << 16) | ((opcua_uint32_t)plain[3] << 24);
    info->request_id = (opcua_uint32_t)plain[4] | ((opcua_uint32_t)plain[5] << 8) | ((opcua_uint32_t)plain[6] << 16) |
                       ((opcua_uint32_t)plain[7] << 24);

    size_t body_len = seqbody_len - 8;
    if (!mu_checked_memcpy(out_body, out_cap, plain + 8, body_len)) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    *out_body_len = body_len;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_asym_chunk_unwrap(const mu_crypto_adapter_t *crypto, const opcua_byte_t *chunk, size_t chunk_len,
                                        opcua_byte_t *out_body, size_t out_cap, size_t *out_body_len,
                                        opcua_byte_t *scratch, size_t scratch_len, mu_asym_chunk_info_t *info) {
    if (!crypto || !chunk || !out_body || !out_body_len || !info || !scratch) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (scratch_len < MU_ASYM_MAX_PLAINTEXT + MU_ASYM_SIGNED_INPUT_MAX) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (chunk_len < 12 || chunk[0] != 'O' || chunk[1] != 'P' || chunk[2] != 'N') {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    size_t msg_size = (size_t)chunk[4] | ((size_t)chunk[5] << 8) | ((size_t)chunk[6] << 16) | ((size_t)chunk[7] << 24);
    if (msg_size > chunk_len) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, chunk, msg_size);
    r.position = 8;
    opcua_statuscode_t s = mu_binary_read_uint32(&r, &info->secure_channel_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t policy_uri;
    mu_bytestring_t sender_cert, receiver_thumb;
    s = mu_binary_read_string(&r, &policy_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_read_bytestring(&r, &sender_cert);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_read_bytestring(&r, &receiver_thumb);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    size_t hdr_len = r.position;

    info->policy = mu_security_policy_from_uri(policy_uri.data, policy_uri.length > 0 ? (size_t)policy_uri.length : 0);
    info->sender_cert = sender_cert.length > 0 ? sender_cert.data : NULL;
    info->sender_cert_len = sender_cert.length > 0 ? (size_t)sender_cert.length : 0;

    if (info->policy == MU_SECURITY_POLICY_NONE_ID) {
        /* Cleartext: SequenceHeader follows the header. */
        if (hdr_len + 8 > msg_size) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        info->sequence_number = (opcua_uint32_t)chunk[hdr_len] | ((opcua_uint32_t)chunk[hdr_len + 1] << 8) |
                                ((opcua_uint32_t)chunk[hdr_len + 2] << 16) | ((opcua_uint32_t)chunk[hdr_len + 3] << 24);
        info->request_id = (opcua_uint32_t)chunk[hdr_len + 4] | ((opcua_uint32_t)chunk[hdr_len + 5] << 8) |
                           ((opcua_uint32_t)chunk[hdr_len + 6] << 16) | ((opcua_uint32_t)chunk[hdr_len + 7] << 24);
        size_t body_len = msg_size - hdr_len - 8;
        if (!mu_checked_memcpy(out_body, out_cap, chunk + hdr_len + 8, body_len)) {
            return MU_STATUS_BAD_RESPONSETOOLARGE;
        }
        *out_body_len = body_len;
        return MU_STATUS_GOOD;
    }
    if (info->policy != MU_SECURITY_POLICY_BASIC256SHA256_ID &&
        info->policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID &&
        info->policy != MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }

    /* Validate the sender certificate and that the chunk is addressed to us. */
    s = mu_certificate_validate(crypto, info->policy, info->sender_cert, info->sender_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    const opcua_byte_t *own_cert = NULL;
    size_t own_cert_len = 0;
    s = crypto->get_own_certificate(crypto->context, &own_cert, &own_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    opcua_byte_t own_thumb[MU_THUMBPRINT_LENGTH];
    s = mu_certificate_thumbprint(crypto, own_cert, own_cert_len, own_thumb);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (receiver_thumb.length != (opcua_int32_t)MU_THUMBPRINT_LENGTH ||
        !mu_secure_memeq(receiver_thumb.data, own_thumb, MU_THUMBPRINT_LENGTH)) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    size_t own_key_bytes = 0, sig_len = 0;
    s = key_bytes(crypto, own_cert, own_cert_len, &own_key_bytes);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = key_bytes(crypto, info->sender_cert, info->sender_cert_len, &sig_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t cipher_len = msg_size - hdr_len;
    if (cipher_len == 0 || cipher_len % own_key_bytes != 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    opcua_byte_t *plain = scratch;
    size_t plain_len = 0;
    s = decrypt_unwrap_blocks(crypto, info->policy, chunk, hdr_len, own_key_bytes, cipher_len, plain, &plain_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (plain_len < sig_len + 1 + 8) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    size_t signed_len = plain_len - sig_len;
    const opcua_byte_t *signature = plain + signed_len;

    s = verify_unwrap_signature(crypto, info, chunk, hdr_len, plain, signed_len, signature, sig_len, scratch);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = extract_unwrap_payload(plain, signed_len, out_body, out_cap, out_body_len, info);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
    return MU_STATUS_GOOD;
}
