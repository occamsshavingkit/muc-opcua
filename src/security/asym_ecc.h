/* src/security/asym_ecc.h
 * ECC SecurityPolicy handshake helpers (spec 059): ephemeral-ECDH channel key
 * derivation per OPC-10000-6 §6.8.1. Compiled only under MUC_OPCUA_CU_SECURITY_ECC. */
#ifndef MUC_OPCUA_ASYM_ECC_H
#define MUC_OPCUA_ASYM_ECC_H

#include "muc_opcua/config.h"

#ifdef MUC_OPCUA_CU_SECURITY_ECC

#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include "security_policy.h"
#include "sym_chunk.h"

/* Derive one direction's symmetric channel keys from the ECDH shared secret via
   HKDF-SHA256 (OPC-10000-6 §6.8.1). With `is_server_direction` true the keys
   protect Server->Client traffic using
     ServerSalt = L(2 LE) | UTF8("opcua-server") | ServerNonce | ClientNonce;
   false derives the Client->Server keys with
     ClientSalt = L | UTF8("opcua-client") | ClientNonce | ServerNonce.
   The salt doubles as the HKDF info. L is the total derived key length (signing +
   encryption + IV) for `policy`. The nonces are the peers' ephemeral public keys. */
opcua_statuscode_t mu_ecc_derive_channel_keys(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                              const opcua_byte_t *shared, size_t shared_len, int is_server_direction,
                                              const opcua_byte_t *server_nonce, size_t server_nonce_len,
                                              const opcua_byte_t *client_nonce, size_t client_nonce_len,
                                              mu_sym_keys_t *out_keys);

#endif /* MUC_OPCUA_CU_SECURITY_ECC */
#endif /* MUC_OPCUA_ASYM_ECC_H */
