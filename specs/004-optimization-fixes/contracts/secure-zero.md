# Contract — Secret Zeroization (`mu_secure_zero`)

**Finding**: T5 · **FR**: FR-007/008 · **Decision**: D6 · **Files**: `src/security/*`, `src/services/secure_channel.c`

## Intent

Ensure no derived keys, key-schedule/KDF intermediates, decrypted plaintext, MACs,
signatures, or nonces remain readable in RAM after use on a no-MMU device.

## Primitive

```c
/* Overwrite n bytes at v with zero in a way the optimizer must not remove. */
void mu_secure_zero(void *v, size_t n);  /* volatile byte-store loop */
```

## Contract rules

- **Not elidable**: implemented with `volatile` byte stores (or an equivalent
  optimization barrier). MUST NOT be a plain `memset` the compiler may drop on a
  dead buffer. No reliance on `memset_s`/`explicit_bzero` (not freestanding-guaranteed).
- **Key lifecycle**: `client_keys`/`server_keys` (and any cipher context, E2) MUST be
  zeroized in `mu_secure_channel_init` AND `mu_secure_channel_close`, before the struct
  is reused for the next connection (`server.c` reuse path).
- **Stack intermediates**: `mu_sym_keys_derive`, `mu_p_sha256`/KDF, and the
  asym/sym wrap/unwrap routines MUST `mu_secure_zero` their secret stack buffers on
  every return path (success and error) after last use.
- **No correctness change**: zeroization happens after the value is consumed; outputs
  are unchanged.

## Acceptance

- Test drives OPN → MSG → channel close/reuse and asserts the key storage region reads
  as all-zero after close.
- Code review checklist confirms each crypto helper scrubs its secret temporaries
  before returning.
- No behavioural/wire change (existing secure handshake + reference-client tests pass).
