# Integration Guide: Pluggable Cryptographic Adapters

## How to build with Mbed TLS
Enable the option in CMake:
```bash
cmake -DMICRO_OPCUA_HAVE_MBEDTLS=ON -B build
```
This compiles `src/platform/mbedtls_crypto_adapter.c` and links against `mbedcrypto`.

## How to build with wolfSSL
Enable the option in CMake:
```bash
cmake -DMICRO_OPCUA_HAVE_WOLFSSL=ON -B build
```
This compiles `src/platform/wolfssl_crypto_adapter.c` and links against `wolfssl`.

## Using the Adapters in C

Initialize the server config and assign the selected adapter:

```c
#include <micro_opcua/server.h>

// For Mbed TLS:
mu_crypto_adapter_t crypto_adapter;
mu_mbedtls_adapter_init(&crypto_adapter, own_certificate, cert_len, own_private_key, key_len);

mu_server_config_t config = {
    .crypto_adapter = &crypto_adapter,
    // ...
};

mu_server_t server;
mu_server_init(&server, &config);
```
