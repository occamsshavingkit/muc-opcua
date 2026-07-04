# Fix Contracts: Audit Follow-Up

## Contract 1: fill_server_nonce status propagation (T1)

**Pre-condition**: entropy adapter is configured and accessible via `server->config.entropy_adapter`.

**Post-condition**: Status from `mu_session_generate_server_nonce` is returned to caller. On non-GOOD, `handle_activate_session` rejects the request.

```c
// Before: static void fill_server_nonce(...)
// After:
static opcua_statuscode_t fill_server_nonce(mu_server_t *server, opcua_byte_t *nonce, size_t len) {
    return mu_session_generate_server_nonce(&server->config.entropy_adapter, nonce, len);
}
```

**Caller**: `handle_activate_session` — generate nonce BEFORE `write_response_prefix`, check return.

**Invariant**: Consistent with `handle_create_session` and `handle_open_secure_channel` which already fail-closed on nonce entropy failure.

## Contract 2: Password buffer zeroization (T7)

**Pre-condition**: `decrypt_buf[256]` contains decrypted password bytes after token processing.

**Post-condition**: Buffer is zeroized at all exit points within `#ifdef MUC_OPCUA_SECURITY`.

**Guard restructuring**: Move `decrypt_buf` declaration and all zeroize calls under `#ifdef MUC_OPCUA_SECURITY`.

**Order**: Zeroize AFTER `user_auth_handler` consumes `decrypted_password`, not before.

## Contract 3: Channel ID uniqueness (T17)

**Pre-condition**: `mu_secure_channel_open` is called.

**Post-condition**: `channel->channel_id` is unique (increments from 1, wraps at UINT32_MAX).

**Test update**: Integration tests read channel_id from response instead of assuming value 1.

## Contract 4: Self-cert validation (T44)

**Pre-condition**: `mu_server_config_validate` is called.

**Post-condition**: If `config->crypto_adapter != NULL` and `get_own_certificate` is set and returns non-GOOD, the error is propagated.

**Skip when**: `config->crypto_adapter == NULL` (SecurityPolicy None builds).
