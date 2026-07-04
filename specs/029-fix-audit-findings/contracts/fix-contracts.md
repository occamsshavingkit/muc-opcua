# Fix Contracts: Five-Lens Audit Findings Cleanup

**Feature**: 029-fix-audit-findings
**Date**: 2026-07-04
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md`

## Contract 1: fill_server_nonce status propagation (T1)

**Pre-condition**: `mu_session_generate_server_nonce` is called with non-null buffer and length ≥ 32.

**Post-condition**:
- On success (returns GOOD): buffer contains cryptographically random nonce.
- On failure (returns error): function returns the error StatusCode; caller must NOT use the buffer contents and must reject the session.

**Contract**:
```c
// Before: void fill_server_nonce(mu_server_t *server, opcua_byte_t *buf, size_t len);
// After:
opcua_statuscode_t fill_server_nonce(mu_server_t *server, opcua_byte_t *buf, size_t len);
// Returns: MU_STATUS_GOOD on success, error code on entropy failure.
// Caller MUST fail the enclosing operation on non-GOOD return.
```

**Callers affected**: `handle_activate_session` — must check return and reject with `MU_STATUS_BAD_SECURITYCHECKSFAILED`.

**Invariant**: Consistent with `handle_create_session` and `handle_open_secure_channel` which already fail-closed on entropy failure.

## Contract 2: Password buffer zeroization (T2)

**Pre-condition**: `decrypt_buf[256]` in `handle_activate_session` contains decrypted password bytes after token processing.

**Post-condition**: After the buffer's last use (at `goto activate_done` or normal function exit), `decrypt_buf` contains zero bytes via `mu_secure_zero(decrypt_buf, sizeof(decrypt_buf))`.

**Contract**:
- Two zeroization points: before the `goto activate_done` label target AND before the closing `}` of the `#ifdef MUC_OPCUA_SECURITY` block.
- Zeroization must occur AFTER the last read of `decrypt_buf` / `decrypted_password`.
- Must use `mu_secure_zero()` (volatile loop) not `memset()`.

## Contract 3: Browse reference preservation (T3)

**Pre-condition**: `requested_max_references_per_node` > 0 and `match_count` > `requested_max_references_per_node` for a browse node.

**Post-condition**: The BrowseResult for that node returns `match_count` references (up to the limit), with `num_references == min(match_count, requested_max_references_per_node)` and a non-zero continuation point indicator.

**Contract**: At the `too_many_refs` label in `browse.c`:
```c
res->references = &ref_pool[node_refs_start];
res->num_references = match_count;  // was: not set, silently dropped
```

## Contract 4: Subscription single-pass scan (T4)

**Pre-condition**: `mu_subscriptions_tick` is called with active subscriptions and monitored items.

**Post-condition**: Reportable items are identified in a single pass over `MU_MAX_MONITORED_ITEMS`. Encoding and cleanup passes iterate only over the identified subset. Wire output is identical to the triple-scan behavior.

**Contract**:
```c
// Collected during sampling pass:
opcua_uint16_t reportable_indices[MU_MAX_MONITORED_ITEMS];
size_t reportable_count = 0;

// Pass 1 (sampling): populate reportable_indices
// Pass 2 (encoding): for i in 0..reportable_count-1, write item reportable_indices[i]
// Pass 3 (cleanup): for i in 0..reportable_count-1, clear item reportable_indices[i]
```

**Invariant**: `reportable_count` ≤ `MU_MAX_MONITORED_ITEMS`. Wire output byte-identical to pre-fix behavior.

## Contract 5: fabs removal (T5)

**Pre-condition**: `monitored_item_change_reportable` is called with Float/Double-typed variants.

**Post-condition**: The absolute difference comparison produces the same boolean result as `fabs(a - b) >= deadband_value`. No `<math.h>` dependency.

**Contract**:
```c
// Before:
return fabs((double)numeric - (double)item->last_reported_numeric) >= (double)item->deadband_value;

// After:
opcua_double_t diff = numeric - item->last_reported_numeric;
if (diff < 0.0) diff = -diff;
return diff >= item->deadband_value;
```

**Invariant**: IEEE 754 `double` subtraction, negation, and comparison produce identical results to `fabs()`. The `<math.h>` include is removed from the TU.

## Contract 6: Deadband None semantics (T27)

**Pre-condition**: `monitored_item_change_reportable` is called with `deadband_type == MU_DEADBAND_TYPE_NONE`.

**Post-condition**: The function returns `false`, deferring the change decision to the caller (`monitored_item_sample_changed`) which performs a scalar value equality check. No notification is generated for unchanged values.

**Before**:
```c
if (item->deadband_type != MU_DEADBAND_TYPE_ABSOLUTE) {
    return true;  // SPAM: reports on every tick
}
```

**After**:
```c
if (item->deadband_type == MU_DEADBAND_TYPE_NONE) {
    return false;  // Defer to caller's value-equality check
}
if (item->deadband_type != MU_DEADBAND_TYPE_ABSOLUTE) {
    return true;
}
```
