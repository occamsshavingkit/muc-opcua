# Data Model: Deferred Audit Findings

**Feature**: 031-deferred-audit-fixes
**Date**: 2026-07-04

## Entities Affected

No new entities. All fixes modify existing entities or their behavior.

### Monitored Item (mu_monitored_item_t) — subscription.c

**Changes**:
- T4: No struct change. A local bitmap (uint32_t[]) is built during
  `mu_subscriptions_tick` to track reportable indices. The bitmap lives
  on the stack within the function scope. Size: (MU_MAX_MONITORED_ITEMS
  + 31) / 32 × sizeof(uint32_t) ≈ 32 bytes for 256 items.
- T27: The `monitored_item_change_reportable` function now falls through
  to value equality comparison when `deadband_type == MU_DEADBAND_TYPE_NONE`,
  instead of early-returning true. See [research.md §T27](../research.md) for
  rationale. `deadband_value` field behavior unchanged.

**Fields referenced**: `last_reported_numeric`, `deadband_value`,
`deadband_type`, `last_reported` (variant).

### Subscription (mu_subscription_t) — subscription.c

**Changes**:
- T6: `advance_sample_timer` replaces 64-bit division with repeated
  subtraction in catch-up path. No struct change.
- T31: `advance_publish_timer` adds a local loop counter with max bound
  (~100). No struct change.

**Fields referenced**: `publishing_interval`, `sampling_interval`.

### Service Dispatch Table (g_supported_services[]) — service_dispatch.c

**Changes**:
- T23: Array remains const. Dispatch lookup changes from linear scan to
  binary search. May add a small sorted index table (< 100 bytes) if
  binary search over the existing array is insufficient. No struct change.

**Fields referenced**: `service.request_id` (NodeId numeric identifier).

### ServerNonce Buffers — service_dispatch.c

**Changes**:
- T13: Local `nonce_buf[32]` in `handle_create_session` and
  `handle_activate_session` is now zeroized with `mu_secure_zero` after
  `memcpy` into session slot. No struct change.

### Certificate User Token — service_dispatch.c

**Changes**:
- T12: Certificate token decode and verify paths consolidated under
  single `#ifdef MUC_OPCUA_SECURITY`. No struct change.

### Session — service_dispatch.c

**Changes**:
- T8: Session creation (`mu_session_create_with_identifiers`) now occurs
  after `write_response_prefix`. Cleanup path added for encode failure.
  No struct change.

### Profile URIs — service_dispatch.c

**Changes**:
- T25: GetEndpoints parses profile URIs once into local cache array
  (pointers or indices into decoded buffer). No struct change.

### Base Nodes Table — base_nodes.c

**Changes**:
- T28: 35+ individual `#if`/`#endif` guards consolidated into single
  `#ifdef` block. No struct change; purely preprocessor change.

## Validation Rules

- T4: Bitmap size MUST be `(MU_MAX_MONITORED_ITEMS + 31) / 32 * sizeof(uint32_t)`.
- T5: `#include <math.h>` MUST NOT appear in subscription.c after fix.
- T6: 64-bit division MUST be absent from `advance_sample_timer` fast path.
- T8: `write_response_prefix` MUST precede `mu_session_create_with_identifiers`.
- T12: Certificate token code MUST compile to empty when `MUC_OPCUA_SECURITY` off.
- T13: `mu_secure_zero` MUST be called after memcpy, before function return.
- T27: Deadband NONE MUST NOT early-return true unconditionally.
- T31: `advance_publish_timer` loop MUST have an iteration bound.
- T28: Single `#ifdef` wrapping type-system node definitions.
- MISRA 15.6: No single-statement if/for/while bodies without braces.
- MISRA 10.4: No `size_t` compared to `0u` literal.
