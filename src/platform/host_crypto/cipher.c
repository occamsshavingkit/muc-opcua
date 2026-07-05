/* src/platform/host_crypto/cipher.c
 * AES-256-CBC / AES-128-CBC encrypt/decrypt and context management. */
#include "common.h"

static opcua_statuscode_t aes_cbc(const EVP_CIPHER *cipher, const opcua_byte_t *key, const opcua_byte_t *iv,
                                  const opcua_byte_t *in, size_t len, opcua_byte_t *out, int encrypt) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    int outl = 0, finl = 0;
    int ok = encrypt
                 ? (EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                    EVP_EncryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                    EVP_EncryptFinal_ex(ctx, out + outl, &finl) == 1)
                 : (EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                    EVP_DecryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                    EVP_DecryptFinal_ex(ctx, out + outl, &finl) == 1);
    if (ok) {
        rc = MU_STATUS_GOOD;
    }
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

opcua_statuscode_t h_aes_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                 size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_256_cbc(), key, iv, in, len, out, 1);
}
opcua_statuscode_t h_aes_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                 size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_256_cbc(), key, iv, in, len, out, 0);
}
opcua_statuscode_t h_aes128_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                    size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_128_cbc(), key, iv, in, len, out, 1);
}
opcua_statuscode_t h_aes128_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                    size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_128_cbc(), key, iv, in, len, out, 0);
}

static void store_cipher_handle(opcua_byte_t *ctx_storage, struct host_cipher_context *handle) {
    (void)memcpy(ctx_storage, &handle, sizeof(handle));
}

static struct host_cipher_context *load_cipher_handle(const opcua_byte_t *ctx_storage) {
    struct host_cipher_context *handle = NULL;
    (void)memcpy(&handle, ctx_storage, sizeof(handle));
    return handle;
}

static void free_cipher_handle(struct host_cipher_context *handle) {
    if (!handle) {
        return;
    }
    EVP_CIPHER_CTX_free(handle->encrypt);
    EVP_CIPHER_CTX_free(handle->decrypt);
    free(handle);
}

opcua_statuscode_t h_cipher_ctx_init(void *c, const opcua_byte_t *key, opcua_byte_t *ctx_storage) {
    (void)c;
    if (!key || !ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#if MUC_OPCUA_ALLOW_HEAP
    struct host_cipher_context *handle = (struct host_cipher_context *)calloc(1, sizeof(*handle));
    if (!handle) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
#else
    return MU_STATUS_BAD_OUTOFMEMORY;
#endif

    handle->encrypt = EVP_CIPHER_CTX_new();
    handle->decrypt = EVP_CIPHER_CTX_new();
    if (!handle->encrypt || !handle->decrypt) {
        free_cipher_handle(handle);
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    int ok = EVP_EncryptInit_ex(handle->encrypt, EVP_aes_256_cbc(), NULL, key, NULL) == 1 &&
             EVP_CIPHER_CTX_set_padding(handle->encrypt, 0) == 1 &&
             EVP_DecryptInit_ex(handle->decrypt, EVP_aes_256_cbc(), NULL, key, NULL) == 1 &&
             EVP_CIPHER_CTX_set_padding(handle->decrypt, 0) == 1;
    if (!ok) {
        free_cipher_handle(handle);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    store_cipher_handle(ctx_storage, handle);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t aes_cbc_ctx(EVP_CIPHER_CTX *ctx, const opcua_byte_t *iv, const opcua_byte_t *in, size_t len,
                                      opcua_byte_t *out, int encrypt) {
    if (!ctx || !iv || (!in && len) || !out) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    int outl = 0, finl = 0;
    int ok = encrypt ? (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                        EVP_EncryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                        EVP_EncryptFinal_ex(ctx, out + outl, &finl) == 1)
                     : (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                        EVP_DecryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                        EVP_DecryptFinal_ex(ctx, out + outl, &finl) == 1);
    if (ok) {
        rc = MU_STATUS_GOOD;
    }
    return rc;
}

opcua_statuscode_t h_aes_encrypt_ctx(void *c, opcua_byte_t *ctx_storage, const opcua_byte_t *iv, const opcua_byte_t *in,
                                     size_t len, opcua_byte_t *out) {
    (void)c;
    if (!ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    struct host_cipher_context *handle = load_cipher_handle(ctx_storage);
    if (!handle) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return aes_cbc_ctx(handle->encrypt, iv, in, len, out, 1);
}

opcua_statuscode_t h_aes_decrypt_ctx(void *c, opcua_byte_t *ctx_storage, const opcua_byte_t *iv, const opcua_byte_t *in,
                                     size_t len, opcua_byte_t *out) {
    (void)c;
    if (!ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    struct host_cipher_context *handle = load_cipher_handle(ctx_storage);
    if (!handle) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return aes_cbc_ctx(handle->decrypt, iv, in, len, out, 0);
}

void h_cipher_ctx_free(void *c, opcua_byte_t *ctx_storage) {
    (void)c;
    if (!ctx_storage) {
        return;
    }
    struct host_cipher_context *handle = load_cipher_handle(ctx_storage);
    free_cipher_handle(handle);
    handle = NULL;
    store_cipher_handle(ctx_storage, handle);
}
