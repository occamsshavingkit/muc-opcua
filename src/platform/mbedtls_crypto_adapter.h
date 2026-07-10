/* src/platform/mbedtls_crypto_adapter.h
 * Mbed TLS crypto backend behind the mu_crypto_adapter_t interface, for
 * host/RTOS targets that link Mbed TLS. RSA SecurityPolicies are fully served;
 * the ECC SecurityPolicies (spec 059) are supported for nistP256 only —
 * Mbed TLS 2.x has X25519 ECDH but no Ed25519 signatures, so the curve25519
 * policy (which mandates Ed25519) can never complete and every curve25519 ECC
 * op fails closed. The adapter's identities are caller-provided. */
#ifndef MUC_OPCUA_MBEDTLS_CRYPTO_ADAPTER_H
#define MUC_OPCUA_MBEDTLS_CRYPTO_ADAPTER_H

#include "muc_opcua/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Provision the adapter's optional ECC signing identity (spec 059) used by
   ecc_sign and get_own_ecc_certificate. `cert_der`/`key_der` are the DER-encoded
   ECC certificate and matching private key; the adapter borrows `cert_der`
   (caller keeps it alive) and parses a private copy of `key_der`. Only
   MU_ECC_CURVE_NISTP256 is accepted — MU_ECC_CURVE_25519 returns a non-GOOD
   status because Mbed TLS 2.x has no Ed25519. Released in cleanup. */
opcua_statuscode_t mu_mbedtls_crypto_adapter_set_ecc_identity(mu_crypto_adapter_t *adapter, mu_ecc_curve_t curve,
                                                              const opcua_byte_t *cert_der, size_t cert_len,
                                                              const opcua_byte_t *key_der, size_t key_len);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_MBEDTLS_CRYPTO_ADAPTER_H */
