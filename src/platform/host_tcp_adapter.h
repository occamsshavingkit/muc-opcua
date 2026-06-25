/* src/platform/host_tcp_adapter.h */
#ifndef MICRO_OPCUA_HOST_TCP_ADAPTER_H
#define MICRO_OPCUA_HOST_TCP_ADAPTER_H

#include "micro_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the host TCP adapter using POSIX sockets.
 * Returns MU_STATUS_GOOD on success.
 */
opcua_statuscode_t mu_host_tcp_adapter_init(mu_tcp_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_HOST_TCP_ADAPTER_H */
