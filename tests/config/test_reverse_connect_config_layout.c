#include <stddef.h>

#include "muc_opcua/server.h"

#ifndef EXPECT_REVERSE_CONNECT_FIELD
#if defined(MUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER) && MUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER
#define EXPECT_REVERSE_CONNECT_FIELD 1
#else
#define EXPECT_REVERSE_CONNECT_FIELD 0
#endif
#endif

#if EXPECT_REVERSE_CONNECT_FIELD
_Static_assert(offsetof(mu_server_config_t, application_uri) ==
                   offsetof(mu_server_config_t, reverse_connect_url) + sizeof(const char *),
               "enabled Reverse Connect must expose reverse_connect_url");
#else
_Static_assert(offsetof(mu_server_config_t, application_uri) ==
                   offsetof(mu_server_config_t, endpoint_url) + sizeof(const char *),
               "disabled Reverse Connect must omit reverse_connect_url");
#endif
