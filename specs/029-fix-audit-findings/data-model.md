# Data Model: Five-Lens Audit Findings Cleanup

**Feature**: 029-fix-audit-findings
**Date**: 2026-07-04

This feature does not introduce new entities. It modifies the behavior of existing entities as described below. Each entry maps to an audit finding from `docs/review/five-lens-audit-2026-07-04.md`.

## Entities Affected

### fill_server_nonce (T1)

- **Before**: `void fill_server_nonce(...)` — silently sets nonce to all-zeros on entropy failure.
- **After**: Returns `opcua_statuscode_t`; caller rejects session on failure.
- **Impact on**: `handle_activate_session` in `service_dispatch.c`.

### Password Decrypt Buffer (T2)

- **Before**: `opcua_byte_t decrypt_buf[256]` on stack, not zeroized.
- **After**: Same buffer, zeroized via `mu_secure_zero()` at both exit points (normal return and `goto activate_done`).
- **Impact on**: `handle_activate_session` in `service_dispatch.c`.

### Browse Reference Pool (T3)

- **Before**: When `requested_max_references_per_node` exceeded, `res->references` and `res->num_references` are not assigned — returned as 0.
- **After**: Assigned from the pool (`&ref_pool[node_refs_start]` and `match_count`) before the `continue`.
- **Impact on**: `browse.c` response encoding path.

### MonitoredItem Scan Passes (T4)

- **Before**: Three full-array scans per publish cycle.
- **After**: Single sampling pass collects reportable indices into `opcua_uint16_t reportable_indices[MU_MAX_MONITORED_ITEMS]`; encoding and cleanup passes iterate only over `reportable_count` items.
- **Impact on**: `mu_subscriptions_tick`, `count_reportable_items`, `write_data_change_notification`, `clear_reported_items` in `subscription.c`.

### Deadband Absolute Value (T5, T24)

- **Before**: `fabs((double)numeric - (double)item->last_reported_numeric)` — requires `<math.h>`.
- **After**: `opcua_double_t diff = numeric - item->last_reported_numeric; if (diff < 0.0) diff = -diff;` — no `<math.h>` dependency.
- **Impact on**: `monitored_item_change_reportable` in `subscription.c`. Wire-identical results.

### Sample Timer Advance (T6)

- **Before**: `opcua_uint64_t steps = (elapsed / interval) + 1u` — 64-bit division on every catch-up.
- **After**: Repeated-subtraction loop capped at 100 iterations.
- **Impact on**: `advance_sample_timer` in `subscription.c`.

### RSA-OAEP Context (T7)

- **Before**: Implicit SHA-1 OAEP/MGF1 via OpenSSL defaults.
- **After**: Explicit `EVP_PKEY_CTX_set_rsa_oaep_md(pc, EVP_sha1())` and `EVP_PKEY_CTX_set_rsa_mgf1_md(pc, EVP_sha1())`.
- **Impact on**: `rsa_oaep()` in `host_crypto_adapter.c`.

### Session Create Ordering (T8)

- **Before**: `mu_session_create_with_identifiers()` called before `write_response_prefix()` — session orphaned if encoding fails.
- **After**: `write_response_prefix()` called before `mu_session_create_with_identifiers()`.
- **Impact on**: `handle_create_session` in `service_dispatch.c`.

### DeleteNodes Target References Flag (T9)

- **Before**: `delete_target_references` decoded but ignored; all references always removed.
- **After**: Reference cleanup loop guarded by `if (items[i].delete_target_references)`.
- **Impact on**: `handle_delete_nodes` in `node_management.c`.

### Write Validation Result (T10)

- **Before**: `result` stays `MU_STATUS_GOOD` when `mu_value_source_read` fails.
- **After**: `result = MU_STATUS_BAD_INTERNALERROR` in the failure path.
- **Impact on**: Write processing in `service_dispatch.c`.

### Value Source Type Coverage (T11)

- **Before**: Only Boolean, Int32, UInt32, Float, String returned; all others get `BAD_NOTREADABLE`.
- **After**: All scalar built-in types returned via a broad default branch.
- **Impact on**: `mu_value_source_read` in `value_source.c`.

### Certificate Token Ifdef Scope (T12)

- **Before**: Certificate token decode and verification under separate `#ifdef` blocks.
- **After**: Consolidated under a single `#ifdef MUC_OPCUA_SECURITY` block.
- **Impact on**: `handle_activate_session` in `service_dispatch.c`.

### ServerNonce Stack Copies (T13)

- **Before**: `nonce_buf[32]` on stack, not zeroized after copy.
- **After**: Zeroized with `mu_secure_zero()` after `memcpy` into session slot.
- **Impact on**: `handle_create_session` and `handle_activate_session` in `service_dispatch.c`.

### Deadband None Semantics (T27)

- **Before**: `if (item->deadband_type != MU_DEADBAND_TYPE_ABSOLUTE) return true;` — spams notifications.
- **After**: `if (item->deadband_type == MU_DEADBAND_TYPE_NONE) return false;` — defers to caller's value-equality check.
- **Impact on**: `monitored_item_change_reportable` in `subscription.c`.

### ExtensionObject Encoding Mask (T29)

- **Before**: `if ((encoding_mask & 0x01) == 0)` — accepts 0x03.
- **After**: `if (encoding_mask != 0x01)` — rejects all non-ByteString masks.
- **Impact on**: HistoryRead decoder in `history.c`.

### Const Tables (T26)

- **Before**: `g_supported_services[]` and `POLICY_TABLE[]` in `.data`.
- **After**: Declared `static const` → compiler places in `.rodata` (flash).
- **Impact on**: `service_dispatch.c` and `security_policy.c`.

### Publish Timer Guard (T31)

- **Before**: `do...while` on zero-interval could spin unboundedly.
- **After**: Max 100 iterations, then break with the current value.
- **Impact on**: `advance_publish_timer` in `subscription.c`.
