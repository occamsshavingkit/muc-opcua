/* src/services/secure_channel.h */
#ifndef MICRO_OPCUA_SERVICES_SECURE_CHANNEL_H
#define MICRO_OPCUA_SERVICES_SECURE_CHANNEL_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/types.h"
#include "micro_opcua/status.h"
#include "../core/sequence.h"
#include "../security/security_policy.h"   /* mu_security_policy_id_t, mu_message_security_mode_t */
#ifdef MICRO_OPCUA_SECURITY
#include "../security/sym_chunk.h"          /* mu_sym_keys_t */
#endif

typedef struct {
    opcua_uint32_t channel_id;
    opcua_uint32_t token_id;
    opcua_uint32_t created_at; /* simplistic time for now */
    opcua_uint32_t revised_lifetime;

    mu_sequence_validator_t sequence;       /* validates inbound SequenceNumbers */
    opcua_uint32_t out_sequence_number;     /* monotonic SequenceNumber for responses */
    bool is_open;

    /* Negotiated security (None unless a Basic256Sha256 OPN established keys). */
    mu_security_policy_id_t policy;
    mu_message_security_mode_t mode;
#ifdef MICRO_OPCUA_SECURITY
    mu_sym_keys_t client_keys;   /* verify/decrypt inbound MSG (client->server) */
    mu_sym_keys_t server_keys;   /* sign/encrypt outbound MSG (server->client) */
    bool keys_valid;
#endif
} mu_secure_channel_t;

#define MU_SECURITY_POLICY_NONE "http://opcfoundation.org/UA/SecurityPolicy#None"

void mu_secure_channel_init(mu_secure_channel_t *channel);

opcua_statuscode_t mu_secure_channel_open(mu_secure_channel_t *channel, 
                                          const mu_string_t *security_policy,
                                          opcua_uint32_t requested_lifetime, 
                                          opcua_uint32_t *revised_lifetime);

opcua_statuscode_t mu_secure_channel_close(mu_secure_channel_t *channel);

#endif /* MICRO_OPCUA_SERVICES_SECURE_CHANNEL_H */
