/* src/client/client_write.c
 * Client-side Write service: OPC-10000-4 §5.11.4 */
#include "client_internal.h"
#include "muc_opcua/client.h"
#include "muc_opcua/status.h"

opcua_statuscode_t mu_client_write(mu_client_t *client, const mu_write_item_t *items, size_t num_items,
                                   opcua_statuscode_t *item_statuses) {
    if (client == NULL || items == NULL || item_statuses == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)num_items;
    return MU_STATUS_GOOD;
}
