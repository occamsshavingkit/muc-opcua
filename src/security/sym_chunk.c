/* src/security/sym_chunk.c
 * Symmetric (MSG/CLO) chunk protection for Basic256Sha256 (OPC 10000-6 §6.7.2). */
#include "sym_chunk.h"
#include "key_derivation.h"
#include "micro_opcua/encoding.h"
#include <string.h>

#define MU_SYM_HEADER_SIZE 16   /* type+IsFinal(4) + MessageSize(4) + SecureChannelId(4) + TokenId(4) */
#define MU_SYM_SIG_LEN     MU_B256S256_SIGNATURE_LENGTH       /* 32 (HMAC-SHA256) */
#define MU_SYM_BLOCK       MU_B256S256_ENCRYPTION_BLOCK_SIZE  /* 16 (AES) */
/* Scratch for the signed (and possibly decrypted) region; bounds MSG body size.
   Sized to hold a discovery response carrying certificates in each endpoint. */
#define MU_SYM_SCRATCH 8192

opcua_statuscode_t mu_sym_keys_derive(const mu_crypto_adapter_t *crypto,
                                      const opcua_byte_t *secret, size_t secret_len,
                                      const opcua_byte_t *seed, size_t seed_len,
                                      mu_sym_keys_t *out_keys) {
    if (!out_keys) return MU_STATUS_BAD_INTERNALERROR;
    opcua_byte_t material[MU_B256S256_DERIVED_KEY_LENGTH];
    opcua_statuscode_t s = mu_p_sha256(crypto, secret, secret_len, seed, seed_len, material, sizeof(material));
    if (s != MU_STATUS_GOOD) return s;
    memcpy(out_keys->signing_key, material, MU_B256S256_SIGNATURE_KEY_LENGTH);
    memcpy(out_keys->encrypting_key, material + MU_B256S256_SIGNATURE_KEY_LENGTH, MU_B256S256_ENCRYPTION_KEY_LENGTH);
    memcpy(out_keys->iv, material + MU_B256S256_SIGNATURE_KEY_LENGTH + MU_B256S256_ENCRYPTION_KEY_LENGTH,
           MU_B256S256_ENCRYPTION_BLOCK_SIZE);
    return MU_STATUS_GOOD;
}

/* Write the cleartext symmetric header with a placeholder MessageSize. */
static void write_header(opcua_byte_t *out, const char message_type[3],
                         opcua_uint32_t scid, opcua_uint32_t token_id) {
    out[0] = (opcua_byte_t)message_type[0];
    out[1] = (opcua_byte_t)message_type[1];
    out[2] = (opcua_byte_t)message_type[2];
    out[3] = 'F';
    /* MessageSize at [4..8) patched later. */
    out[8]  = (opcua_byte_t)(scid);       out[9]  = (opcua_byte_t)(scid >> 8);
    out[10] = (opcua_byte_t)(scid >> 16); out[11] = (opcua_byte_t)(scid >> 24);
    out[12] = (opcua_byte_t)(token_id);       out[13] = (opcua_byte_t)(token_id >> 8);
    out[14] = (opcua_byte_t)(token_id >> 16); out[15] = (opcua_byte_t)(token_id >> 24);
}

static void put_u32(opcua_byte_t *p, opcua_uint32_t v) {
    p[0] = (opcua_byte_t)v; p[1] = (opcua_byte_t)(v >> 8);
    p[2] = (opcua_byte_t)(v >> 16); p[3] = (opcua_byte_t)(v >> 24);
}
static opcua_uint32_t get_u32(const opcua_byte_t *p) {
    return (opcua_uint32_t)p[0] | ((opcua_uint32_t)p[1] << 8) |
           ((opcua_uint32_t)p[2] << 16) | ((opcua_uint32_t)p[3] << 24);
}

opcua_statuscode_t mu_sym_chunk_wrap(
    const mu_crypto_adapter_t *crypto,
    mu_message_security_mode_t mode, const mu_sym_keys_t *keys,
    const char message_type[3],
    opcua_uint32_t secure_channel_id, opcua_uint32_t token_id,
    opcua_uint32_t sequence_number, opcua_uint32_t request_id,
    const opcua_byte_t *body, size_t body_len,
    opcua_byte_t *out, size_t out_cap, size_t *out_len)
{
    if (!crypto || !keys || !out || !out_len || (!body && body_len)) return MU_STATUS_BAD_INTERNALERROR;
    if (mode != MU_MESSAGE_SECURITY_MODE_SIGN && mode != MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT) {
        return MU_STATUS_BAD_SECURITYMODEREJECTED;
    }

    write_header(out, message_type, secure_channel_id, token_id);
    size_t seqbody_len = 8 + body_len;

    if (mode == MU_MESSAGE_SECURITY_MODE_SIGN) {
        size_t total = MU_SYM_HEADER_SIZE + seqbody_len + MU_SYM_SIG_LEN;
        if (total > out_cap) return MU_STATUS_BAD_RESPONSETOOLARGE;
        put_u32(out + 4, (opcua_uint32_t)total);
        put_u32(out + MU_SYM_HEADER_SIZE, sequence_number);
        put_u32(out + MU_SYM_HEADER_SIZE + 4, request_id);
        memcpy(out + MU_SYM_HEADER_SIZE + 8, body, body_len);
        size_t signed_len = MU_SYM_HEADER_SIZE + seqbody_len;
        opcua_byte_t mac[MU_SYM_SIG_LEN];
        opcua_statuscode_t s = crypto->hmac_sha256(crypto->context, keys->signing_key,
                                                   sizeof(keys->signing_key), out, signed_len, mac);
        if (s != MU_STATUS_GOOD) return s;
        memcpy(out + signed_len, mac, MU_SYM_SIG_LEN);
        *out_len = total;
        return MU_STATUS_GOOD;
    }

    /* SignAndEncrypt: pad so (SeqHeader+body+paddingSize+padding+signature) % block == 0. */
    size_t base = seqbody_len + 1 + MU_SYM_SIG_LEN;
    size_t pad_count = (MU_SYM_BLOCK - (base % MU_SYM_BLOCK)) % MU_SYM_BLOCK;
    size_t enc_len = base + pad_count;
    size_t presig_len = seqbody_len + 1 + pad_count;
    size_t total = MU_SYM_HEADER_SIZE + enc_len;
    if (total > out_cap) return MU_STATUS_BAD_RESPONSETOOLARGE;

    put_u32(out + 4, (opcua_uint32_t)total);
    put_u32(out + MU_SYM_HEADER_SIZE, sequence_number);
    put_u32(out + MU_SYM_HEADER_SIZE + 4, request_id);
    memcpy(out + MU_SYM_HEADER_SIZE + 8, body, body_len);
    out[MU_SYM_HEADER_SIZE + seqbody_len] = (opcua_byte_t)pad_count;            /* PaddingSize */
    memset(out + MU_SYM_HEADER_SIZE + seqbody_len + 1, (int)pad_count, pad_count);

    /* Sign-then-encrypt: HMAC over [header | SeqHeader | body | paddingSize | padding]. */
    size_t signed_len = MU_SYM_HEADER_SIZE + presig_len;
    opcua_byte_t mac[MU_SYM_SIG_LEN];
    opcua_statuscode_t s = crypto->hmac_sha256(crypto->context, keys->signing_key,
                                               sizeof(keys->signing_key), out, signed_len, mac);
    if (s != MU_STATUS_GOOD) return s;
    memcpy(out + signed_len, mac, MU_SYM_SIG_LEN);   /* signature tails the plaintext */

    /* Encrypt the SequenceHeader..signature region in place (AES-CBC preserves length). */
    s = crypto->aes256_cbc_encrypt(crypto->context, keys->encrypting_key, keys->iv,
                                   out + MU_SYM_HEADER_SIZE, enc_len, out + MU_SYM_HEADER_SIZE);
    if (s != MU_STATUS_GOOD) return s;

    *out_len = total;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_sym_chunk_unwrap(
    const mu_crypto_adapter_t *crypto,
    mu_message_security_mode_t mode, const mu_sym_keys_t *keys,
    const opcua_byte_t *chunk, size_t chunk_len,
    opcua_byte_t *out_body, size_t out_cap, size_t *out_body_len,
    mu_sym_chunk_info_t *info)
{
    if (!crypto || !keys || !chunk || !out_body || !out_body_len || !info) return MU_STATUS_BAD_INTERNALERROR;
    if (chunk_len < MU_SYM_HEADER_SIZE) return MU_STATUS_BAD_DECODINGERROR;

    size_t msg_size = get_u32(chunk + 4);
    if (msg_size > chunk_len || msg_size < MU_SYM_HEADER_SIZE) return MU_STATUS_BAD_DECODINGERROR;

    info->message_type[0] = (char)chunk[0];
    info->message_type[1] = (char)chunk[1];
    info->message_type[2] = (char)chunk[2];
    info->secure_channel_id = get_u32(chunk + 8);
    info->token_id = get_u32(chunk + 12);
    info->mode = mode;

    if (mode == MU_MESSAGE_SECURITY_MODE_SIGN) {
        if (msg_size < MU_SYM_HEADER_SIZE + 8 + MU_SYM_SIG_LEN) return MU_STATUS_BAD_DECODINGERROR;
        size_t signed_len = msg_size - MU_SYM_SIG_LEN;
        opcua_byte_t mac[MU_SYM_SIG_LEN];
        opcua_statuscode_t s = crypto->hmac_sha256(crypto->context, keys->signing_key,
                                                   sizeof(keys->signing_key), chunk, signed_len, mac);
        if (s != MU_STATUS_GOOD) return s;
        if (memcmp(mac, chunk + signed_len, MU_SYM_SIG_LEN) != 0) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        info->sequence_number = get_u32(chunk + MU_SYM_HEADER_SIZE);
        info->request_id = get_u32(chunk + MU_SYM_HEADER_SIZE + 4);
        size_t body_len = signed_len - MU_SYM_HEADER_SIZE - 8;
        if (body_len > out_cap) return MU_STATUS_BAD_RESPONSETOOLARGE;
        memcpy(out_body, chunk + MU_SYM_HEADER_SIZE + 8, body_len);
        *out_body_len = body_len;
        return MU_STATUS_GOOD;
    }
    if (mode != MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT) return MU_STATUS_BAD_SECURITYMODEREJECTED;

    size_t cipher_len = msg_size - MU_SYM_HEADER_SIZE;
    if (cipher_len == 0 || cipher_len % MU_SYM_BLOCK != 0) return MU_STATUS_BAD_DECODINGERROR;
    if (MU_SYM_HEADER_SIZE + cipher_len > MU_SYM_SCRATCH) return MU_STATUS_BAD_RESPONSETOOLARGE;

    /* Decrypt into a buffer prefixed with the cleartext header, so the HMAC input
       [header | plaintext-minus-signature] is contiguous. */
    opcua_byte_t buf[MU_SYM_SCRATCH];
    memcpy(buf, chunk, MU_SYM_HEADER_SIZE);
    opcua_statuscode_t s = crypto->aes256_cbc_decrypt(crypto->context, keys->encrypting_key, keys->iv,
                                                      chunk + MU_SYM_HEADER_SIZE, cipher_len,
                                                      buf + MU_SYM_HEADER_SIZE);
    if (s != MU_STATUS_GOOD) return s;

    size_t enc_len = cipher_len;
    if (enc_len < MU_SYM_SIG_LEN + 1 + 8) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    size_t signed_len = MU_SYM_HEADER_SIZE + enc_len - MU_SYM_SIG_LEN;
    opcua_byte_t mac[MU_SYM_SIG_LEN];
    s = crypto->hmac_sha256(crypto->context, keys->signing_key, sizeof(keys->signing_key), buf, signed_len, mac);
    if (s != MU_STATUS_GOOD) return s;
    if (memcmp(mac, buf + signed_len, MU_SYM_SIG_LEN) != 0) return MU_STATUS_BAD_SECURITYCHECKSFAILED;

    size_t pad_count = buf[signed_len - 1];
    if (enc_len < MU_SYM_SIG_LEN + 1 + pad_count + 8) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    size_t seqbody_len = enc_len - MU_SYM_SIG_LEN - 1 - pad_count;

    info->sequence_number = get_u32(buf + MU_SYM_HEADER_SIZE);
    info->request_id = get_u32(buf + MU_SYM_HEADER_SIZE + 4);
    size_t body_len = seqbody_len - 8;
    if (body_len > out_cap) return MU_STATUS_BAD_RESPONSETOOLARGE;
    memcpy(out_body, buf + MU_SYM_HEADER_SIZE + 8, body_len);
    *out_body_len = body_len;
    return MU_STATUS_GOOD;
}
