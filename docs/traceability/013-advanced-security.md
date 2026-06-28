# Traceability: Advanced Security Policies (013-advanced-security)

This document maps the requirements from the Advanced Security Policies feature to the implementation and tests.

| Requirement | Implementation File | Test File |
|-------------|---------------------|-----------|
| Add `Aes128-Sha256-RsaOaep` | `include/micro_opcua/security.h`, `src/security/asym_chunk.c`, `src/platform/host_crypto_adapter.c` | `tests/unit/test_security_rsa_oaep.c`, `tests/unit/test_security_rsa_oaep_errors.c` |
| Add `Aes256-Sha256-RsaPss` | `include/micro_opcua/security.h`, `src/security/asym_chunk.c`, `src/platform/host_crypto_adapter.c` | `tests/unit/test_security_rsa_pss.c`, `tests/unit/test_security_rsa_pss_errors.c` |
| Client Certificate TrustList | `include/micro_opcua/security.h`, `src/security/trustlist.c`, `src/core/service_dispatch.c` | `tests/unit/test_security_trustlist.c` |
