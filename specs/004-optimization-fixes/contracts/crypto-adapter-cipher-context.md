# Contract — Crypto Adapter Cipher-Context Extension (additive)

**Finding**: T2 · **FR**: FR-012 · **Decision**: D2 · **File**: `include/micro_opcua/platform.h`

## Intent

Allow the AES key schedule to be computed once per secure channel instead of once
per MSG chunk, **without breaking existing adapters**. Purely additive to
`mu_crypto_adapter_t`. The existing `aes256_cbc_encrypt`/`aes256_cbc_decrypt` remain
and are the fallback.

## Added (optional) members on `mu_crypto_adapter_t`

```c
/* Prepare a reusable cipher context for one direction of a channel.
   `ctx_storage` is caller-owned, MU_CIPHER_CTX_SIZE bytes, lives in the
   secure-channel struct. Returns Good on success. May be NULL (=> fallback). */
opcua_statuscode_t (*cipher_ctx_init)(void *context,
    const opcua_byte_t *key /*32B*/, opcua_byte_t *ctx_storage);

/* AES-256-CBC using a prepared context. `iv` still supplied per message
   (CBC chaining IV); `length` multiple of MU_AES_BLOCK_SIZE; in-place allowed. */
opcua_statuscode_t (*aes256_cbc_encrypt_ctx)(void *context,
    opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
    const opcua_byte_t *input, size_t length, opcua_byte_t *output);
opcua_statuscode_t (*aes256_cbc_decrypt_ctx)(void *context,
    opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
    const opcua_byte_t *input, size_t length, opcua_byte_t *output);

/* Optional teardown (scrub backend state). May be NULL. */
void (*cipher_ctx_free)(void *context, opcua_byte_t *ctx_storage);
```

## Contract rules

- **Backward compatible**: if any of `cipher_ctx_init`/`aes256_cbc_*_ctx` is NULL, the
  codec MUST use the existing stateless `aes256_cbc_*`. Existing adapters keep working
  with zero changes.
- **Correctness identical**: ciphertext/plaintext MUST be byte-for-byte identical
  between the ctx and non-ctx paths for the same key/iv/input (SC-003).
- **No heap**: `ctx_storage` is caller-provided fixed-size (`MU_CIPHER_CTX_SIZE`); the
  adapter MUST NOT allocate. A compile-time assert MUST verify the backend's schedule
  fits.
- **Lifecycle**: `cipher_ctx_init` is called once per channel at OPN (after key
  derivation). `cipher_ctx_free` (if provided) + `mu_secure_zero` run at channel
  close/reuse (FR-007).
- **In-place**: the ctx variants MUST support in-place operation (existing MSG decrypt
  is in-place).

## Acceptance

- Symmetric MSG round-trip tests pass with both a ctx-capable and a ctx-less stub
  adapter; outputs identical.
- A counter in the test stub proves the key schedule runs once per channel (ctx path)
  vs once per message (fallback path).
