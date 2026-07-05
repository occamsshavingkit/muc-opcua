# Research: Clear Remaining Backlog

**Feature**: 039-clear-remaining-backlog
**Date**: 2026-07-05

## Decision 1: Portable CTZ Fallback (CQ6)

**Decision**: Replace `__builtin_ctz()` with `mu_ctz_u32()` that uses
`__builtin_ctz` when GCC/Clang detected, falling back to a de Bruijn sequence
lookup table otherwise. Handle zero input by returning 32 (matching
`__builtin_ctz`'s documented "undefined for zero" — callers that may pass zero
must guard first).

**Rationale**: The de Bruijn method is the standard portable CTZ implementation
used in Linux kernel (`include/asm-generic/bitops/__ffs.h`), musl libc, and
FreeBSD. It requires a single 32-entry lookup table (128 bytes static const)
and one multiply. Faster than a linear bit-scan loop for all 9 call sites.

**Alternatives considered**:
- `__builtin_ctz` + `#error` on unsupported compilers: Too aggressive; would
  block compiling on MSVC, IAR, etc.
- Linear bit-scan `while ((x & 1) == 0) { x >>= 1; count++; }`: Simple but
  O(bits) worst case vs O(1) de Bruijn.
- Compiler-provided `<stdbit.h>` (C23 `stdc_trailing_zeros`): Not portable to
  C11.

## Decision 2: LE uint32 Write Consolidation (CQ7)

**Decision**: Audit all three duplicate helpers across encoding files
(binary_reader.c, binary_writer.c, binary_nodeid.c). Standardize on the
implementation in `binary_writer.c` (`mu_binary_write_u32_le`) since it already
handles edge cases (ensure_space, buffer overflow). Move declaration to
`src/encoding/binary_encoding.h` (internal) and update all call sites.

**Rationale**: All three implementations are functionally identical — 4-byte
little-endian write with bounds check. Consolidation reduces code size and
eliminates the risk of one implementation diverging in behavior.

**Alternatives considered**:
- Inline macro: Would avoid a function call but duplicates bounds-check logic
  at each call site, inflating code size.
- Leave as-is: Three copies of identical logic is a maintenance liability
  flagged by the comprehensive review.

## Decision 3: Message Dispatch Deduplication (CQ8)

**Decision**: Extract shared dispatch logic from the three duplicate patterns
in `server.c` into `mu_server_dispatch_message()` in `src/core/dispatch.c`.
The existing decomposition from Spec 038 (CQ2) handles poll-loop duplication;
this addresses message-type dispatch duplication where three nearly-identical
if/else chains exist in different functions.

**Rationale**: These three copies diverged over time and have subtle
differences. A single dispatch function with a service-type parameter
eliminates the maintenance risk.

**Alternatives considered**:
- Function pointer table: Over-engineered for the current 3-copy case;
  adds indirection without measurable benefit.
- Macro: Harder to debug; doesn't improve readability.

## Decision 4: Read Cache Enablement (HP2)

**Decision**: The read cache infrastructure exists but `mu_read_cache_lookup()`
is called with parameters that always produce a cache miss (e.g., comparing
against uninitialized timestamps). Fix: ensure cache entries are populated
on first read with the current timestamp and value; on subsequent reads,
compare node version or use a simple TTL.

**Rationale**: The cache data structure (`mu_read_cache_entry`) is already
allocated statically in the server struct. The bug is purely in the lookup
logic — the cache is populated but never matched due to a timestamp comparison
error. Minimal fix: correct the timestamp tracking.

**Alternatives considered**:
- Remove cache entirely: Would eliminate the dead code but lose the ~20%
  performance gain the cache was designed to provide.
- Version-based invalidation: More correct than TTL but requires address-space
  version tracking infrastructure not yet built.

## Decision 5: Session Timeout Scan Optimization (HP9)

**Decision**: Add a `next_session_timeout_ms` field to the server struct,
updated whenever a session is created/refreshed. The poll loop checks this
field and only scans session slots when a timeout is actually due, rather
than scanning all slots every poll cycle.

**Rationale**: With 50+ session slots and a 10ms poll interval, the full
linear scan wastes CPU. The "next expiry" pattern is standard in event-loop
design (similar to timer-wheel optimization) and adds only 8 bytes of state.

**Alternatives considered**:
- Timer heap/priority queue: Correct but over-engineered for the session
  count; adds allocator dependency.
- Ignore: Acceptable for nano/micro profiles (few sessions), but 50 sessions
  in standard profile makes this measurable.

## Decision 6: Constant-Time Trust List Comparison (SE3)

**Decision**: Replace trust list `memcmp()` with a constant-time comparison
using a simple byte-wise XOR accumulation pattern:

```c
int mu_constant_time_memcmp(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff; /* 0 = match, non-zero = difference */
}
```

**Rationale**: Standard constant-time comparison pattern used in cryptographic
libraries (OpenSSL `CRYPTO_memcmp`, libsodium `sodium_memcmp`). Eliminates the
timing side-channel in certificate thumbprint comparison without adding
dependencies.

**Alternatives considered**:
- OpenBSD `timingsafe_memcmp`: Not universally available.
- Volatile pointer trick: Compiler-dependent; may be optimized away.
- Leave as-is: Certificate trust list comparison is not on the hot path, but
  security review correctly identifies it as a timing leak.

## Decision 7: EventFilter select_count Cap (SE4)

**Decision**: Add `#define MU_EVENT_FILTER_MAX_SELECT_COUNT 100` in config.h
and enforce it in `read_event_filter_body()`. Return Bad_EventFilterInvalid
if exceeded.

**Rationale**: Without a cap, a malicious client can allocate unbounded memory
on the server by specifying a huge select_count. The OPC UA spec (OPC-10000-4
§7.22) doesn't mandate a specific limit but 100 clauses is far beyond practical
use (the CTT tests with ≤10 clauses).

**Alternatives considered**:
- Make cap configurable via CMake flag: Adds complexity; 100 is generous enough
  for any legitimate use.
- Stack-allocate variable-length: Not portable C11; VLAs are optional in C11.

## Decision 8: Duplicate Node Check Optimization (HP11)

**Decision**: Replace O(N^2) startup duplicate check with a sort-then-scan
approach: sort node pointers by NodeId, then scan for adjacent duplicates.
The address space is static at startup, so sorting once is acceptable.

**Rationale**: For a standard profile address space with ~500 nodes, O(N^2)
= 250K comparisons vs O(N log N) ≈ 4.5K operations. The address space is
initialized once at startup, so sort overhead is paid only at init time.

**Alternatives considered**:
- Hash set: Requires heap allocation for the hash table; violates memory model.
- Skip entirely: Not acceptable — duplicate nodes are a real configuration bug.
- Accept O(N^2) for now: Address spaces are small enough that N^2 may not be
  measurable at typical sizes (50-100 nodes). But the review flags it, so fix
  it while we're cleaning up.

## Decision 9: ADR Format and Location

**Decision**: Place ADRs in `docs/adr/` using the Michael Nygard format
(markdown files named `NNNN-title-with-dashes.md`). ADRs to create:
- `0001-zero-heap-design.md`: Rationale for static/caller-provided memory model
- `0002-adapter-pattern.md`: Platform abstraction layer design
- `0003-profile-tier-system.md`: Nano/Micro/Embedded/Standard profile gating
- `0004-poll-model.md`: Single-threaded cooperative poll loop design

**Rationale**: The Nygard format is widely adopted (Google, AWS, ThoughtWorks)
and provides a consistent structure: Title, Status, Context, Decision,
Consequences. The project already uses markdown for all docs.

**Alternatives considered**:
- Comment-based ADRs in source: Harder to discover; not linked from README.
- Single DESIGN.md: Becomes unwieldy as decisions accumulate.
