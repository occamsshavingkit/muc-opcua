/* src/services/secure_channel.c */
#include "secure_channel.h"
#include <stddef.h>
#include <string.h>

void mu_secure_channel_init(mu_secure_channel_t *channel) {
    if (channel) {
        channel->channel_id = 0;
        channel->token_id = 0;
        channel->created_at = 0;
        channel->revised_lifetime = 0;
        channel->is_open = false;
        channel->out_sequence_number = 0;
        channel->policy = MU_SECURITY_POLICY_NONE_ID;
        channel->mode = MU_MESSAGE_SECURITY_MODE_NONE;
#ifdef MICRO_OPCUA_SECURITY
        channel->keys_valid = false;
#endif
        mu_sequence_validator_init(&channel->sequence);
    }
}

opcua_statuscode_t mu_secure_channel_open(mu_secure_channel_t *channel, 
                                          const mu_string_t *security_policy,
                                          opcua_uint32_t requested_lifetime, 
                                          opcua_uint32_t *revised_lifetime) 
{
    if (!channel || !revised_lifetime) return MU_STATUS_BAD_INTERNALERROR;

    if (security_policy != NULL && security_policy->length > 0) {
        if (security_policy->length != 47 || strncmp((const char*)security_policy->data, MU_SECURITY_POLICY_NONE, 47) != 0) {
            return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
        }
    }

    /* In a real implementation we would generate a random channel ID and token ID */
    if (!channel->is_open) {
        channel->channel_id = 1;
        channel->token_id = 1;
        /* The inbound SequenceNumber is validated channel-wide starting from the
           OPN chunk; the validator is reset per connection in mu_secure_channel_init,
           not here (resetting here would skip the gap check on the first MSG). */
        channel->is_open = true;
    } else {
        /* Renew */
        channel->token_id++;
    }

    /* Bounded lifetime */
    opcua_uint32_t lifetime = requested_lifetime;
    if (lifetime < 10000) lifetime = 10000;
    if (lifetime > 3600000) lifetime = 3600000;

    channel->revised_lifetime = lifetime;
    *revised_lifetime = lifetime;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_secure_channel_close(mu_secure_channel_t *channel) {
    if (!channel) return MU_STATUS_BAD_INTERNALERROR;
    if (!channel->is_open) return MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN;

    mu_secure_channel_init(channel); /* Reset */
    return MU_STATUS_GOOD;
}
