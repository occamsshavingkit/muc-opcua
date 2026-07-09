/* src/services/secure_channel.h */
#ifndef MUC_OPCUA_SERVICES_SECURE_CHANNEL_H
#define MUC_OPCUA_SERVICES_SECURE_CHANNEL_H

#include "../core/sequence.h"
#include "../security/security_policy.h" /* mu_security_policy_id_t, mu_message_security_mode_t */
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/platform.h" /* mu_entropy_adapter_t */
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"
#ifdef MUC_OPCUA_SECURITY
#include "../security/sym_chunk.h" /* mu_sym_keys_t */
#endif

typedef struct {
    opcua_uint32_t channel_id;
    opcua_uint32_t token_id;
    opcua_uint32_t created_at; /* simplistic time for now */
    opcua_uint32_t revised_lifetime;

    mu_sequence_validator_t sequence;   /* validates inbound SequenceNumbers */
    opcua_uint32_t out_sequence_number; /* monotonic SequenceNumber for responses */
    /* Previous inbound (client->server) SequenceNumber, for the AEAD per-chunk
       nonce of the ECC-curve25519 SecurityPolicy (OPC-10000-6 §6.8.1, Table 69:
       IV XOR TokenId||LastSequenceNumber). Unused by the CBC/None paths. */
    opcua_uint32_t in_last_sequence_number;
    bool is_open;

    /* Negotiated security (None unless a Basic256Sha256 OPN established keys). */
    mu_security_policy_id_t policy;
    mu_message_security_mode_t mode;
#ifdef MUC_OPCUA_SECURITY
    mu_sym_keys_t client_keys; /* verify/decrypt inbound MSG (client->server) */
    mu_sym_keys_t server_keys; /* sign/encrypt outbound MSG (server->client) */
    bool keys_valid;
#endif
} mu_secure_channel_t;

#define MU_SECURITY_POLICY_NONE "http://opcfoundation.org/UA/SecurityPolicy#None"

void mu_secure_channel_init(mu_secure_channel_t *channel);

opcua_statuscode_t mu_secure_channel_open(mu_secure_channel_t *channel, const mu_string_t *security_policy,
                                          mu_message_security_mode_t security_mode, opcua_uint32_t requested_lifetime,
                                          const mu_entropy_adapter_t *entropy, opcua_uint32_t *revised_lifetime);

opcua_statuscode_t mu_secure_channel_close(mu_secure_channel_t *channel);

#endif /* MUC_OPCUA_SERVICES_SECURE_CHANNEL_H */
