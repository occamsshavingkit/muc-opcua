/* src/services/discovery.c */
#include "discovery.h"
#include <stddef.h>

opcua_statuscode_t mu_discovery_get_application_description(const mu_server_config_t *config, 
                                                            mu_application_description_t *desc)
{
    if (!config || !desc) return MU_STATUS_BAD_INTERNALERROR;

    desc->application_uri = config->application_uri;
    desc->product_uri = config->product_uri;
    desc->application_name = config->application_name;
    desc->application_type = MU_APPLICATION_TYPE_SERVER;
    desc->discovery_url = config->endpoint_url;

    return MU_STATUS_GOOD;
}
