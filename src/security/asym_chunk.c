/* src/security/asym_chunk.c
 * Asymmetric (OPN) chunk protection for Basic256Sha256 (OPC 10000-6 §6.7.2). */
#include "asym_chunk.h"
#include "certificate.h"
#include "key_derivation.h"
#include "micro_opcua/encoding.h"
#include <string.h>

/* RSA-OAEP with SHA-1 (MGF1-SHA1) overhead: plaintext block = keybytes - 2*20 - 2. */
#define MU_OAEP_SHA1_OVERHEAD 42

/* RSA modulus-sized scratch for Basic256Sha256. A 2048-bit RSA modulus is
   256 bytes; keeping headroom for 4096-bit RSA requires 512 bytes. RSA-OAEP
   plaintext blocks are smaller than the modulus by MU_OAEP_SHA1_OVERHEAD. */
#define MU_ASYM_SCRATCH 512

/* Signature input is [cleartext header | signed plaintext], not an RSA block.
   Keep the existing envelope limit separate so modulus scratch remains small
   without changing accepted OPN header/body sizes. */
#define MU_ASYM_SIGNED_INPUT_MAX 4096

static opcua_statuscode_t key_bytes(const mu_crypto_adapter_t *crypto, const opcua_byte_t *cert, size_t cert_len,
                                    size_t *bytes) {
    size_t bits = 0;
    opcua_statuscode_t s = crypto->get_certificate_key_bits(crypto->context, cert, cert_len, &bits);
    if (s != MU_STATUS_GOOD || bits == 0)
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    *bytes = bits / 8;
    /* The RSA modulus must exceed the OAEP overhead, or the plaintext block size
       would underflow. (Basic256Sha256 also mandates >= 2048-bit keys.) */
    if (*bytes <= MU_OAEP_SHA1_OVERHEAD)
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    return MU_STATUS_GOOD;
}

/* Write the cleartext OPN header: "OPNF", a zero MessageSize placeholder,
   SecureChannelId, then the AsymmetricAlgorithmSecurityHeader. */
static opcua_statuscode_t write_header(mu_binary_writer_t *w, mu_security_policy_id_t policy, opcua_uint32_t scid,
                                       const opcua_byte_t *sender_cert, size_t sender_cert_len,
                                       const opcua_byte_t *receiver_thumbprint, size_t thumb_len) {
    const char *uri = mu_security_policy_uri(policy);
    if (!uri)
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;

    w->buffer[0] = 'O';
    w->buffer[1] = 'P';
    w->buffer[2] = 'N';
    w->buffer[3] = 'F';
    w->position = 4;
    opcua_statuscode_t s = mu_binary_write_uint32(w, 0); /* MessageSize placeholder */
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_uint32(w, scid);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_string_t policy_uri = {(opcua_int32_t)strlen(uri), (const opcua_byte_t *)uri};
    s = mu_binary_write_string(w, &policy_uri);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_bytestring_t cert = {sender_cert ? (opcua_int32_t)sender_cert_len : -1, sender_cert};
    s = mu_binary_write_bytestring(w, &cert);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_bytestring_t thumb = {receiver_thumbprint ? (opcua_int32_t)thumb_len : -1, receiver_thumbprint};
    return mu_binary_write_bytestring(w, &thumb);
}

/* --- None: cleartext framing --- */

static opcua_statuscode_t wrap_none(opcua_uint32_t scid, opcua_uint32_t seq, opcua_uint32_t rid,
                                    const opcua_byte_t *body, size_t body_len, opcua_byte_t *out, size_t out_cap,
                                    size_t *out_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, out_cap);
    opcua_statuscode_t s = write_header(&w, MU_SECURITY_POLICY_NONE_ID, scid, NULL, 0, NULL, 0);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_uint32(&w, seq);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_uint32(&w, rid);
    if (s != MU_STATUS_GOOD)
        return s;
    if (w.position + body_len > out_cap)
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    memcpy(out + w.position, body, body_len);
    size_t total = w.position + body_len;
    /* Patch MessageSize. */
    out[4] = (opcua_byte_t)(total);
    out[5] = (opcua_byte_t)(total >> 8);
    out[6] = (opcua_byte_t)(total >> 16);
    out[7] = (opcua_byte_t)(total >> 24);
    *out_len = total;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_asym_chunk_wrap(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                      opcua_uint32_t secure_channel_id, opcua_uint32_t sequence_number,
                                      opcua_uint32_t request_id, const opcua_byte_t *receiver_cert,
                                      size_t receiver_cert_len, const opcua_byte_t *body, size_t body_len,
                                      opcua_byte_t *out, size_t out_cap, size_t *out_len) {
    if (!out || !out_len || (!body && body_len))
        return MU_STATUS_BAD_INTERNALERROR;

    if (policy == MU_SECURITY_POLICY_NONE_ID) {
        return wrap_none(secure_channel_id, sequence_number, request_id, body, body_len, out, out_cap, out_len);
    }
    if (policy != MU_SECURITY_POLICY_BASIC256SHA256_ID && policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID)
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    if (!crypto || !receiver_cert || receiver_cert_len == 0)
        return MU_STATUS_BAD_INTERNALERROR;

    /* Our certificate goes in the header; we sign with our key. */
    const opcua_byte_t *sender_cert = NULL;
    size_t sender_cert_len = 0;
    opcua_statuscode_t s = crypto->get_own_certificate(crypto->context, &sender_cert, &sender_cert_len);
    if (s != MU_STATUS_GOOD)
        return s;

    size_t sig_len = 0, recv_key_bytes = 0;
    s = key_bytes(crypto, sender_cert, sender_cert_len, &sig_len);
    if (s != MU_STATUS_GOOD)
        return s;
    s = key_bytes(crypto, receiver_cert, receiver_cert_len, &recv_key_bytes);
    if (s != MU_STATUS_GOOD)
        return s;
    size_t plain_block = recv_key_bytes - MU_OAEP_SHA1_OVERHEAD;
    size_t cipher_block = recv_key_bytes;

    /* Receiver thumbprint for the header. */
    opcua_byte_t thumb[MU_THUMBPRINT_LENGTH];
    s = mu_certificate_thumbprint(crypto, receiver_cert, receiver_cert_len, thumb);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, out_cap);
    s = write_header(&w, policy, secure_channel_id, sender_cert, sender_cert_len, thumb, sizeof(thumb));
    if (s != MU_STATUS_GOOD)
        return s;
    size_t hdr_len = w.position;

    /* Plaintext to protect: SequenceHeader(8) + body. */
    size_t seqbody_len = 8 + body_len;
    /* Pad so (seqbody + paddingSizeByte + padding + signature) is a multiple of plain_block. */
    size_t base = seqbody_len + 1 + sig_len;
    size_t pad_count = (plain_block - (base % plain_block)) % plain_block;
    size_t enc_len = base + pad_count;               /* multiple of plain_block */
    size_t presig_len = seqbody_len + 1 + pad_count; /* signed region within plaintext */
    size_t cipher_len = (enc_len / plain_block) * cipher_block;
    size_t total = hdr_len + cipher_len;

    if (enc_len > MU_ASYM_MAX_PLAINTEXT)
        return MU_STATUS_BAD_REQUESTTOOLARGE;
    if (hdr_len + presig_len > MU_ASYM_SIGNED_INPUT_MAX)
        return MU_STATUS_BAD_REQUESTTOOLARGE;
    if (total > out_cap)
        return MU_STATUS_BAD_RESPONSETOOLARGE;

    /* Patch the final (encrypted) MessageSize before signing — the signature
       covers the header, which includes MessageSize. */
    out[4] = (opcua_byte_t)(total);
    out[5] = (opcua_byte_t)(total >> 8);
    out[6] = (opcua_byte_t)(total >> 16);
    out[7] = (opcua_byte_t)(total >> 24);

    /* Assemble the plaintext: SequenceHeader + body + paddingSize + padding. */
    opcua_byte_t plain[MU_ASYM_MAX_PLAINTEXT];
    mu_binary_writer_t pw;
    mu_binary_writer_init(&pw, plain, sizeof(plain));
    s = mu_binary_write_uint32(&pw, sequence_number);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(plain, sizeof(plain));
        return s;
    }
    s = mu_binary_write_uint32(&pw, request_id);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(plain, sizeof(plain));
        return s;
    }
    memcpy(plain + 8, body, body_len);
    plain[seqbody_len] = (opcua_byte_t)pad_count;               /* PaddingSize */
    memset(plain + seqbody_len + 1, (int)pad_count, pad_count); /* Padding bytes */

    /* Sign over [cleartext header | SequenceHeader | body | paddingSize | padding].
       The header is already in `out`; stage the signed plaintext after it so no
       separate header+plaintext stack buffer is needed. */
    if (hdr_len + presig_len > MU_ASYM_SIGNED_INPUT_MAX) {
        mu_secure_zero(plain, sizeof(plain));
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_byte_t sig[MU_ASYM_SCRATCH];
    if (sig_len > sizeof(sig)) {
        mu_secure_zero(plain, sizeof(plain));
        return MU_STATUS_BAD_INTERNALERROR;
    }
    memcpy(out + hdr_len, plain, presig_len);
    size_t produced_sig = sizeof(sig);
    s = crypto->rsa_sha256_sign(crypto->context, out, hdr_len + presig_len, sig, &produced_sig);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(sig, sizeof(sig));
        mu_secure_zero(out + hdr_len, presig_len);
        mu_secure_zero(plain, sizeof(plain));
        return s;
    }
    if (produced_sig != sig_len) {
        mu_secure_zero(sig, sizeof(sig));
        mu_secure_zero(out + hdr_len, presig_len);
        mu_secure_zero(plain, sizeof(plain));
        return MU_STATUS_BAD_INTERNALERROR;
    }
    memcpy(plain + presig_len, sig, sig_len); /* signature is the tail of the plaintext */
    mu_secure_zero(sig, sizeof(sig));
    mu_secure_zero(out + hdr_len, presig_len);

    /* Encrypt the plaintext block-by-block to the receiver's public key. */
    size_t blocks = enc_len / plain_block;
    for (size_t i = 0; i < blocks; i++) {
        size_t out_block = cipher_block;
        s = crypto->rsa_oaep_encrypt(crypto->context, receiver_cert, receiver_cert_len, plain + i * plain_block,
                                     plain_block, out + hdr_len + i * cipher_block, &out_block);
        if (s != MU_STATUS_GOOD) {
            mu_secure_zero(out + hdr_len, cipher_len);
            mu_secure_zero(plain, sizeof(plain));
            return s;
        }
        if (out_block != cipher_block) {
            mu_secure_zero(out + hdr_len, cipher_len);
            mu_secure_zero(plain, sizeof(plain));
            return MU_STATUS_BAD_INTERNALERROR;
        }
    }

    *out_len = total;
    mu_secure_zero(plain, sizeof(plain));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_asym_chunk_unwrap(const mu_crypto_adapter_t *crypto, const opcua_byte_t *chunk, size_t chunk_len,
                                        opcua_byte_t *out_body, size_t out_cap, size_t *out_body_len,
                                        opcua_byte_t *scratch, size_t scratch_len, mu_asym_chunk_info_t *info) {
    if (!crypto || !chunk || !out_body || !out_body_len || !info || !scratch)
        return MU_STATUS_BAD_INTERNALERROR;
    if (scratch_len < MU_ASYM_MAX_PLAINTEXT + MU_ASYM_SIGNED_INPUT_MAX)
        return MU_STATUS_BAD_INTERNALERROR;
    if (chunk_len < 12 || chunk[0] != 'O' || chunk[1] != 'P' || chunk[2] != 'N') {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    size_t msg_size = (size_t)chunk[4] | ((size_t)chunk[5] << 8) | ((size_t)chunk[6] << 16) | ((size_t)chunk[7] << 24);
    if (msg_size > chunk_len)
        return MU_STATUS_BAD_DECODINGERROR;

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, chunk, msg_size);
    r.position = 8;
    opcua_statuscode_t s = mu_binary_read_uint32(&r, &info->secure_channel_id);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_string_t policy_uri;
    mu_bytestring_t sender_cert, receiver_thumb;
    s = mu_binary_read_string(&r, &policy_uri);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_read_bytestring(&r, &sender_cert);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_read_bytestring(&r, &receiver_thumb);
    if (s != MU_STATUS_GOOD)
        return s;
    size_t hdr_len = r.position;

    info->policy = mu_security_policy_from_uri(policy_uri.data, policy_uri.length > 0 ? (size_t)policy_uri.length : 0);
    info->sender_cert = sender_cert.length > 0 ? sender_cert.data : NULL;
    info->sender_cert_len = sender_cert.length > 0 ? (size_t)sender_cert.length : 0;

    if (info->policy == MU_SECURITY_POLICY_NONE_ID) {
        /* Cleartext: SequenceHeader follows the header. */
        if (hdr_len + 8 > msg_size)
            return MU_STATUS_BAD_DECODINGERROR;
        info->sequence_number = (opcua_uint32_t)chunk[hdr_len] | ((opcua_uint32_t)chunk[hdr_len + 1] << 8) |
                                ((opcua_uint32_t)chunk[hdr_len + 2] << 16) | ((opcua_uint32_t)chunk[hdr_len + 3] << 24);
        info->request_id = (opcua_uint32_t)chunk[hdr_len + 4] | ((opcua_uint32_t)chunk[hdr_len + 5] << 8) |
                           ((opcua_uint32_t)chunk[hdr_len + 6] << 16) | ((opcua_uint32_t)chunk[hdr_len + 7] << 24);
        size_t body_len = msg_size - hdr_len - 8;
        if (body_len > out_cap)
            return MU_STATUS_BAD_RESPONSETOOLARGE;
        memcpy(out_body, chunk + hdr_len + 8, body_len);
        *out_body_len = body_len;
        return MU_STATUS_GOOD;
    }
    if (info->policy != MU_SECURITY_POLICY_BASIC256SHA256_ID &&
        info->policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID)
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;

    /* Validate the sender certificate and that the chunk is addressed to us. */
    s = mu_certificate_validate(crypto, info->policy, info->sender_cert, info->sender_cert_len);
    if (s != MU_STATUS_GOOD)
        return s;

    const opcua_byte_t *own_cert = NULL;
    size_t own_cert_len = 0;
    s = crypto->get_own_certificate(crypto->context, &own_cert, &own_cert_len);
    if (s != MU_STATUS_GOOD)
        return s;
    opcua_byte_t own_thumb[MU_THUMBPRINT_LENGTH];
    s = mu_certificate_thumbprint(crypto, own_cert, own_cert_len, own_thumb);
    if (s != MU_STATUS_GOOD)
        return s;
    if (receiver_thumb.length != (opcua_int32_t)MU_THUMBPRINT_LENGTH ||
        !mu_secure_memeq(receiver_thumb.data, own_thumb, MU_THUMBPRINT_LENGTH)) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    size_t own_key_bytes = 0, sig_len = 0;
    s = key_bytes(crypto, own_cert, own_cert_len, &own_key_bytes);
    if (s != MU_STATUS_GOOD)
        return s;
    s = key_bytes(crypto, info->sender_cert, info->sender_cert_len, &sig_len);
    if (s != MU_STATUS_GOOD)
        return s;

    size_t cipher_len = msg_size - hdr_len;
    if (cipher_len == 0 || cipher_len % own_key_bytes != 0)
        return MU_STATUS_BAD_DECODINGERROR;

    /* Decrypt block-by-block with our private key. */
    opcua_byte_t *plain = scratch;
    size_t plain_len = 0;
    size_t blocks = cipher_len / own_key_bytes;
    for (size_t i = 0; i < blocks; i++) {
        size_t out_block = MU_ASYM_MAX_PLAINTEXT - plain_len;
        s = crypto->rsa_oaep_decrypt(crypto->context, chunk + hdr_len + i * own_key_bytes, own_key_bytes,
                                     plain + plain_len, &out_block);
        if (s != MU_STATUS_GOOD) {
            mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        plain_len += out_block;
    }

    if (plain_len < sig_len + 1 + 8) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    size_t signed_len = plain_len - sig_len; /* SequenceHeader+body+paddingSize+padding */
    const opcua_byte_t *signature = plain + signed_len;

    /* Verify the sender signature over [cleartext header | signed plaintext]. */
    if (hdr_len + signed_len > MU_ASYM_SIGNED_INPUT_MAX) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_byte_t *verify_buf = scratch + MU_ASYM_MAX_PLAINTEXT;
    memcpy(verify_buf, chunk, hdr_len);
    memcpy(verify_buf + hdr_len, plain, signed_len);
    s = crypto->rsa_sha256_verify(crypto->context, info->sender_cert, info->sender_cert_len, verify_buf,
                                  hdr_len + signed_len, signature, sig_len);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(verify_buf, MU_ASYM_SIGNED_INPUT_MAX);
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    mu_secure_zero(verify_buf, MU_ASYM_SIGNED_INPUT_MAX);

    /* Strip padding. */
    size_t pad_count = plain[signed_len - 1];
    if (signed_len < pad_count + 1 + 8) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    /* Verify padding bytes are all equal to pad_count */
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
    if (body_len > out_cap) {
        mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    memcpy(out_body, plain + 8, body_len);
    *out_body_len = body_len;
    mu_secure_zero(plain, MU_ASYM_MAX_PLAINTEXT);
    return MU_STATUS_GOOD;
}
