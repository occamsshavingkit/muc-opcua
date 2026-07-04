/* src/services/discovery.c */
#include "discovery.h"
#include <stddef.h>
#include <string.h>

/* Write a C string as an OPC UA String (NULL -> null string, length -1). */
static opcua_statuscode_t write_cstr(mu_binary_writer_t *w, const char *s) {
    if (s == NULL) {
        mu_string_t null_str = {-1, NULL};
        return mu_binary_write_string(w, &null_str);
    }
    mu_string_t str = {(opcua_int32_t)strlen(s), (const opcua_byte_t *)s};
    return mu_binary_write_string(w, &str);
}

/* Write a C string as an OPC UA LocalizedText (text only, no locale). */
static opcua_statuscode_t write_localizedtext(mu_binary_writer_t *w, const char *text) {
    if (text == NULL) {
        return mu_binary_write_byte(w, 0x00); /* no fields present */
    }
    opcua_statuscode_t s = mu_binary_write_byte(w, 0x02); /* text present */
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    return write_cstr(w, text);
}

opcua_statuscode_t mu_application_description_encode(mu_binary_writer_t *writer,
                                                     const mu_application_description_t *desc) {
    if (!writer || !desc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t s;
    s = write_cstr(writer, desc->application_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = write_cstr(writer, desc->product_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = write_localizedtext(writer, desc->application_name);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_uint32(writer, (opcua_uint32_t)desc->application_type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = write_cstr(writer, NULL);
    if (s != MU_STATUS_GOOD) {
        return s; /* gatewayServerUri */
    }
    s = write_cstr(writer, NULL);
    if (s != MU_STATUS_GOOD) {
        return s; /* discoveryProfileUri */
    }

    /* discoveryUrls: String[] (one entry: the discovery URL) */
    if (desc->discovery_url != NULL) {
        s = mu_binary_write_int32(writer, 1);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = write_cstr(writer, desc->discovery_url);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    } else {
        s = mu_binary_write_int32(writer, 0);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_endpoint_description_encode(mu_binary_writer_t *writer, const mu_endpoint_description_t *desc) {
    if (!writer || !desc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t s;
    s = write_cstr(writer, desc->endpoint_url);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_application_description_encode(writer, &desc->server);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    { /* serverCertificate (ByteString; null when the server has no certificate) */
        mu_bytestring_t cert = {desc->server_certificate ? (opcua_int32_t)desc->server_certificate_len : -1,
                                desc->server_certificate};
        s = mu_binary_write_bytestring(writer, &cert);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_uint32(writer, (opcua_uint32_t)desc->security_mode);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = write_cstr(writer, desc->security_policy_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* userIdentityTokens: UserTokenPolicy[] */
    s = mu_binary_write_int32(writer, (opcua_int32_t)desc->num_user_identity_tokens);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (size_t i = 0; i < desc->num_user_identity_tokens; ++i) {
        const mu_user_token_policy_t *t = &desc->user_identity_tokens[i];
        s = write_cstr(writer, t->policy_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_write_uint32(writer, (opcua_uint32_t)t->token_type);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = write_cstr(writer, t->issued_token_type);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = write_cstr(writer, t->issuer_endpoint_url);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = write_cstr(writer, t->security_policy_uri);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = write_cstr(writer, desc->transport_profile_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_byte(writer, desc->security_level);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_discovery_get_application_description(const mu_server_config_t *config,
                                                            mu_application_description_t *desc) {
    if (!config || !desc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    desc->application_uri = config->application_uri;
    desc->product_uri = config->product_uri;
    desc->application_name = config->application_name;
    desc->application_type = MU_APPLICATION_TYPE_SERVER;
    desc->discovery_url = config->endpoint_url;

    return MU_STATUS_GOOD;
}

static void fill_endpoint(const mu_server_config_t *config, mu_endpoint_description_t *desc,
                          mu_message_security_mode_t mode, const char *policy_uri, const opcua_byte_t *cert,
                          size_t cert_len, opcua_byte_t level) {
#ifdef MUC_OPCUA_USER_AUTH
    mu_security_policy_id_t policy = MU_SECURITY_POLICY_INVALID_ID;
    if (policy_uri != NULL) {
        policy = mu_security_policy_from_uri((const opcua_byte_t *)policy_uri, strlen(policy_uri));
    }
#endif

    desc->endpoint_url = config->endpoint_url;
    mu_discovery_get_application_description(config, &desc->server);
    desc->server_certificate = cert;
    desc->server_certificate_len = cert_len;
    desc->security_mode = mode;
    desc->security_policy_uri = policy_uri;

    desc->num_user_identity_tokens = 1;
    desc->user_identity_tokens[0].policy_id = "anonymous";
    desc->user_identity_tokens[0].token_type = MU_USER_TOKEN_TYPE_ANONYMOUS;
    desc->user_identity_tokens[0].issued_token_type = NULL;
    desc->user_identity_tokens[0].issuer_endpoint_url = NULL;
    desc->user_identity_tokens[0].security_policy_uri = NULL;

#ifdef MUC_OPCUA_USER_AUTH
    /* OPC-10000-4 sections 5.5.4.2 and 7.40.2.1: advertise username/password
       tokens only on endpoints whose SecurityPolicy can protect those secrets. */
    if (mu_security_policy_allows_username_password_tokens(policy)) {
        desc->user_identity_tokens[desc->num_user_identity_tokens].policy_id = "username";
        desc->user_identity_tokens[desc->num_user_identity_tokens].token_type = MU_USER_TOKEN_TYPE_USERNAME;
        desc->user_identity_tokens[desc->num_user_identity_tokens].issued_token_type = NULL;
        desc->user_identity_tokens[desc->num_user_identity_tokens].issuer_endpoint_url = NULL;
        desc->user_identity_tokens[desc->num_user_identity_tokens].security_policy_uri = NULL;
        desc->num_user_identity_tokens++;
    }
#endif

    desc->transport_profile_uri = "http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary";
    desc->security_level = level;
}

opcua_statuscode_t mu_discovery_get_endpoint_description(const mu_server_config_t *config,
                                                         mu_endpoint_description_t *desc) {
    if (!config || !desc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    fill_endpoint(config, desc, MU_MESSAGE_SECURITY_MODE_NONE, mu_security_policy_uri(MU_SECURITY_POLICY_NONE_ID), NULL,
                  0, 0);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_discovery_get_endpoints(const mu_server_config_t *config, mu_endpoint_description_t *eps,
                                              size_t max, size_t *count) {
    if (!config || !eps || !count || max == 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    bool have_crypto = config->crypto_adapter != NULL && config->crypto_adapter->get_own_certificate != NULL &&
                       config->crypto_adapter->get_own_certificate(config->crypto_adapter->context, &cert, &cert_len) ==
                           MU_STATUS_GOOD;

    const char *none_uri = mu_security_policy_uri(MU_SECURITY_POLICY_NONE_ID);
#ifdef MUC_OPCUA_SECURITY
    const char *b256_uri = mu_security_policy_uri(MU_SECURITY_POLICY_BASIC256SHA256_ID);
    const char *aes128_uri = mu_security_policy_uri(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID);
#endif
    size_t n = 0;

    fill_endpoint(config, &eps[n++], MU_MESSAGE_SECURITY_MODE_NONE, none_uri, have_crypto ? cert : NULL,
                  have_crypto ? cert_len : 0, 0);
#ifdef MUC_OPCUA_SECURITY
    if (have_crypto) {
        if (n < max) {
            fill_endpoint(config, &eps[n++], MU_MESSAGE_SECURITY_MODE_SIGN, b256_uri, cert, cert_len, 1);
        }
        if (n < max) {
            fill_endpoint(config, &eps[n++], MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, b256_uri, cert, cert_len, 2);
        }
        if (n < max) {
            fill_endpoint(config, &eps[n++], MU_MESSAGE_SECURITY_MODE_SIGN, aes128_uri, cert, cert_len, 3);
        }
        if (n < max) {
            fill_endpoint(config, &eps[n++], MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, aes128_uri, cert, cert_len, 4);
        }
    }
#endif
    *count = n;
    return MU_STATUS_GOOD;
}
