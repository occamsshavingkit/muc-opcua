/* src/services/secure_channel.h */
#ifndef MICRO_OPCUA_SERVICES_SECURE_CHANNEL_H
#define MICRO_OPCUA_SERVICES_SECURE_CHANNEL_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include "../core/sequence.h"

typedef struct {
    opcua_uint32_t channel_id;
    opcua_uint32_t token_id;
    opcua_uint32_t created_at; /* simplistic time for now */
    opcua_uint32_t revised_lifetime;
    
    mu_sequence_validator_t sequence;
    bool is_open;
} mu_secure_channel_t;

#define MU_SECURITY_POLICY_NONE "http://opcfoundation.org/UA/SecurityPolicy#None"

void mu_secure_channel_init(mu_secure_channel_t *channel);

opcua_statuscode_t mu_secure_channel_open(mu_secure_channel_t *channel, 
                                          const mu_string_t *security_policy,
                                          opcua_uint32_t requested_lifetime, 
                                          opcua_uint32_t *revised_lifetime);

opcua_statuscode_t mu_secure_channel_close(mu_secure_channel_t *channel);

#endif /* MICRO_OPCUA_SERVICES_SECURE_CHANNEL_H */
