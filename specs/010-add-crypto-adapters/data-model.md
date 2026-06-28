# Data Model: Pluggable Cryptographic Adapters

## Pluggable Cryptographic Interface

The `mu_crypto_adapter_t` struct is defined as:

```c
typedef struct mu_crypto_adapter {
    void *context;
    
    mu_status_t (*rsa_oaep_decrypt)(void *context,
                                    const opcua_byte_t *cipher_text, size_t cipher_len,
                                    const opcua_byte_t *private_key, size_t key_len,
                                    opcua_byte_t *plain_text, size_t *plain_len);
                                    
    mu_status_t (*rsa_sha256_sign)(void *context,
                                   const opcua_byte_t *data, size_t data_len,
                                   const opcua_byte_t *private_key, size_t key_len,
                                   opcua_byte_t *signature, size_t *sig_len);
                                   
    mu_status_t (*rsa_sha256_verify)(void *context,
                                     const opcua_byte_t *data, size_t data_len,
                                     const opcua_byte_t *public_key, size_t key_len,
                                     const opcua_byte_t *signature, size_t sig_len);
                                     
    mu_status_t (*symmetric_encrypt)(void *context,
                                     const opcua_byte_t *plain_text, size_t plain_len,
                                     const opcua_byte_t *key, size_t key_len,
                                     const opcua_byte_t *iv, size_t iv_len,
                                     opcua_byte_t *cipher_text, size_t *cipher_len);
                                     
    mu_status_t (*symmetric_decrypt)(void *context,
                                     const opcua_byte_t *cipher_text, size_t cipher_len,
                                     const opcua_byte_t *key, size_t key_len,
                                     const opcua_byte_t *iv, size_t iv_len,
                                     opcua_byte_t *plain_text, size_t *plain_len);
                                     
    mu_status_t (*symmetric_sign)(void *context,
                                  const opcua_byte_t *data, size_t data_len,
                                  const opcua_byte_t *key, size_t key_len,
                                  opcua_byte_t *signature, size_t *sig_len);
                                  
    mu_status_t (*symmetric_verify)(void *context,
                                    const opcua_byte_t *data, size_t data_len,
                                    const opcua_byte_t *key, size_t key_len,
                                    const opcua_byte_t *signature, size_t sig_len);
                                    
    mu_status_t (*generate_key_derivation)(void *context,
                                           const opcua_byte_t *secret, size_t secret_len,
                                           const opcua_byte_t *seed, size_t seed_len,
                                           opcua_byte_t *output, size_t output_len);
                                           
    mu_status_t (*get_own_certificate)(void *context, const opcua_byte_t **cert, size_t *cert_len);
} mu_crypto_adapter_t;
```

## Memory Layout
* **No dynamic allocations**: Both Mbed TLS and wolfSSL wrapper files MUST allocate their required internal contexts (like RSA contexts or cipher contexts) on the function stack.
* **No global state**: Any required key/certificate buffers are held in the adapter's custom `context` structure provided at initialization.
