/* tests/support/fake_platform.h */
#ifndef FAKE_PLATFORM_H
#define FAKE_PLATFORM_H

#include "micro_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the fake platform and return adapters for tests */
void fake_platform_init(
    mu_tcp_adapter_t *tcp_adapter,
    mu_time_adapter_t *time_adapter,
    mu_entropy_adapter_t *entropy_adapter
);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_PLATFORM_H */
