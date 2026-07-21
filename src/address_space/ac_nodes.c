/* src/address_space/ac_nodes.c — Alarms & Conditions type node definitions.
   Placeholder: type nodes are not yet registered in the address space.
   OPC-10000-9 §5. */
#include "ac_nodes.h"

#ifdef MUC_OPCUA_CU_ALARMS_CONDITIONS

opcua_statuscode_t mu_ac_register_type_nodes(mu_server_t *server) {
    (void)server;
    /* Type nodes deferred to a follow-up task.
       In the interim, the A&C method dispatch and condition state
       tracking operate without a full AddressSpace type hierarchy. */
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_CU_ALARMS_CONDITIONS */
