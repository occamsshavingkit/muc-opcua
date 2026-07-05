# Data Model: Clear Remaining Backlog

**Feature**: 039-clear-remaining-backlog
**Date**: 2026-07-05

## Summary

No new data model entities are introduced. All changes are fixes to existing
code and do not add new data structures, tables, or types beyond minor
additions noted below.

## Modified Entities

### Read Cache Entry (HP2)

Existing `mu_read_cache_entry` struct — no schema change. Fix is purely
in lookup logic (timestamp comparison correction).

### Server State (HP3, HP6, HP9)

`mu_server` struct gains one optional field:
- `next_session_timeout_ms` (HP9): Tracks earliest session expiry to avoid full slot scans.

### CTZ Utility (CQ6)

New standalone `mu_ctz_u32()` function — no new struct, pure function with
optional 32-entry `static const uint8_t` lookup table for fallback path.

### LE uint32 Writer (CQ7)

Consolidation of three existing identical implementations into one canonical
function. No schema change — call sites redirected.

## Unchanged Entities

All other data structures (session, subscription, monitored item, address
space node, encoding buffers, security tokens) are unchanged in structure.
