#include "muc_opcua/server.h"

#ifndef MUC_OPCUA_SERVICE_WRITE
#error "canonical Write CU must expose the Write public API"
#endif

#ifndef MUC_OPCUA_EVENTS
#error "canonical Events CU must expose the Events public API"
#endif

#ifndef MUC_OPCUA_DATA_ACCESS
#error "canonical Data Access CU must expose the Data Access public API"
#endif

#ifndef MUC_OPCUA_METHOD_SERVER
#error "canonical Method Server CU must expose the Method Server public API"
#endif

static mu_write_value_t write_value;
static mu_range_t range_value;

void canonical_public_api_gate_probe(void) {
    (void)write_value;
    (void)range_value;
    (void)mu_server_trigger_event;
    (void)mu_server_register_method_callback;
}
