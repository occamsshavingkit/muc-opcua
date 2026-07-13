/* src/security/asym_ecc.c
 * ECC SecurityPolicy handshake helpers (spec 059): ephemeral-ECDH channel key
 * derivation (OPC-10000-6 §6.8.1). Compiled only under MUC_OPCUA_CU_SECURITY_ECC. This is a
 * dedicated, droppable translation unit so profiles without ECC pay no .text. */
#include "security/asym_ecc.h"

#ifdef MUC_OPCUA_CU_SECURITY_ECC

#include "security/key_derivation.h"
#include <string.h>

/* Salt/info scratch: L(2) + longest label(12, "opcua-server") + two nonces (each
   an ephemeral public key, <= MU_ECC_MAX_PUBKEY_LENGTH). */
#define MU_ECC_SALT_MAX (2u + 12u + 2u * MU_ECC_MAX_PUBKEY_LENGTH)

/* ServerSalt/ClientSalt = L(2 LE) | label | first_nonce | second_nonce. */
static size_t build_salt(opcua_uint16_t L, const char *label, size_t label_len, const opcua_byte_t *n1, size_t n1_len,
                         const opcua_byte_t *n2, size_t n2_len, opcua_byte_t *out) {
    size_t off = 0;
    out[off++] = (opcua_byte_t)(L & 0xFFu);
    out[off++] = (opcua_byte_t)((L >> 8) & 0xFFu);
    (void)memcpy(out + off, label, label_len);
    off += label_len;
    (void)memcpy(out + off, n1, n1_len);
    off += n1_len;
    (void)memcpy(out + off, n2, n2_len);
    off += n2_len;
    return off;
}

opcua_statuscode_t mu_ecc_derive_channel_keys(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                              const opcua_byte_t *shared, size_t shared_len, int is_server_direction,
                                              const opcua_byte_t *server_nonce, size_t server_nonce_len,
                                              const opcua_byte_t *client_nonce, size_t client_nonce_len,
                                              mu_sym_keys_t *out_keys) {
    if (!crypto || !shared || !server_nonce || !client_nonce || !out_keys) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    (void)memset(out_keys, 0, sizeof(*out_keys));
    out_keys->policy = policy;

    size_t sig_key_len = mu_security_policy_signature_key_length(policy);
    size_t enc_key_len = mu_security_policy_encryption_key_length(policy);
    size_t iv_len = mu_security_policy_iv_length(policy);
    size_t L = sig_key_len + enc_key_len + iv_len;
    if (sig_key_len > sizeof(out_keys->signing_key) || enc_key_len > sizeof(out_keys->encrypting_key) ||
        iv_len > sizeof(out_keys->iv) || L > MU_ECC_NISTP256_DERIVED_KEY_LENGTH || L > 0xFFFFu) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (server_nonce_len > MU_ECC_MAX_PUBKEY_LENGTH || client_nonce_len > MU_ECC_MAX_PUBKEY_LENGTH) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* ServerSalt orders the nonces (Server, Client); ClientSalt (Client, Server). */
    const char *label = is_server_direction ? "opcua-server" : "opcua-client";
    const opcua_byte_t *n1 = is_server_direction ? server_nonce : client_nonce;
    size_t n1_len = is_server_direction ? server_nonce_len : client_nonce_len;
    const opcua_byte_t *n2 = is_server_direction ? client_nonce : server_nonce;
    size_t n2_len = is_server_direction ? client_nonce_len : server_nonce_len;

    opcua_byte_t salt[MU_ECC_SALT_MAX];
    size_t salt_len = build_salt((opcua_uint16_t)L, label, strlen(label), n1, n1_len, n2, n2_len, salt);

    /* Salt doubles as the HKDF info (OPC-10000-6 §6.8.1). */
    opcua_byte_t material[MU_ECC_NISTP256_DERIVED_KEY_LENGTH];
    opcua_statuscode_t s = mu_hkdf_sha256(crypto, salt, salt_len, shared, shared_len, salt, salt_len, material, L);
    if (s != MU_STATUS_GOOD) {
        mu_secure_zero(material, sizeof(material));
        mu_secure_zero(salt, sizeof(salt));
        return s;
    }
    (void)memcpy(out_keys->signing_key, material, sig_key_len);
    (void)memcpy(out_keys->encrypting_key, material + sig_key_len, enc_key_len);
    (void)memcpy(out_keys->iv, material + sig_key_len + enc_key_len, iv_len);
    mu_secure_zero(material, sizeof(material));
    mu_secure_zero(salt, sizeof(salt));
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_CU_SECURITY_ECC */
