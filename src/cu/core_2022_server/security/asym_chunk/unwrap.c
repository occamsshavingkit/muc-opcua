/* src/security/asym_chunk/unwrap.c
 * mu_asym_chunk_unwrap and its helpers. */
#include "security/asym_chunk.h"
#include "security/asym_chunk/common.h"
#ifdef MUC_OPCUA_CU_SECURITY_ECC
#include "muc_opcua/config.h" /* MU_ECC_SIGNATURE_LENGTH */
#endif

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

#ifdef MUC_OPCUA_CU_SECURITY_ECC
/* Unwrap a signed-only ECC OPN chunk (OPC-10000-6 §6.8.1): verify the sender
   signature and recipient thumbprint, then recover the cleartext SequenceHeader +
   body. `hdr_len` is the byte offset past the AsymmetricAlgorithmSecurityHeader;
   `receiver_thumb` is the ReceiverCertificateThumbprint field from that header. */
static opcua_statuscode_t unwrap_ecc(const mu_asym_unwrap_params_t *p, size_t hdr_len, size_t msg_size,
                                     const mu_bytestring_t *receiver_thumb) {
    mu_ecc_curve_t curve;
    if (!mu_security_policy_ecc_curve(p->info->policy, &curve)) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }
    if (!p->crypto->ecc_verify || !p->crypto->get_own_ecc_certificate) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }

    opcua_statuscode_t s =
        mu_certificate_validate(p->crypto, p->info->policy, p->info->sender_cert, p->info->sender_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* Our own ECC identity for this curve — the ReceiverCertificateThumbprint the
       peer computed must match it. */
    const opcua_byte_t *own_cert = NULL;
    size_t own_cert_len = 0;
    s = p->crypto->get_own_ecc_certificate(p->crypto->context, curve, &own_cert, &own_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    opcua_byte_t own_thumb[MU_THUMBPRINT_LENGTH];
    s = mu_certificate_thumbprint(p->crypto, own_cert, own_cert_len, own_thumb);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (receiver_thumb->length != (opcua_int32_t)MU_THUMBPRINT_LENGTH ||
        !mu_secure_memeq(receiver_thumb->data, own_thumb, MU_THUMBPRINT_LENGTH)) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    /* [hdr | SequenceHeader(8) | body | signature]; signed region is everything
       before the trailing fixed-length signature and is contiguous (not encrypted). */
    if (msg_size < hdr_len + 8 + MU_ECC_SIGNATURE_LENGTH) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    size_t signed_len = msg_size - MU_ECC_SIGNATURE_LENGTH;
    const opcua_byte_t *signature = p->chunk + signed_len;
    s = p->crypto->ecc_verify(p->crypto->context, curve, p->info->sender_cert, p->info->sender_cert_len, p->chunk,
                              signed_len, signature, MU_ECC_SIGNATURE_LENGTH);
    if (s != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    const opcua_byte_t *sh = p->chunk + hdr_len;
    p->info->sequence_number = (opcua_uint32_t)sh[0] | ((opcua_uint32_t)sh[1] << 8) | ((opcua_uint32_t)sh[2] << 16) |
                               ((opcua_uint32_t)sh[3] << 24);
    p->info->request_id = (opcua_uint32_t)sh[4] | ((opcua_uint32_t)sh[5] << 8) | ((opcua_uint32_t)sh[6] << 16) |
                          ((opcua_uint32_t)sh[7] << 24);
    size_t body_len = signed_len - hdr_len - 8;
    if (!mu_checked_memcpy(p->out_body, p->out_cap, sh + 8, body_len)) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    *p->out_body_len = body_len;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_CU_SECURITY_ECC */

opcua_statuscode_t mu_asym_chunk_unwrap(const mu_asym_unwrap_params_t *p) {
    if (!p->crypto || !p->chunk || !p->out_body || !p->out_body_len || !p->info || !p->scratch) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (p->scratch_len < MU_ASYM_MAX_PLAINTEXT + MU_ASYM_SIGNED_INPUT_MAX) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (p->chunk_len < 12 || p->chunk[0] != 'O' || p->chunk[1] != 'P' || p->chunk[2] != 'N') {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    size_t msg_size =
        (size_t)p->chunk[4] | ((size_t)p->chunk[5] << 8) | ((size_t)p->chunk[6] << 16) | ((size_t)p->chunk[7] << 24);
    if (msg_size > p->chunk_len) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, p->chunk, msg_size);
    r.position = 8;
    opcua_statuscode_t s = mu_binary_read_uint32(&r, &p->info->secure_channel_id);
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

    p->info->policy =
        mu_security_policy_from_uri(policy_uri.data, policy_uri.length > 0 ? (size_t)policy_uri.length : 0);
    p->info->sender_cert = sender_cert.length > 0 ? sender_cert.data : NULL;
    p->info->sender_cert_len = sender_cert.length > 0 ? (size_t)sender_cert.length : 0;

    if (p->info->policy == MU_SECURITY_POLICY_NONE_ID) {
        /* Cleartext: SequenceHeader follows the header. */
        if (hdr_len + 8 > msg_size) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        p->info->sequence_number = (opcua_uint32_t)p->chunk[hdr_len] | ((opcua_uint32_t)p->chunk[hdr_len + 1] << 8) |
                                   ((opcua_uint32_t)p->chunk[hdr_len + 2] << 16) |
                                   ((opcua_uint32_t)p->chunk[hdr_len + 3] << 24);
        p->info->request_id = (opcua_uint32_t)p->chunk[hdr_len + 4] | ((opcua_uint32_t)p->chunk[hdr_len + 5] << 8) |
                              ((opcua_uint32_t)p->chunk[hdr_len + 6] << 16) |
                              ((opcua_uint32_t)p->chunk[hdr_len + 7] << 24);
        size_t body_len = msg_size - hdr_len - 8;
        if (!mu_checked_memcpy(p->out_body, p->out_cap, p->chunk + hdr_len + 8, body_len)) {
            return MU_STATUS_BAD_RESPONSETOOLARGE;
        }
        *p->out_body_len = body_len;
        return MU_STATUS_GOOD;
    }
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    if (mu_security_policy_asym_family(p->info->policy) == MU_ASYM_FAMILY_ECC) {
        return unwrap_ecc(p, hdr_len, msg_size, &receiver_thumb);
    }
#endif

    if (p->info->policy != MU_SECURITY_POLICY_BASIC256SHA256_ID &&
        p->info->policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID &&
        p->info->policy != MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }

    /* Validate the sender certificate and that the p->chunk is addressed to us. */
    s = mu_certificate_validate(p->crypto, p->info->policy, p->info->sender_cert, p->info->sender_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    const opcua_byte_t *own_cert = NULL;
    size_t own_cert_len = 0;
    s = p->crypto->get_own_certificate(p->crypto->context, &own_cert, &own_cert_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    opcua_byte_t own_thumb[MU_THUMBPRINT_LENGTH];
    s = mu_certificate_thumbprint(p->crypto, own_cert, own_cert_len, own_thumb);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (receiver_thumb.length != (opcua_int32_t)MU_THUMBPRINT_LENGTH ||
        !mu_secure_memeq(receiver_thumb.data, own_thumb, MU_THUMBPRINT_LENGTH)) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    size_t own_key_bytes = 0, sig_len = 0;
    s = key_bytes(p->crypto, own_cert, own_cert_len, &own_key_bytes);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = key_bytes(p->crypto, p->info->sender_cert, p->info->sender_cert_len, &sig_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t cipher_len = msg_size - hdr_len;
    if (cipher_len == 0 || cipher_len % own_key_bytes != 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    opcua_byte_t *plain = p->scratch;
    size_t plain_len = 0;
    s = decrypt_unwrap_blocks(p->crypto, p->info->policy, p->chunk, hdr_len, own_key_bytes, cipher_len, plain,
                              &plain_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (plain_len < sig_len + 1 + 8) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    size_t signed_len = plain_len - sig_len;
    const opcua_byte_t *signature = plain + signed_len;

    s = verify_unwrap_signature(p->crypto, p->info, p->chunk, hdr_len, plain, signed_len, signature, sig_len,
                                p->scratch);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = extract_unwrap_payload(plain, signed_len, p->out_body, p->out_cap, p->out_body_len, p->info);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
    return MU_STATUS_GOOD;
}
