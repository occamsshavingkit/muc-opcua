# Feature Specification: Audit Follow-Up — Remaining Security Findings

**Feature Branch**: `030-audit-followup`  
**Created**: 2026-07-04  
**Status**: Draft  
**Input**: User description: "fix remaining audit findings: nonce fail-closed, password zeroize, channel entropy, self-cert validation"
**Parent Feature**: 029-fix-audit-findings (16 findings remediated, 26 carried forward)
**Source**: `docs/review/five-lens-audit-2026-07-04.md`

## User Scenarios & Testing

### User Story 1 — ActivateSession nonce fail-closed (Priority: P1)

A deployer of muc-opcua on a real network must not have the ActivateSession replay
protection silently degraded when the platform entropy source is unavailable. The
ActivateSession path must fail closed on entropy failure, consistent with
CreateSession and OpenSecureChannel behavior.

**Why this priority**: T1 is a security finding where ActivateSession was the only
auth path that silently fell back to a zero-nonce on entropy failure. Fix blocked
in 029 by test `fake_entropy` parameter-order mismatch with the adapter callback.

**Independent Test**: Drive ActivateSession through mock crypto adapter whose
entropy hook returns failure; assert session is rejected with
`MU_STATUS_BAD_SECURITYCHECKSFAILED`. Fix `test_dispatch_services`
`fake_entropy` to match adapter callback signature.

**Acceptance Scenarios**:
1. `fill_server_nonce()` returns StatusCode; `handle_activate_session` rejects on failure.
2. Test `fake_entropy` callback signature matches `mu_entropy_adapter_t.generate_random`.
3. Existing ActivateSession tests continue to pass.

---

### User Story 2 — Password decrypt buffer zeroization (Priority: P1)

Decrypted user passwords in `decrypt_buf[256]` must be zeroized at all exit points
in `handle_activate_session`. This was blocked in 029 because `mu_secure_zero` is
declared in `key_derivation.h` which is only included under `#ifdef MUC_OPCUA_SECURITY`,
but the password decrypt path is under `#ifdef MUC_OPCUA_USER_AUTH` without a
SECURITY guard.

**Why this priority**: T7 is the only remaining zeroization gap in the security
layer. Codacy flagged it as HIGH (sensitive data in a buffer). All other
key/nonce buffers are already zeroized.

**Independent Test**: Valgrind or canary-pattern test confirms no plaintext password
bytes survive past function return on all exit paths (goto + normal exit).

**Acceptance Scenarios**:
1. `mu_secure_zero(decrypt_buf, ...)` called at each `goto activate_done` site within `#ifdef MUC_OPCUA_SECURITY`.
2. `mu_secure_zero(decrypt_buf, ...)` called before closing `}` of the decrypt block.
3. Zeroization does not execute before `user_auth_handler` receives the password.
4. ARM nano/micro profiles compile clean (no implicit declaration errors).

---

### User Story 3 — Channel entropy & secure channel hardening (Priority: P2)

The SecureChannel ID must not be hardcoded to 1. Each channel open must produce a
unique channel ID. The static counter approach from 029 broke integration tests
that assumed channel_id=1. The fix must either update tests or use a different
approach.

**Why this priority**: T17 is flagged by Codacy as a security concern (predictable
channel ID). With `MU_MAX_CONNECTIONS=1` it's low-risk but should be addressed.

**Independent Test**: Two sequential channel opens produce different channel_id
values. All integration tests pass.

**Acceptance Scenarios**:
1. Channel ID uses an incrementing counter instead of hardcoded 1.
2. Integration tests that assumed channel_id=1 are updated or tolerant.
3. New integration test verifies channel IDs are unique.

---

### User Story 4 — Server self-cert validation fail-closed (Priority: P2)

When `get_own_certificate` fails at server init, the error must be propagated
instead of silently skipping cert validation. Currently the check only runs if
`get_own_certificate` succeeds; if it fails, the server proceeds with no cert
validation.

**Why this priority**: T44 is flagged by Codacy as HIGH (fail-open cert check).
The fail-closed approach in 029 broke integration tests because they don't
configure `get_own_certificate`. The fix must be compatible with test init order.

**Independent Test**: Server init with a crypto adapter whose `get_own_certificate`
returns non-GOOD is rejected with the adapter's error status.

**Acceptance Scenarios**:
1. `get_own_certificate` failure is propagated in `mu_server_config_validate`.
2. Tests that use SecurityPolicy None are unaffected (no cert validation needed).
3. Tests that DO configure a crypto adapter have their `get_own_certificate`
   properly set up before `mu_server_init`.

---

### Edge Cases

- What happens when `MU_SECURE_SCRATCH_SIZE` is at minimum and the zeroization path
  allocates additional stack? The existing `_Static_assert` must still pass.
- How does the channel ID counter behave after 2^32 renewals? Wrap from UINT32_MAX to 1.
- What if `get_own_certificate` succeeds but returns a cert with `own_cert_len == 0`?
  Treat as invalid (needs a valid DER certificate).

## Requirements

### Functional Requirements

- **FR-001**: `fill_server_nonce()` MUST return `opcua_statuscode_t` instead of `void`.
- **FR-002**: `handle_activate_session` MUST fail with `MU_STATUS_BAD_SECURITYCHECKSFAILED` when `fill_server_nonce` returns non-GOOD.
- **FR-003**: `decrypt_buf[256]` in `handle_activate_session` MUST be zeroized via `mu_secure_zero()` at all exit points within the `#ifdef MUC_OPCUA_SECURITY` block.
- **FR-004**: Zeroization MUST occur AFTER `decrypted_password` has been consumed, not before `user_auth_handler` is called.
- **FR-005**: `mu_secure_channel_open` MUST generate a unique channel ID using an incrementing counter instead of hardcoding `1`.
- **FR-006**: `mu_server_config_validate` MUST propagate `get_own_certificate` failure when `config->crypto_adapter != NULL` and the function pointer is set.
- **FR-007**: `test_dispatch_services` `fake_entropy` callback MUST match `mu_entropy_adapter_t.generate_random` signature `(void *context, opcua_byte_t *buffer, size_t length)`.
- **FR-008**: Integration tests that check channel behavior MUST be tolerant of non-1 channel IDs or explicitly set expected values.
- **FR-009**: All four ARM profiles + ASAN/UBSan MUST stay green.
- **FR-010**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap.

### OPC UA Normative Scope

- **OPC-001**: ActivateSession nonce/entropy handling cites OPC-10000-4 §5.7.3, §7.38.2.
- **OPC-002**: Password token security cites OPC-10000-4 §5.7.3.
- **OPC-003**: SecureChannel ID uniqueness cites OPC-10000-6 §6.7.4.
- **OPC-004**: Certificate validation cites OPC-10000-4 §5.5.

### Scope Boundaries

- **In Scope**: T1, T7, T17, T44 from `docs/review/five-lens-audit-2026-07-04.md`.
- **Out of Scope**: Performance optimizations (T4, T5, T6), Tier 3 items from 029.
  Codacy findings that are pre-existing MISRA patterns or false positives.

### Key Entities

- **fill_server_nonce**: Function that generates the ServerNonce for session
  activation responses. Changed from `void` to `opcua_statuscode_t` return.
- **decrypt_buf**: Stack buffer in `handle_activate_session` holding decrypted
  password bytes. Must be zeroized at all exit paths.
- **SecureChannel ID**: Unique identifier for each channel. Changed from hardcoded 1
  to incrementing counter.
- **Server self-cert**: The server's own certificate, validated at init time.
  `get_own_certificate` failure now propagated.

## Success Criteria

- **SC-001**: ActivateSession with failed entropy returns `BAD_SECURITYCHECKSFAILED`.
- **SC-002**: `decrypt_buf` is zeroized on all exit paths (valgrind clean).
- **SC-003**: Consecutive channel opens produce unique channel IDs.
- **SC-004**: Server init fails when `get_own_certificate` returns non-GOOD.
- **SC-005**: All four ARM profiles + ASAN/UBSan green; no regressions from 029.
- **SC-006**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap.

## Assumptions

- The integration test `fake_entropy` parameter-order fix is backward-compatible
  with the adapter callback signature.
- `mu_secure_zero` can be made available in the USER_AUTH path by restructuring
  the `#ifdef` guards in `service_dispatch.c`.
- The channel ID counter is safe under `MU_MAX_CONNECTIONS=1`. A process-level
  counter is acceptable for the single-connection model.
- `get_own_certificate` is configured before `mu_server_init` in all integration
  tests that use the host crypto adapter.
