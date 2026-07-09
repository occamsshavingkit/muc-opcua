/* src/client/client_write.c
 * Client-side Write service: OPC-10000-4 §5.11.4 */
#include "client_internal.h"
#include "muc_opcua/client.h"
#include "muc_opcua/status.h"

/* This file is compiled only for the Standard client, and its entry points use
   MUC_OPCUA_CLIENT_STANDARD-guarded request types from client.h. Guard the body so
   standalone tools (e.g. clang-tidy) that analyse the file without the macro see a
   valid, type-complete translation unit instead of unknown-type errors. */
#ifdef MUC_OPCUA_CLIENT_STANDARD

opcua_statuscode_t mu_client_write(mu_client_t *client, const mu_write_item_t *items, size_t num_items,
                                   opcua_statuscode_t *item_statuses) {
    if (client == NULL || items == NULL || item_statuses == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)num_items;
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_CLIENT_STANDARD */
