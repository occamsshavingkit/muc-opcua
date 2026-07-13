/* src/services/secure_channel.c */
#include "secure_channel.h"
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
#include "../security/key_derivation.h"
#endif
#include <stddef.h>

static mu_security_policy_id_t requested_policy_id(const mu_string_t *security_policy) {
    if (security_policy == NULL || security_policy->length <= 0) {
        return MU_SECURITY_POLICY_NONE_ID;
    }
    if (security_policy->data == NULL) {
        return MU_SECURITY_POLICY_INVALID_ID;
    }
    return mu_security_policy_from_uri(security_policy->data, (size_t)security_policy->length);
}

static bool is_valid_security_mode(mu_message_security_mode_t security_mode) {
    return security_mode == MU_MESSAGE_SECURITY_MODE_NONE || security_mode == MU_MESSAGE_SECURITY_MODE_SIGN ||
           security_mode == MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT;
}

/* OPC-10000-6 section 6.7.2.2: "When a Server starts the first SecureChannelId
   used should be a value that is likely to be unique after each restart." The
   identifier is therefore drawn from the entropy adapter rather than a static
   incrementing counter (which is predictable on reboot). 0 is reserved for an
   unset/invalid id (it is the value carried by an OpenSecureChannel request). */
static opcua_statuscode_t generate_channel_id(const mu_entropy_adapter_t *entropy, opcua_uint32_t *channel_id) {
    if (entropy == NULL || entropy->generate_random == NULL || channel_id == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_byte_t random[sizeof(opcua_uint32_t)];
    if (entropy->generate_random(entropy->context, random, sizeof(random)) != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_uint32_t candidate = ((opcua_uint32_t)random[0]) | ((opcua_uint32_t)random[1] << 8) |
                               ((opcua_uint32_t)random[2] << 16) | ((opcua_uint32_t)random[3] << 24);
    if (candidate == 0u) {
        candidate = 1u; /* 0 reserved for invalid; 2^-32 collision folded to 1 */
    }
    *channel_id = candidate;
    return MU_STATUS_GOOD;
}

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
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
        mu_secure_zero(&channel->client_keys, sizeof(channel->client_keys));
        mu_secure_zero(&channel->server_keys, sizeof(channel->server_keys));
        channel->keys_valid = false;
#endif
        mu_sequence_validator_init(&channel->sequence);
    }
}

opcua_statuscode_t mu_secure_channel_open(mu_secure_channel_t *channel, const mu_string_t *security_policy,
                                          mu_message_security_mode_t security_mode, opcua_uint32_t requested_lifetime,
                                          const mu_entropy_adapter_t *entropy, opcua_uint32_t *revised_lifetime) {
    if (!channel || !revised_lifetime) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* OPC-10000-4 §5.6.2.2 carries SecurityPolicyUri in the OPN header and
       securityMode in the request body; §5.6.2.3 requires explicit rejection
       when either value does not meet the server's active security capability. */
    mu_security_policy_id_t requested_policy = requested_policy_id(security_policy);
    if (requested_policy == MU_SECURITY_POLICY_INVALID_ID) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }
    if (!is_valid_security_mode(security_mode)) {
        return MU_STATUS_BAD_SECURITYMODEREJECTED;
    }

    if (channel->policy == MU_SECURITY_POLICY_NONE_ID) {
        if (requested_policy != MU_SECURITY_POLICY_NONE_ID) {
            return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
        }
        if (security_mode != MU_MESSAGE_SECURITY_MODE_NONE) {
            return MU_STATUS_BAD_SECURITYMODEREJECTED;
        }
    }
#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
    else if (channel->policy == MU_SECURITY_POLICY_BASIC256SHA256_ID ||
             channel->policy == MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID ||
             channel->policy == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
        if (requested_policy != channel->policy) {
            return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
        }
        if (security_mode == MU_MESSAGE_SECURITY_MODE_NONE) {
            return MU_STATUS_BAD_SECURITYMODEREJECTED;
        }
    }
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    /* ECC SecurityPolicies (spec 059): same discipline — the requested URI must
       match the channel's classified policy and a real security mode is required
       (ECC never runs None). */
    else if (mu_security_policy_asym_family(channel->policy) == MU_ASYM_FAMILY_ECC) {
        if (requested_policy != channel->policy) {
            return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
        }
        if (security_mode == MU_MESSAGE_SECURITY_MODE_NONE) {
            return MU_STATUS_BAD_SECURITYMODEREJECTED;
        }
    }
#endif
#endif
    else {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }

    /* Assign a cryptographically random SecureChannelId on ISSUE (Renew keeps
       the existing id and only rolls the token). With MU_INTERN_MAX_CONNECTIONS=1 the
       id need only be non-zero and restart-unique per OPC-10000-6 section 6.7.2.2. */
    if (!channel->is_open) {
        opcua_statuscode_t id_status = generate_channel_id(entropy, &channel->channel_id);
        if (id_status != MU_STATUS_GOOD) {
            return id_status;
        }
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
    if (lifetime < 10000) {
        lifetime = 10000;
    }
    if (lifetime > 3600000) {
        lifetime = 3600000;
    }

    channel->revised_lifetime = lifetime;
    *revised_lifetime = lifetime;
    channel->mode = security_mode;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_secure_channel_close(mu_secure_channel_t *channel) {
    if (!channel) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (!channel->is_open) {
        return MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN;
    }

#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
    mu_sym_keys_release_cipher(&channel->client_keys);
    mu_sym_keys_release_cipher(&channel->server_keys);
    mu_secure_zero(&channel->client_keys, sizeof(channel->client_keys));
    mu_secure_zero(&channel->server_keys, sizeof(channel->server_keys));
    channel->keys_valid = false;
#endif
    mu_secure_channel_init(channel); /* Reset */
    return MU_STATUS_GOOD;
}
