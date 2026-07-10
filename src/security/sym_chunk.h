/* src/security/sym_chunk.h
 * Symmetric (MSG/CLO) message-chunk protection for SecurityPolicy Basic256Sha256
 * (OPC 10000-6 §6.7.2). The SymmetricAlgorithmSecurityHeader (TokenId) is sent in
 * cleartext; the SequenceHeader + body that follow are signed with HMAC-SHA256 and,
 * for SignAndEncrypt, encrypted with AES-256-CBC using keys derived from the
 * channel nonces (§6.7.5). Sign-then-encrypt: the signature is inside the
 * encrypted payload. */
#ifndef MUC_OPCUA_SYM_CHUNK_H
#define MUC_OPCUA_SYM_CHUNK_H

#include "muc_opcua/config.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include "security_policy.h"
#include <stddef.h>

/* Per-direction channel keys derived via P-SHA256 (OPC 10000-6 §6.7.5). */
typedef struct {
    mu_security_policy_id_t policy;
    opcua_byte_t signing_key[MU_B256S256_SIGNATURE_KEY_LENGTH];
    opcua_byte_t encrypting_key[MU_B256S256_ENCRYPTION_KEY_LENGTH];
    opcua_byte_t iv[MU_B256S256_ENCRYPTION_BLOCK_SIZE];
    opcua_byte_t cipher_ctx[MU_CIPHER_CTX_SIZE];
    const mu_crypto_adapter_t *crypto;
    bool cipher_ctx_valid;
} mu_sym_keys_t;

typedef struct {
    mu_message_security_mode_t mode;
    opcua_uint32_t secure_channel_id;
    opcua_uint32_t token_id;
    opcua_uint32_t sequence_number;
    opcua_uint32_t request_id;
    char message_type[3]; /* "MSG" or "CLO" */
} mu_sym_chunk_info_t;

/* Derive a key set from a (secret, seed) nonce pair: signing key || encrypting
   key || IV, split from dynamic key material lengths of P-SHA256 output. */
opcua_statuscode_t mu_sym_keys_derive(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                      const opcua_byte_t *secret, size_t secret_len, const opcua_byte_t *seed,
                                      size_t seed_len, mu_sym_keys_t *out_keys);

void mu_sym_keys_prepare_cipher(mu_sym_keys_t *keys, const mu_crypto_adapter_t *crypto);
void mu_sym_keys_release_cipher(mu_sym_keys_t *keys);

/* Build a symmetric chunk (message_type is "MSG" or "CLO") carrying `body` (an
   encoded service message, without the SequenceHeader). Signs (Sign) or
   signs+encrypts (SignAndEncrypt) with `keys`. */
opcua_statuscode_t mu_sym_chunk_wrap(const mu_crypto_adapter_t *crypto, mu_message_security_mode_t mode,
                                     const mu_sym_keys_t *keys, const char message_type[3],
                                     opcua_uint32_t secure_channel_id, opcua_uint32_t token_id,
                                     opcua_uint32_t sequence_number, opcua_uint32_t request_id,
                                     const opcua_byte_t *body, size_t body_len, opcua_byte_t *out, size_t out_cap,
                                     size_t *out_len);

/* Parse, decrypt (in place), and verify a symmetric chunk, recovering the body
   (without the SequenceHeader). `chunk` is mutable and is decrypted in place;
   `*out_body` points into it (no copy, no scratch buffer). `mode` is the expected
   security mode. */
opcua_statuscode_t mu_sym_chunk_unwrap(const mu_crypto_adapter_t *crypto, mu_message_security_mode_t mode,
                                       const mu_sym_keys_t *keys, opcua_byte_t *chunk, size_t chunk_len,
                                       const opcua_byte_t **out_body, size_t *out_body_len, mu_sym_chunk_info_t *info);

#ifdef MUC_OPCUA_ECC
/* AEAD (ChaCha20-Poly1305) symmetric chunk protection for the ECC-curve25519
   SecurityPolicy (OPC-10000-6 §6.7.2.5.2). Unlike the CBC+HMAC path there is no
   padding: [Header | TokenId] [enc: SequenceHeader+body] [16-byte tag]. The
   per-chunk IV is keys->iv with its first 8 bytes XORed with TokenId||last_seq
   (Table 69, §6.8.1); `last_sequence_number` is the previous chunk's sequence
   number in this direction (0 for the first). Always Sign+Encrypt (AEAD). */
opcua_statuscode_t mu_sym_chunk_wrap_aead(const mu_crypto_adapter_t *crypto, const mu_sym_keys_t *keys,
                                          const char message_type[3], opcua_uint32_t secure_channel_id,
                                          opcua_uint32_t token_id, opcua_uint32_t sequence_number,
                                          opcua_uint32_t last_sequence_number, opcua_uint32_t request_id,
                                          const opcua_byte_t *body, size_t body_len, opcua_byte_t *out, size_t out_cap,
                                          size_t *out_len);

opcua_statuscode_t mu_sym_chunk_unwrap_aead(const mu_crypto_adapter_t *crypto, const mu_sym_keys_t *keys,
                                            opcua_byte_t *chunk, size_t chunk_len, opcua_uint32_t last_sequence_number,
                                            const opcua_byte_t **out_body, size_t *out_body_len,
                                            mu_sym_chunk_info_t *info);
#endif /* MUC_OPCUA_ECC */

#endif /* MUC_OPCUA_SYM_CHUNK_H */
