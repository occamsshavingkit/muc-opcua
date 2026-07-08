/* src/client/client_monitored_item.c
 * Client-side MonitoredItem lifecycle: CreateMonitoredItems and DeleteMonitoredItems.
 * OPC-10000-4 §5.13.2, §5.13.6 */
#include "client_internal.h"
#include "muc_opcua/client.h"
#include "muc_opcua/status.h"

opcua_statuscode_t mu_client_create_monitored_item(mu_client_t *client, opcua_uint32_t subscription_id,
                                                   const mu_client_monitored_item_create_t *params,
                                                   opcua_uint32_t *out_monitored_item_id) {
    if (client == NULL || params == NULL || out_monitored_item_id == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)subscription_id;
    *out_monitored_item_id = 0;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_client_delete_monitored_item(mu_client_t *client, opcua_uint32_t subscription_id,
                                                   opcua_uint32_t monitored_item_id) {
    if (client == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)subscription_id;
    (void)monitored_item_id;
    return MU_STATUS_GOOD;
}
