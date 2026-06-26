/* src/services/discovery.h */
#ifndef MICRO_OPCUA_SERVICES_DISCOVERY_H
#define MICRO_OPCUA_SERVICES_DISCOVERY_H

#include "micro_opcua/server.h"
#include "micro_opcua/encoding.h"
#include "../security/security_policy.h"  /* mu_message_security_mode_t */

typedef enum {
    MU_APPLICATION_TYPE_SERVER = 0,
    MU_APPLICATION_TYPE_CLIENT = 1,
    MU_APPLICATION_TYPE_CLIENTANDSERVER = 2,
    MU_APPLICATION_TYPE_DISCOVERYSERVER = 3
} mu_application_type_t;

typedef struct {
    const char *application_uri;
    const char *product_uri;
    const char *application_name;
    mu_application_type_t application_type;
    const char *discovery_url;
} mu_application_description_t;

opcua_statuscode_t mu_discovery_get_application_description(const mu_server_config_t *config, 
                                                            mu_application_description_t *desc);

/* MessageSecurityMode is defined in security_policy.h (included above). */

/* UserTokenType */
typedef enum {
    MU_USER_TOKEN_TYPE_ANONYMOUS = 0,
    MU_USER_TOKEN_TYPE_USERNAME = 1,
    MU_USER_TOKEN_TYPE_CERTIFICATE = 2,
    MU_USER_TOKEN_TYPE_ISSUEDTOKEN = 3
} mu_user_token_type_t;

typedef struct {
    const char *policy_id;
    mu_user_token_type_t token_type;
    const char *issued_token_type;
    const char *issuer_endpoint_url;
    const char *security_policy_uri;
} mu_user_token_policy_t;

typedef struct {
    const char *endpoint_url;
    mu_application_description_t server;
    const opcua_byte_t *server_certificate;   /* DER; NULL if none */
    size_t server_certificate_len;
    mu_message_security_mode_t security_mode;
    const char *security_policy_uri;

    mu_user_token_policy_t user_identity_tokens[1]; /* only 1 supported: Anonymous */
    size_t num_user_identity_tokens;

    const char *transport_profile_uri;
    opcua_byte_t security_level;
} mu_endpoint_description_t;

/* Up to None + Basic256Sha256(Sign) + Basic256Sha256(SignAndEncrypt). */
#define MU_DISCOVERY_MAX_ENDPOINTS 3

opcua_statuscode_t mu_discovery_get_endpoint_description(const mu_server_config_t *config,
                                                         mu_endpoint_description_t *desc);

/* Fill the array of endpoints the server advertises: the None endpoint always,
   plus Basic256Sha256 Sign and SignAndEncrypt when a crypto adapter is present. */
opcua_statuscode_t mu_discovery_get_endpoints(const mu_server_config_t *config,
                                              mu_endpoint_description_t *eps, size_t max, size_t *count);

/* OPC UA Binary encoders (OPC 10000-4 7.2, 7.14, 7.41). */
opcua_statuscode_t mu_application_description_encode(mu_binary_writer_t *writer,
                                                     const mu_application_description_t *desc);
opcua_statuscode_t mu_endpoint_description_encode(mu_binary_writer_t *writer,
                                                  const mu_endpoint_description_t *desc);

#endif /* MICRO_OPCUA_SERVICES_DISCOVERY_H */
