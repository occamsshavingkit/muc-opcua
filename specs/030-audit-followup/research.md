# Research: Audit Follow-Up

**Feature**: 030-audit-followup
**Date**: 2026-07-04

## Decision 1: fill_server_nonce fail-closed (T1)

**Decision**: Change `fill_server_nonce` return type from `void` to `opcua_statuscode_t`. Caller in `handle_activate_session` checks return and rejects on non-GOOD. Fix `test_dispatch_services` `fake_entropy` callback signature to match `mu_entropy_adapter_t.generate_random(void *context, opcua_byte_t *buffer, size_t length)`.

**Rationale**: In 029, this fix broke `test_dispatch_services` because the test's `fake_entropy` callback definition `(void *c, opcua_byte_t *buf, size_t len)` matches the ADAPTER signature `generate_random(void *context, opcua_byte_t *buffer, size_t length)` — the params are actually in the correct order. The 029 failure was from a different issue: the `fill_server_nonce` return was checked after `write_response_prefix` had already been written, but the nonce generation was succeeding (returning GOOD). The REAL issue was that the test's `activate_result` flow had a latent bug exposed by the nonce check.

In 030, the approach is different: move the nonce GENERATION before `write_response_prefix` so the response isn't partially written on failure. This mirrors the CreateSession flow pattern.

**Alternatives considered**:
- Keep zero-fallback: rejected — inconsistent with CreateSession/OPN.
- Generate nonce in a separate helper: adds complexity, same result.

## Decision 2: Password buffer zeroization (T7)

**Decision**: Move the `decrypt_buf` declaration and its `mu_secure_zero` calls under `#ifdef MUC_OPCUA_SECURITY`. The password decryption only occurs when a crypto adapter is present (SECURITY=ON), so zeroization can be guarded the same way. The non-security path (USER_AUTH without SECURITY) uses `decrypted_password = user_token.password` (the raw wire bytes, not decrypt_buf) and doesn't need zeroization.

**Rationale**: The 029 attempt added `mu_secure_zero` calls without the SECURITY guard, causing implicit-declaration errors on ARM nano/micro builds (SECURITY=OFF). The `key_derivation.h` include is inside `#ifdef MUC_OPCUA_SECURITY` at line 24 of service_dispatch.c.

The original code structure already has the decrypt logic inside `#ifdef MUC_OPCUA_USER_AUTH`. Within that, the password DECRYPTION path already checks `config->crypto_adapter != NULL` which is a compile-time feature that requires SECURITY. Adding the `#ifdef MUC_OPCUA_SECURITY` guard around decrypt_buf and zeroize calls properly isolates them.

**Zeroization points per CodeRabbit fix**:
1. At each `goto activate_done` within the SECURITY block (5 sites)
2. After `user_auth_handler` completes (NOT before!) — the password must still be valid when passed to the handler

**Alternatives considered**:
- Use `memset` instead: rejected — memset can be optimized away.
- Move to scratch region: unnecessary — the stack buffer is small and the zeroize is called before function exit.

## Decision 3: Channel ID counter (T17)

**Decision**: Use a `static` function-local counter in `mu_secure_channel_open` that increments on each channel open. The counter wraps from `UINT32_MAX` to 1.

**Rationale**: In 029, this broke integration tests because the counter persists across tests in the same process. In 030, the fix is to ALSO make the counter restartable by adding a `mu_secure_channel_init` call in test setup. Or — simpler — make the tests not depend on channel_id being any specific value.

The simplest approach: change the tests to read the actual channel_id from the OPN response instead of assuming it's 1. The test helpers already read the response and can extract the channel_id.

**Alternatives considered**:
- Use `entropy_adapter` for randomness: requires API change to pass entropy to `mu_secure_channel_open`.
- Keep hardcoded 1: rejected by Codacy as security concern.

## Decision 4: Self-cert validation fail-closed (T44)

**Decision**: In `mu_server_config_validate`, propagate `get_own_certificate` failure as an error. BUT — only do this when `config->crypto_adapter != NULL` AND the function pointer is non-NULL (adapter is intentionally configured). When the adapter is NULL (SecurityPolicy None builds), skip the check.

**Rationale**: In 029, the fail-closed approach broke integration tests because they configure a crypto adapter whose `get_own_certificate` hasn't been fully initialized at config-validate time. The fix: the config-validate now actively checks whether `get_own_certificate` is present AND returns non-GOOD, and only fails in that case.

**Key insight**: The test that failed (`test_view_services`) uses SecurityPolicy None and does NOT configure a crypto adapter. So `config->crypto_adapter == NULL` and the check is entirely skipped. The tests that DO configure crypto (secure handshake tests) already properly initialize `get_own_certificate` before server init.

**Alternatives considered**:
- Lazy validation at first secure channel open: more correct but larger refactor.
- Keep fail-soft with comment: rejected as Codacy HIGH requires action.
