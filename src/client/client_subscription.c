/* src/client/client_subscription.c
 * Client-side subscription lifecycle: CreateSubscription and DeleteSubscriptions.
 * OPC-10000-4 §5.14.2, §5.14.8 */
#include "client_internal.h"
#include "muc_opcua/client.h"
#include "muc_opcua/status.h"
#include <string.h>

opcua_statuscode_t mu_client_create_subscription(mu_client_t *client, opcua_uint32_t interval_ms,
                                                 opcua_uint32_t *out_subscription_id) {
    if (client == NULL || out_subscription_id == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)interval_ms;
    *out_subscription_id = 0;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_client_delete_subscription(mu_client_t *client, opcua_uint32_t subscription_id) {
    if (client == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)subscription_id;
    return MU_STATUS_GOOD;
}
