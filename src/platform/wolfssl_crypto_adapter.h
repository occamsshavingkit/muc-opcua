/* src/platform/wolfssl_crypto_adapter.h
 * Public surface for the wolfSSL crypto backend beyond the generic
 * mu_crypto_adapter_t vtable declared in muc_opcua/platform.h. The RSA identity
 * is provisioned through mu_wolfssl_crypto_adapter_init (see platform.h); the ECC
 * SecurityPolicies (spec 059) take a caller-supplied per-curve identity via the
 * setter below. */
#ifndef MUC_OPCUA_WOLFSSL_CRYPTO_ADAPTER_H
#define MUC_OPCUA_WOLFSSL_CRYPTO_ADAPTER_H

#include "muc_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MUC_OPCUA_CU_SECURITY_ECC
/* Provision the server's ECC signing identity for `curve` on an already
   initialized wolfSSL adapter. `key_der` is the DER-encoded private key
   (PKCS#8 or SEC1 for nistP256; PKCS#8 for curve25519/Ed25519) and `cert_der`
   the matching DER certificate. Both buffers are borrowed (not copied): the
   caller must keep them alive for the adapter's lifetime. May be called once per
   curve. Returns MU_STATUS_GOOD on success. */
opcua_statuscode_t mu_wolfssl_crypto_adapter_set_ecc_identity(mu_crypto_adapter_t *adapter, mu_ecc_curve_t curve,
                                                              const opcua_byte_t *cert_der, size_t cert_len,
                                                              const opcua_byte_t *key_der, size_t key_len);
#endif /* MUC_OPCUA_CU_SECURITY_ECC */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_WOLFSSL_CRYPTO_ADAPTER_H */
