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
    if (policy != MU_SECURITY_POLICY_BASIC256SHA256_ID && policy != MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID &&
        policy != MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID) {
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

    /* OPC-10000-4 §5.5: a secured policy MUST reject expired / not-yet-valid
       certificates. Fail closed when the backend cannot check validity — a
       missing hook must not silently skip the check. */
    if (crypto->verify_certificate_validity == NULL) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    opcua_statuscode_t vs = crypto->verify_certificate_validity(crypto->context, certificate, length);
    if (vs != MU_STATUS_GOOD) {
        return vs;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_asym_signature_sign(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                          const opcua_byte_t *data, size_t data_length, opcua_byte_t *signature,
                                          size_t *signature_length) {
    if (crypto == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    if (mu_security_policy_uses_pss(policy)) {
        if (crypto->rsa_pss_sha256_sign == NULL) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED; /* fail closed: no PKCS#1.5 fallback */
        }
        return crypto->rsa_pss_sha256_sign(crypto->context, data, data_length, signature, signature_length);
    }
    if (crypto->rsa_sha256_sign == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return crypto->rsa_sha256_sign(crypto->context, data, data_length, signature, signature_length);
}

opcua_statuscode_t mu_asym_signature_verify(const mu_crypto_adapter_t *crypto, mu_security_policy_id_t policy,
                                            const opcua_byte_t *certificate, size_t certificate_length,
                                            const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                            size_t signature_length) {
    if (crypto == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    if (mu_security_policy_uses_pss(policy)) {
        if (crypto->rsa_pss_sha256_verify == NULL) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED; /* fail closed: no PKCS#1.5 fallback */
        }
        return crypto->rsa_pss_sha256_verify(crypto->context, certificate, certificate_length, data, data_length,
                                             signature, signature_length);
    }
    if (crypto->rsa_sha256_verify == NULL) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return crypto->rsa_sha256_verify(crypto->context, certificate, certificate_length, data, data_length, signature,
                                     signature_length);
}

opcua_statuscode_t mu_certificate_validate_application_uri(const mu_crypto_adapter_t *crypto,
                                                           mu_security_policy_id_t policy,
                                                           const opcua_byte_t *certificate, size_t certificate_length,
                                                           const char *application_uri, size_t application_uri_length) {
    /* OPC-10000-4 §5.7.2.1: the check is only meaningful on a secured channel
       with a client certificate and a declared ApplicationUri. Skipping the
       absent cases preserves backward compatibility with SecurityPolicy=None
       and with test clients that omit the ApplicationUri. */
    if (policy == MU_SECURITY_POLICY_NONE_ID || policy == MU_SECURITY_POLICY_INVALID_ID) {
        return MU_STATUS_GOOD;
    }
    if (certificate == NULL || certificate_length == 0) {
        return MU_STATUS_GOOD;
    }
    if (application_uri == NULL || application_uri_length == 0) {
        return MU_STATUS_GOOD;
    }
    if (crypto == NULL || crypto->verify_certificate_application_uri == NULL) {
        return MU_STATUS_GOOD;
    }
    opcua_statuscode_t s = crypto->verify_certificate_application_uri(crypto->context, certificate, certificate_length,
                                                                      application_uri, application_uri_length);
    return (s == MU_STATUS_GOOD) ? MU_STATUS_GOOD : MU_STATUS_BAD_CERTIFICATEURIINVALID;
}
