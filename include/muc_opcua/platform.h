/* include/muc_opcua/platform.h */
#ifndef MUC_OPCUA_PLATFORM_H
#define MUC_OPCUA_PLATFORM_H

#include "muc_opcua/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TCP Transport Adapter Interface
 * Provides non-blocking networking primitives.
 */
typedef struct mu_tcp_adapter {
    void *context;

    /* Initialize the listening endpoint */
    opcua_statuscode_t (*listen)(void *context, const char *endpoint_url);

    /* Accept a new connection */
    opcua_statuscode_t (*accept)(void *context, void **connection_handle);

    /* Read from connection (non-blocking) */
    opcua_statuscode_t (*read)(void *context, void *connection_handle, opcua_byte_t *buffer, size_t buffer_size,
                               size_t *bytes_read);

    /* Write to connection (non-blocking) */
    opcua_statuscode_t (*write)(void *context, void *connection_handle, const opcua_byte_t *buffer, size_t buffer_size,
                                size_t *bytes_written);

    /* Close a connection */
    void (*close_connection)(void *context, void *connection_handle);

    /* Shut down the adapter */
    void (*shutdown)(void *context);
} mu_tcp_adapter_t;

/*
 * UDP Transport Adapter Interface (Optional, for PubSub)
 */
typedef struct mu_udp_adapter {
    void *context;

    /* Initialize the UDP socket for broadcasting */
    opcua_statuscode_t (*init)(void *context, uint16_t port);

    /* Send a UDP packet to a destination (or broadcast) */
    opcua_statuscode_t (*send)(void *context, const opcua_byte_t *buffer, size_t buffer_size, const char *address,
                               uint16_t port);

    /* Shut down the adapter */
    void (*shutdown)(void *context);
} mu_udp_adapter_t;

/*
 * Time Adapter Interface
 * Provides UTC time and monotonic ticks.
 */
typedef struct mu_time_adapter {
    void *context;

    /* Get current UTC time (100-ns intervals since 1601-01-01) */
    opcua_datetime_t (*get_time)(void *context);

    /* Get monotonic millisecond tick count for timeouts */
    opcua_uint64_t (*get_tick_ms)(void *context);
} mu_time_adapter_t;

/*
 * Entropy Adapter Interface
 * Provides cryptographically secure random bytes (required for session nonces).
 */
typedef struct mu_entropy_adapter {
    void *context;

    /* Generate secure random bytes */
    opcua_statuscode_t (*generate_random)(void *context, opcua_byte_t *buffer, size_t length);
} mu_entropy_adapter_t;

/*
 * Persistence Adapter Interface (Optional)
 * For storing/loading certificates or keys.
 */
typedef struct mu_persistence_adapter {
    void *context;

    /* Read a file/object into buffer */
    opcua_statuscode_t (*read)(void *context, const char *key, opcua_byte_t *buffer, size_t buffer_size,
                               size_t *bytes_read);

    /* Write buffer to file/object */
    opcua_statuscode_t (*write)(void *context, const char *key, const opcua_byte_t *buffer, size_t length);
} mu_persistence_adapter_t;

/*
 * Crypto Adapter Interface (Optional)
 * Primitives required by SecurityPolicy Basic256Sha256 (OPC 10000-7). NULL on the
 * server config when only SecurityPolicy None is used. All buffers are caller-owned;
 * the server's own certificate and private key live in the adapter's context.
 */
#define MU_SHA256_LENGTH 32     /* SHA-256 digest / HMAC-SHA256 size */
#define MU_AES_BLOCK_SIZE 16    /* AES-CBC block / IV size */
#define MU_THUMBPRINT_LENGTH 20 /* certificate thumbprint = SHA-1 of DER */

typedef struct mu_crypto_adapter {
    void *context;

    /* SHA-256 of `data` -> `digest` (MU_SHA256_LENGTH bytes). */
    opcua_statuscode_t (*sha256)(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *digest);

    /* HMAC-SHA-256 of `data` under `key` -> `mac` (MU_SHA256_LENGTH bytes). */
    opcua_statuscode_t (*hmac_sha256)(void *context, const opcua_byte_t *key, size_t key_length,
                                      const opcua_byte_t *data, size_t data_length, opcua_byte_t *mac);

    /* AES-256-CBC (no padding). `length` is a multiple of MU_AES_BLOCK_SIZE,
       `key` is 32 bytes, `iv` is MU_AES_BLOCK_SIZE bytes. */
    opcua_statuscode_t (*aes256_cbc_encrypt)(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    opcua_statuscode_t (*aes256_cbc_decrypt)(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output);

    /* AES-128-CBC (no padding). `length` is a multiple of MU_AES_BLOCK_SIZE,
       `key` is 16 bytes, `iv` is MU_AES_BLOCK_SIZE bytes. */
    opcua_statuscode_t (*aes128_cbc_encrypt)(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    opcua_statuscode_t (*aes128_cbc_decrypt)(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output);

    /* Optional per-channel AES context. `ctx_storage` is MU_CIPHER_CTX_SIZE
       bytes; all callbacks MAY be NULL, in which case the codec falls back to
       aes256_cbc_encrypt/decrypt above. */
    opcua_statuscode_t (*cipher_ctx_init)(void *context, const opcua_byte_t *key /*32B*/, opcua_byte_t *ctx_storage);
    opcua_statuscode_t (*aes256_cbc_encrypt_ctx)(void *context, opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
                                                 const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    opcua_statuscode_t (*aes256_cbc_decrypt_ctx)(void *context, opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
                                                 const opcua_byte_t *input, size_t length, opcua_byte_t *output);
    void (*cipher_ctx_free)(void *context, opcua_byte_t *ctx_storage);

    /* RSA PKCS#1 v1.5 with SHA-256, signing with the server private key (context). */
    opcua_statuscode_t (*rsa_sha256_sign)(void *context, const opcua_byte_t *data, size_t length,
                                          opcua_byte_t *signature, size_t *signature_length);
    /* Verify a signature using a peer certificate (DER). */
    opcua_statuscode_t (*rsa_sha256_verify)(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                            const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                            size_t signature_length);

    /* RSA-OAEP (MGF1-SHA1): decrypt with the server private key, encrypt with a peer cert. */
    opcua_statuscode_t (*rsa_oaep_decrypt)(void *context, const opcua_byte_t *input, size_t length,
                                           opcua_byte_t *output, size_t *output_length);
    opcua_statuscode_t (*rsa_oaep_encrypt)(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                           const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                           size_t *output_length);

    /* RSA-PSS (RSASSA-PSS with SHA-256, salt length = 32B) (User Story 2) */
    opcua_statuscode_t (*rsa_pss_sha256_sign)(void *context, const opcua_byte_t *data, size_t length,
                                              opcua_byte_t *signature, size_t *signature_length);
    opcua_statuscode_t (*rsa_pss_sha256_verify)(void *context, const opcua_byte_t *certificate,
                                                size_t certificate_length, const opcua_byte_t *data, size_t data_length,
                                                const opcua_byte_t *signature, size_t signature_length);

    /* RSA-OAEP (MGF1-SHA256) (User Story 2) */
    opcua_statuscode_t (*rsa_oaep_sha256_decrypt)(void *context, const opcua_byte_t *input, size_t length,
                                                  opcua_byte_t *output, size_t *output_length);
    opcua_statuscode_t (*rsa_oaep_sha256_encrypt)(void *context, const opcua_byte_t *certificate,
                                                  size_t certificate_length, const opcua_byte_t *input, size_t length,
                                                  opcua_byte_t *output, size_t *output_length);

    /* The server's own certificate (DER), and the RSA modulus size (bits) of a cert. */
    opcua_statuscode_t (*get_own_certificate)(void *context, const opcua_byte_t **certificate, size_t *length);
    opcua_statuscode_t (*get_certificate_key_bits)(void *context, const opcua_byte_t *certificate,
                                                   size_t certificate_length, size_t *bits);

    /* Certificate thumbprint: SHA-1 of the DER certificate -> `thumbprint`
       (MU_THUMBPRINT_LENGTH bytes). */
    opcua_statuscode_t (*get_certificate_thumbprint)(void *context, const opcua_byte_t *certificate,
                                                     size_t certificate_length, opcua_byte_t *thumbprint);

    /* Certificate validity period (OPC-10000-4 §5.5): MU_STATUS_GOOD when the
       current time is within [notBefore, notAfter] of the DER certificate,
       MU_STATUS_BAD_CERTIFICATETIMEINVALID when expired or not yet valid, or
       MU_STATUS_BAD_CERTIFICATEINVALID when unparseable. OPTIONAL: a NULL pointer
       means the backend cannot check validity, in which case secured policies MUST
       fail closed (see mu_certificate_validate). */
    opcua_statuscode_t (*verify_certificate_validity)(void *context, const opcua_byte_t *certificate,
                                                      size_t certificate_length);
} mu_crypto_adapter_t;

opcua_statuscode_t mu_mbedtls_crypto_adapter_init(mu_crypto_adapter_t *adapter, const opcua_byte_t *cert_der,
                                                  size_t cert_len, const opcua_byte_t *key_der, size_t key_len);
void mu_mbedtls_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter);

opcua_statuscode_t mu_wolfssl_crypto_adapter_init(mu_crypto_adapter_t *adapter, const opcua_byte_t *cert_der,
                                                  size_t cert_len, const opcua_byte_t *key_der, size_t key_len);
void mu_wolfssl_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_PLATFORM_H */
