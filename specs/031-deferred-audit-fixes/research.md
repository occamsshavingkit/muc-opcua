# Research: Deferred Audit Findings

**Feature**: 031-deferred-audit-fixes
**Date**: 2026-07-04

## Decision: T4 — Bitmap-based single-pass publish scan

**Decision**: Collect reportable item indices into a fixed-size bitmap
(uint32_t array sized to (MU_MAX_MONITORED_ITEMS + 31) / 32) during the
existing sampling loop. Then iterate only set bits for encoding and cleanup.

**Rationale**: The existing triple-scan pattern (count, write, clear) visits
every MU_MAX_MONITORED_ITEMS entry three times per publish cycle. A bitmap
adds ~32 bytes of RAM (well within existing headroom) and eliminates two full
scans. This is the simplest non-intrusive approach — it requires no changes to
the existing sampling loop beyond recording indices, and the bitmap iteration
pattern is well-understood.

**Alternatives considered**:
- Linked list of reportable items: simpler for small counts, but requires
  storage per-item (pointer overhead), complicates the item struct, and adds
  complexity for cleanup.
- Separate "pending" array: doubles storage for monitored item state. Bitmap
  is more compact.
- Just fixing the loop by iterating all items once but tracking state:
  requires restructuring the encoding pass significantly. Bitmap is minimal
  change.

**References**: OPC-10000-4 §5.13.1.2 (Publish service — notification
generation). Existing `mu_subscriptions_tick` at subscription.c:1612-1697.

---

## Decision: T5/T24 — Inline conditional negation replacing `fabs(double)`

**Decision**: Replace `fabs((double)numeric - (double)item->last_reported_numeric)`
with `double diff = numeric - last_reported; if (diff < 0) diff = -diff;`
and remove `#include <math.h>`.

**Rationale**: On Cortex-M0+, `fabs(double)` pulls in ~12 KB of software
double-precision emulation from `<math.h>`. The inline conditional produces
identical results for all normal IEEE 754 values (the only values in this
context — deadband comparison on sensor readings). NaN and subnormal handling
is not relevant: OPC UA DataValues are IEEE 754 normal doubles or integers
cast to double. The existing compiler already knows how to negate doubles
without <math.h>.

**Alternatives considered**:
- `fabsf(float)`: still pulls in math.h, smaller but still unnecessary.
- Integer comparison: if deadband_value is typically integer, could convert
  to integer math. But OPC UA DataChangeFilter deadband is defined as
  Double, so staying in double preserves spec fidelity.
- Custom `mu_fabs()`: unnecessary wrapper for a one-line inline.

**References**: OPC-10000-4 §7.22.2 (DataChangeFilter — DeadbandValue is
Double). Existing code at subscription.c:131.

---

## Decision: T6 — 64-bit division avoidance in sample timer

**Decision**: Keep existing `if (elapsed < interval)` fast-path guard (handles
99%+ of calls). For the catch-up path, replace `elapsed / interval` with
repeated subtraction capped at a small upper bound (e.g., 10 steps), since
publish intervals >= 10ms and sample intervals are typically the same order
of magnitude.

**Rationale**: `__aeabi_uldivmod` (64-bit / 64-bit) costs ~300 cycles on
M0+. The existing fast-path guard already avoids this in the common case.
The catch-up path is hit only when the server is late (system load), and
catching up more than a few steps indicates a configuration problem. Bounding
catch-up to ~10 steps is safe because the real deadline isn't "catch up all
missed samples" — it's "get back on schedule for the current interval."

**Alternatives considered**:
- 32-bit division: safer but requires runtime checks that both operands fit in
  32 bits. More code than repeated subtraction.
- Keep 64-bit division: simplest, but defeats the purpose of the optimization.
  For a single-connection embedded server, a few hundred cycles per publish
  cycle matters.

**References**: Existing `advance_sample_timer` at subscription.c:350-410.
OPC-10000-4 §5.13.1.2.

---

## Decision: T8 — Reorder session creation vs. response prefix

**Decision**: Move `write_response_prefix` call BEFORE `mu_session_create_with_identifiers`.
If any subsequent encode step fails, the session is already committed but the
response is incomplete — use an error cleanup path that calls `mu_session_close`
on the newly created session_id.

**Rationale**: The simplest correct fix. The alternative (create session after
all encode steps succeed) requires restructuring the entire handler to defer
the session ID assignment — complex and error-prone. Adding a cleanup call on
failure is a 5-line change.

**Alternatives considered**:
- Two-phase commit: defer session ID generation. Requires thread-local state
  or struct restructure. Overengineered for a single-threaded server.
- Ignore: a dangling session slot consuming MU_MAX_SESSIONS (2 in nano) is
  actually a resource leak. Must fix.

**References**: OPC-10000-4 §5.7.3 (CreateSession). Existing code at
service_dispatch.c:780-811.

---

## Decision: T12 — Consolidate cert token ifdef blocks

**Decision**: Move all certificate token decode and verify code under a single
`#ifdef MUC_OPCUA_SECURITY` block. The cert token handler is fully excluded
when security is disabled.

**Rationale**: The current split — decode under one `#ifdef`, verify under
another — creates a hypothetical build configuration where tokens are decoded
but never verified. While no actual profile uses this config, the code should
not permit it. Consolidation is a no-op for all existing builds.

**Alternatives considered**:
- `#if` / `#ifndef` guard pair: adds compile-time assertion. Simpler than
  restructure, but less clear to readers.
- Leave as-is: "it works in practice." Violates defense-in-depth.

**References**: OPC-10000-4 §5.7.3 (ActivateSession — Certificate identity
token type 327). Existing code at service_dispatch.c:1029-1223.

---

## Decision: T13 — Zeroize nonce stack copies

**Decision**: Add `mu_secure_zero(nonce_buf, sizeof(nonce_buf))` after the
`memcpy` into the session slot in both `handle_create_session` (~line 772)
and `handle_activate_session` (~line 1240).

**Rationale**: While the nonce is sent in cleartext in the response, it is
also used as KDF input for derived session keys. Stack copies that persist
after the function returns could leak key derivation material. `mu_secure_zero`
is a volatile memset that compiler optimizations cannot remove. This is the
last nonce zeroization gap identified in the audit.

**Alternatives considered**:
- Read nonce directly into session slot: eliminates the copy. Requires
  refactoring `mu_session_generate_server_nonce` to take a target buffer.
  More invasive but zero-copy. Accepted: the zeroize approach is simpler
  and non-invasive.
- Do nothing: nonce is public in the response. But defense-in-depth is the
  project's security stance.

**References**: Existing code at service_dispatch.c:772, 1240. Constitution
V (Security Honesty). T2 already fixed password zeroize; T13 completes the
pattern.

---

## Decision: T23 — Direct-index dispatch table

**Decision**: Build a const lookup array indexed by (request_id - base_offset)
where request_id values from g_supported_services[] are clustered in a narrow
numeric range (400-800 for OPC UA services). If request_ids are contiguous,
use a simple array; if sparse, use a small index table or binary search.

**Rationale**: The existing linear scan visits ~15 entries on average (30
entries, ~halfway hit). OPC UA NodeIds for service methods are clustered —
Discovery services at 400-430, Session services at 440-490, etc. A direct-
index array using the full range (e.g., array[900]) wastes ~900 * 8 bytes =
7.2 KB of flash. Better approach: build a small sorted index table of
(request_id, dispatch_index) pairs, binary-search in O(log N) ≈ 5 comparisons.

**Alternatives considered**:
- Full sparse array: fast but wastes flash. Rejected for size discipline.
- Binary search existing array: O(log N) without extra table. Simplest.
  Accepted for initial implementation; can refine to direct-index later.
- Range-based buckets: e.g., Discovery bucket, Session bucket. Good middle
  ground. Accepted if binary search doesn't satisfy.

**References**: Existing `g_supported_services[]` at service_dispatch.c:235-295.
OPC-10000-4 §7.1 (Service Sets).

---

## Decision: T25 — Cache profile URIs

**Decision**: Parse profile URIs from the request body into a small fixed-size
array (e.g., char* pointers into the decoded buffer, or indices). Compare
each endpoint's profile URI against the cached array instead of re-parsing
the wire UInt32Array every time.

**Rationale**: GetEndpoints iterates endpoints in a loop, re-parsing the same
profile URI array for each endpoint. For a server with 3 endpoints and 4
profiles, this is 12 parses instead of 3. The fix simply captures the parsed
array on first pass and reuses it.

**Alternatives considered**:
- Pre-compute at server init: all endpoints static, could cache. But not
  necessary — the runtime caching is trivial.
- Skip: low-cost on host, but unnecessary overhead on embedded.

**References**: OPC-10000-4 §5.4.3.2 (GetEndpoints). Existing code at
service_dispatch.c:1644-1648.

---

## Decision: T27 — Deadband NONE fall-through to equality

**Decision**: When `deadband_type == MU_DEADBAND_TYPE_NONE`, instead of
returning true immediately, fall through to the value equality comparison.
If the value changed, return true; if unchanged, return false.

**Rationale**: Per OPC UA spec, deadband NONE means "report on any value
change." The current code interprets NONE as "report always," which generates
unnecessary notifications. The fix changes the early-return to a fall-through,
preserving the absolute deadband and percentage deadband paths while adding
correct NONE semantics.

**Alternatives considered**:
- Add new comparison branch: duplicates code. Fall-through is simpler.
- Change only the return value: `return (value != last_reported)` — misses
  the existing absolute/percentage deadband branches below. Fall-through
  preserves them.

**References**: OPC-10000-4 §7.22.2 (DataChangeFilter.deadbandType). Existing
code at subscription.c:144-145.

---

## Decision: T28 — Base nodes guard consolidation

**Decision**: Wrap all type-system nodes in `base_nodes.c` (lines ~263-731)
under a single `#ifdef` block covering the entire section, instead of 35+
individual `#if`/`#endif` guards per-node.

**Rationale**: All 35+ guards use the same feature flag (profile-dependent).
There is no case where a subset of type-system nodes should be included
while others are excluded. Consolidating improves readability and reduces
the chance of guard mismatches during profile changes.

**Alternatives considered**:
- Leave as-is: "works, don't touch." But the audit identified this as a
  code clarity issue, and it's a 2-minute change.
- Split into sub-sections by node type: adds unnecessary nesting.

**References**: Existing code at base_nodes.c:263-731.

---

## Decision: T31 — Publish timer max-iteration guard

**Decision**: Add a loop counter to `advance_publish_timer`'s `do...while(0)`
that terminates after ~100 iterations. If the bound is exceeded, log a
warning and exit.

**Rationale**: When `publishing_interval` is 0 (or extremely small, like
1 microsecond), the catch-up loop could spin for UINT64_MAX iterations.
This is a configuration error, but the server should not hang. A bound of
100 iterations corresponds to catching up 10 seconds of missed cycles at
100ms interval — more than reasonable for any real deployment.

**Alternatives considered**:
- Reject interval=0 at configuration time: doesn't protect against very
  small non-zero intervals. Loop guard is defense-in-depth.
- Use timer comparison instead of counter: requires monotonic time check,
  more complex. Counter is simpler.

**References**: Existing `advance_publish_timer` at subscription.c:1132.
OPC-10000-4 §5.13.1.2.

---

## Decision: MISRA 15.6 — Platform crypto adapter braces

**Decision**: Mechanically add braces to all single-statement bodies
(if/for/while with single-statement bodies) in host_crypto_adapter.c,
mbedtls_crypto_adapter.c, and wolfssl_crypto_adapter.c.

**Rationale**: These are the last remaining MISRA 15.6 violations after
PRs #238 and #239 fixed ~2,100 others. The platform adapter files were
excluded from bulk fix scripts because auto-fixers corrupted arduino
adapter code (see HANDOFF.md lesson 6). Manual inspection of each site
is required to avoid corruption.

**Alternatives considered**:
- Skip: these are platform adapters, not core code. But they're part of
  the codebase and MISRA is already enforced.
- Automated fix: caused corruption in arduino adapter (see lesson 6).

**References**: MISRA-C:2012 Rule 15.6. HANDOFF.md lesson 6.
