/* src/platform/posix_mdns.h
 * POSIX host mDNS adapter — UDP multicast DNS-SD publishing.
 * Available only on POSIX platforms (Linux, macOS). */
#ifndef MUC_OPCUA_PLATFORM_POSIX_MDNS_H
#define MUC_OPCUA_PLATFORM_POSIX_MDNS_H

#include "muc_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the host POSIX mDNS adapter (UDP multicast, 224.0.0.251:5353).
 * Creates a UDP socket, joins the multicast group, and stores the socket fd
 * in adapter->context. Returns MU_STATUS_GOOD on success. On failure, returns
 * an error status — the caller should not pass a failed adapter to config. */
opcua_statuscode_t mu_host_mdns_adapter_init(mu_mdns_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_PLATFORM_POSIX_MDNS_H */
