# Conformance Traceability: Implement Mbed TLS and wolfSSL platform adapters

## Target Profile & Conformance Groups
* **Profile**: Core 2017 Server Facet
* **Security Policies**: Basic256Sha256 and Aes128_Sha256_RsaOaep

## Mappings
| Feature / Service | Normative Reference | Source Implementation File | Test File |
|-------------------|---------------------|----------------------------|-----------|
| Mbed TLS Adapter  | OPC-10000-7 §6.2    | src/platform/mbedtls_crypto_adapter.c | tests/unit/test_mbedtls_adapter.c |
| wolfSSL Adapter   | OPC-10000-7 §6.2    | src/platform/wolfssl_crypto_adapter.c  | tests/unit/test_wolfssl_adapter.c  |
