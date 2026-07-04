# Handoff — 2026-07-04

## Completed on main

### Five-Lens Audit Remediation (#236, #237, #238, #239)

| PR | What | Status |
|----|------|--------|
| #236 | 18 correctness/code-quality fixes from 42 audit findings | Merged |
| #237 | 4 security-critical fixes (nonce fail-closed, zeroize, cert validation) | Merged |
| #238 | 2,167 style fixes (1,896 braces, 271 void casts) + `mu_checked_memcpy` wrapper | Merged |
| #239 | 104 remaining code fixes (braces, void casts), arduino adapter repair | Merged |

**Test status**: 105/105 pass. All CI green. Size within constitution gates.

### Codacy Triage: 3,598 → ~0

- Configured `.codacy.yaml` with engine exclusions (flawfinder, lizard, markdownlint, shellcheck)
- Added `src/core/safe_mem.h` with `mu_checked_memcpy`/`mu_checked_memcpy_off` inline wrappers
- Suppressed false-positive patterns via CLI (memset, memcpy, strlen, MISRA noise)
- Created custom coding standard in Codacy UI for 5 persistent patterns
- Triage report: `docs/review/codacy-issue-triage-2026-07-04.md`

## Backlog

### Deferred audit findings (3 hotspots)

| File | Findings | Notes |
|------|----------|-------|
| `src/services/subscription.c` | T4, T5/T24, T6, T27, T28, T31 | Single-pass scan, fabs removal, 64-bit div, deadband, publish timer. Split into atomic tasks. |
| `src/core/service_dispatch.c` | T8, T12, T13, T23, T25 | Session ordering, cert token ifdef, nonce stack zeroize, dispatch scan, profile URI parse |
| `src/services/secure_channel.c` | T17 | Channel ID entropy. Fix 3 integration tests first, then change counter. |

### Remaining style fixes

- 10 MISRA 15.6 single-statement bodies in platform crypto adapters (wolfssl, host_crypto, mbedtls)
- 2 MISRA 10.4 `size_t` vs `0u` type inconsistencies in `subscription.c:1576,1610`

### Codacy

- Finish custom coding standard in Codacy UI (finalize pattern exclusions)
- Tune Lizard complexity thresholds (CCN>8, NLOC>50, params>5, file>500 are too low for protocol code)

### Tech debt

- `subscription.c` is 1,692 lines — split into publish, monitor, filter modules
- `service_dispatch.c` is large — extract per-service dispatch

## Key lessons

1. **Subscription.c changes need individual atomic tasks** — batching broke tests
2. **Channel ID (T17) needs test refactoring first** — 3 tests assume `channel_id=1`
3. **`mu_secure_*` in `USER_AUTH` blocks must be `#ifdef MUC_OPCUA_SECURITY` guarded**
4. **Codacy CLI ignores don't survive rebase merges** — only coding-standard changes in the UI are permanent
5. **`clang-tidy` + `clang-format` brace fix pipeline works** — tidy adds braces, format fixes indent
6. **Auto-fixer scripts can corrupt platform stubs** — arduino adapter got `{ {` and `(void)static`. Always inspect platform files.
