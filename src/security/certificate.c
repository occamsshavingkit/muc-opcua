/* src/security/certificate.c */
#include "certificate.h"

opcua_statuscode_t mu_certificate_thumbprint(const mu_crypto_adapter_t *crypto, const opcua_byte_t *certificate,
                                             size_t length, opcua_byte_t *thumbprint) {
    if (!crypto || !crypto->get_certificate_thumbprint || !certificate || !thumbprint || length == 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return crypto->get_certificate_thumbprint(crypto->context, certificate, length, thumbprint);
}

opcua_statuscode_t mu_certificate_validate(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                           const opcua_byte_t *certificate, size_t length) {
    /* SecurityPolicy None carries no certificate. */
    if (policy == MU_SECURITY_POLICY_NONE_ID) {
        return MU_STATUS_GOOD;
    }
    if (policy != MU_SECURITY_POLICY_BASIC256SHA256_ID && policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID) {
        return MU_STATUS_BAD_SECURITYPOLICYREJECTED;
    }
    if (!crypto || !crypto->get_certificate_key_bits) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    /* A secure policy requires a certificate. */
    if (certificate == NULL || length == 0) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }

    /* Parseable with an RSA key inside the profile's key-size bounds. */
    size_t bits = 0;
    opcua_statuscode_t s = crypto->get_certificate_key_bits(crypto->context, certificate, length, &bits);
    if (s != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    if (bits < MU_B256S256_MIN_ASYMMETRIC_KEY_BITS || bits > MU_B256S256_MAX_ASYMMETRIC_KEY_BITS) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return MU_STATUS_GOOD;
}
