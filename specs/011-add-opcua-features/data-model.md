# Data Model: Custom Methods, Aes256_Sha256_RsaPss, Server Diagnostics, and Dynamic Node Management

## 1. Custom Method Representation in Address Space

We will extend the `mu_node_t` struct (or the method-specific node class union) to hold the pointer to the user C handler callback:

```c
typedef struct {
    mu_node_base_t base;
    mu_method_callback_t callback;
    void *user_data;
} mu_method_node_t;
```

When registering a method node via address space initialization APIs, the integrator will provide the callback function pointer.

## 2. Dynamic Address Space Node Pool

To prevent dynamic heap allocation on the hot path for dynamic node management:

```c
#define MU_MAX_DYNAMIC_NODES 32

typedef struct {
    mu_node_t nodes[MU_MAX_DYNAMIC_NODES];
    bool in_use[MU_MAX_DYNAMIC_NODES];
} mu_dynamic_node_pool_t;
```

We will integrate this pool into the main server address space struct.

## 3. Asymmetric PSS Cryptographic Context

For `Aes256_Sha256_RsaPss`, the asymmetric signature verification and decryption require updated adapter signatures:

```c
typedef struct {
    // ...
    opcua_statuscode_t (*rsa_pss_sign)(void *context, const opcua_byte_t *data, size_t data_len, opcua_byte_t *sig, size_t *sig_len);
    opcua_statuscode_t (*rsa_pss_verify)(void *context, const opcua_byte_t *data, size_t data_len, const opcua_byte_t *sig, size_t sig_len);
    opcua_statuscode_t (*rsa_oaep_sha256_decrypt)(void *context, const opcua_byte_t *cipher, size_t cipher_len, opcua_byte_t *plain, size_t *plain_len);
    opcua_statuscode_t (*rsa_oaep_sha256_encrypt)(void *context, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *plain, size_t plain_len, opcua_byte_t *cipher, size_t *cipher_len);
} mu_crypto_adapter_t;
```
*(Alternatively, we can extend existing methods to accept parameter flags indicating the padding mode, which preserves signature compatibility).*
