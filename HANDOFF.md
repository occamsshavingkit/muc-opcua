# Handoff — 2026-07-04 (updated after PR #241)

## Completed on main

### Five-Lens Audit Remediation (#236, #237, #238, #239, #240, #241)

| PR | What | Status |
|----|------|--------|
| #236 | 18 correctness/code-quality fixes from 42 audit findings | Merged |
| #237 | 4 security-critical fixes (nonce fail-closed, zeroize, cert validation) | Merged |
| #238 | 2,167 style fixes (1,896 braces, 271 void casts) + `mu_checked_memcpy` wrapper | Merged |
| #239 | 104 remaining code fixes (braces, void casts), arduino adapter repair | Merged |
| #240 | 13 deferred audit findings (subscription hot-path + dispatch correctness/security/perf) + 12 style fixes | Merged |
| #241 | Source file splitting (subscription.c → 3 modules, service_dispatch.c → 1 core + 7 handler modules) + WriteResponse diagnosticInfos fix | Merged |

**Test status**: 108/108 pass. All CI green. Size within constitution gates.

**Audit status**: 35 of 42 findings remediated. 7 remain (T17 blocked, T22 structural, 5 doc-only).

### Codacy: Fully triaged

### Source file structure

| Before | After |
|--------|-------|
| `subscription.c` (1,825 lines) | `subscription_monitor.c` + `subscription_publish.c` + `subscription_aggregate.c` |
| `service_dispatch.c` (4,509 lines) | 7 `dispatch_*.c` handler modules + `service_dispatch.c` (2,156 lines, 52% reduced) |

## Remaining Backlog

### Deferred audit findings (2 hotspots)

| File | Findings | Notes |
|------|----------|-------|
| `src/services/secure_channel.c` | T17 | Channel ID entropy. Fix 3 integration tests first, then change counter. |
| `src/services/browse.c` | T22 | TypeDefinition cache. Requires adding field to `mu_node_t` struct. |

### Interop test hardening (TODO.md)

WriteResponse missing `diagnosticInfos[]` was undetected — interop smoke has 0 write tests, unit test didn't read the full response. Audit all encode/decode tests for complete field coverage.
