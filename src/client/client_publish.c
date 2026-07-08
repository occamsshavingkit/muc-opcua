/* src/client/client_publish.c
 * Client-side Publish request parking and notification dispatch.
 * OPC-10000-4 §5.14.5 */
#include "client_internal.h"
#include "muc_opcua/client.h"
#include "muc_opcua/status.h"

opcua_statuscode_t mu_client_poll(mu_client_t *client) {
    if (client == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}
