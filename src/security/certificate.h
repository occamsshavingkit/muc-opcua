/* src/security/certificate.h
 * Certificate thumbprint and peer-certificate validation for the secure-channel
 * handshake (OPC 10000-6 §6.7.4, OPC 10000-4 §7.2). */
#ifndef MUC_OPCUA_CERTIFICATE_H
#define MUC_OPCUA_CERTIFICATE_H

#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include "security_policy.h"
#include <stddef.h>

/* Compute the certificate thumbprint (SHA-1 of the DER), MU_THUMBPRINT_LENGTH
   bytes, via the crypto adapter. */
opcua_statuscode_t mu_certificate_thumbprint(const mu_crypto_adapter_t *crypto, const opcua_byte_t *certificate,
                                             size_t length, opcua_byte_t *thumbprint);

/* Validate a peer certificate for the given SecurityPolicy. For None, no
   certificate is required and the result is Good. For Basic256Sha256 the
   certificate must be present, parseable, and carry an RSA key within the
   profile's key-size bounds. Returns Bad_CertificateInvalid on structural
   failure or Bad_SecurityChecksFailed on key-size violation. */
opcua_statuscode_t mu_certificate_validate(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                           const opcua_byte_t *certificate, size_t length);

/* Feature 026: policy-keyed asymmetric signing/verification. Dispatches to the
   RSA-PKCS#1.5-SHA256 or RSA-PSS-SHA256 adapter primitive per the SecurityPolicy
   (OPC-10000-7), so callers never hardcode the scheme. Fails closed
   (Bad_SecurityChecksFailed) if the crypto adapter or the required primitive for
   the policy is absent — never a silent PKCS#1.5 fallback for a PSS policy. */
opcua_statuscode_t mu_asym_signature_sign(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                          const opcua_byte_t *data, size_t data_length, opcua_byte_t *signature,
                                          size_t *signature_length);

opcua_statuscode_t mu_asym_signature_verify(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                            const opcua_byte_t *certificate, size_t certificate_length,
                                            const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                            size_t signature_length);

#endif /* MUC_OPCUA_CERTIFICATE_H */
