/* src/platform/mdns_noop.c
 * No-op mDNS adapter implementation.
 * Platforms without mDNS support can use this as a safe default. */
#include "muc_opcua/platform.h"

mu_mdns_adapter_t mu_mdns_adapter_noop = {NULL, NULL, NULL};
