/* platform/pico/mu_pico_adapter.h */
#ifndef MU_PICO_ADAPTER_H
#define MU_PICO_ADAPTER_H

#include "micro_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the Pico adapters */
void mu_pico_adapter_init(
    mu_tcp_adapter_t *tcp_adapter,
    mu_time_adapter_t *time_adapter,
    mu_entropy_adapter_t *entropy_adapter
);

#ifdef __cplusplus
}
#endif

#endif /* MU_PICO_ADAPTER_H */
