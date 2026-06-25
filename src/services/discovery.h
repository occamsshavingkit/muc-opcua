/* src/services/discovery.h */
#ifndef MICRO_OPCUA_SERVICES_DISCOVERY_H
#define MICRO_OPCUA_SERVICES_DISCOVERY_H

#include "micro_opcua/server.h"

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

#endif /* MICRO_OPCUA_SERVICES_DISCOVERY_H */
