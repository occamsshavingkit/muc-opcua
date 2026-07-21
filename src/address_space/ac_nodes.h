#ifndef MUC_OPCUA_AC_NODES_H
#define MUC_OPCUA_AC_NODES_H

#include "muc_opcua/server.h"

#ifdef MUC_OPCUA_CU_ALARMS_CONDITIONS

opcua_statuscode_t mu_ac_register_type_nodes(mu_server_t *server);

#endif /* MUC_OPCUA_CU_ALARMS_CONDITIONS */

#endif /* MUC_OPCUA_AC_NODES_H */
