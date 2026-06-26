/* src/platform/host_crypto_adapter.h
 * A host crypto backend for SecurityPolicy Basic256Sha256, implemented with OpenSSL.
 * For development/interop on a host; embedded targets would provide an mbedTLS/PSA
 * backend behind the same mu_crypto_adapter_t interface. */
#ifndef MICRO_OPCUA_HOST_CRYPTO_ADAPTER_H
#define MICRO_OPCUA_HOST_CRYPTO_ADAPTER_H

#include "micro_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the adapter, generating a self-signed RSA-2048 application instance
   certificate + private key (held in the adapter context). Returns MU_STATUS_GOOD
   on success. Call mu_host_crypto_adapter_cleanup to release resources. */
opcua_statuscode_t mu_host_crypto_adapter_init(mu_crypto_adapter_t *adapter);

void mu_host_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_HOST_CRYPTO_ADAPTER_H */
