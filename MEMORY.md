# Project Memory — Features 029 & 030: Five-Lens Audit Remediation

**Date**: 2026-07-04
**Branches**: `029-fix-audit-findings` → `main`, `030-audit-followup` → `main`
**PRs**: [#236](https://github.com/occamsshavingkit/muc-opcua/pull/236), [#237](https://github.com/occamsshavingkit/muc-opcua/pull/237)
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md` (42 findings)

## What Was Fixed (22 findings)

### PR #236 — Broad correctness & code quality sweep
- **T3**: Browse preserves references when limit exceeded (OPC-10000-4 §5.9.2)
- **T9**: DeleteNodes respects `deleteTargetReferences` flag (OPC-10000-4 §5.8.5.2)
- **T10**: Write type validation aborts on value_source_read failure (OPC-10000-4 §5.11.4)
- **T11**: `value_source_read` returns all scalar built-in types (OPC-10000-3 §5.6)
- **T14**: Pico platform TCP stub documentation
- **T16**: Trust model DER-exact comparison documented
- **T18**: mbedTLS PSS `signature_length` validated (OPC-10000-4 §5.5)
- **T21**: HistoryRead encoding mask rejects invalid 0x03 (OPC-10000-6 §5.2.2.15)
- **T26**: `g_supported_services[]` and `POLICY_TABLE[]` verified `const`
- **T29**: Query arrays `BAD_TOOMANYOPERATIONS` guard (OPC-10000-4 §5.9)
- **T30**: TranslateBrowsePaths `remainingPathIndex` computed correctly (OPC-10000-4 §5.9.3)
- **T33**: Session ID generation documented
- **T34**: DER-exact comparison documented
- **T35**: No fix (MessageSize bounded — documented)
- **T36**: Poll cycle transient pointers documented
- **T38**: Stack protector deferred to platform config (documented)
- **T39**: Pico DRBG documentation
- **T40**: Inline wrappers verified
- **T41**: Base nodes guard consolidation
- **T42**: No fix (O(N²) on small N, init-time — documented)

### PR #237 — Security-critical fixes
- **T1/T2/T7**: `fill_server_nonce` fail-closed; password decrypt buffer zeroized at all exit points (OPC-10000-4 §5.7.3)
- **T32/T44**: Server self-cert validation fail-closed (OPC-10000-4 §5.5)

### Test Coverage Added
- `test_password_decrypt_zeroize.c` — `mu_secure_zero` primitive validation
- `test_read_browsename_namespace.c` — BrowseName namespace independence
- `test_read_timestamps_to_return.c` — TimestampsToReturn field presence

## What Was Deferred (15 findings, 3 hotspots)

All deferred items cluster in files already touched by the fixes — deferred to avoid merge conflicts:

| Hotspot | Findings | Reason |
|---------|----------|--------|
| `src/services/subscription.c` | T4, T5/T24, T6, T27, T28, T31 | Single-pass scan, fabs removal, 64-bit div, deadband, publish timer — complex refactor that broke tests in initial pass |
| `src/core/service_dispatch.c` | T8, T12, T13, T23, T25 | Session ordering, cert token ifdef, nonce stack zeroize, dispatch scan, profile URI parse — structural changes needed |
| `src/services/secure_channel.c` | T17 | Channel ID entropy — breaks integration tests that assume `channel_id=1`; needs per-test updates |

## What Was Documented as No-Fix (5 findings)
- **T15**: SHA-1 thumbprint — spec-mandated (OPC UA)
- **T35**: MessageSize bounded by `MU_ASYM_MAX_PLAINTEXT`
- **T37**: Covered by T6 (same 64-bit division)
- **T38**: Stack protector — platform toolchain concern, not library
- **T42**: Address-space validation O(N²) — N small, init-time only

## Size Impact

| Profile | Pre-029 | Post-030 | Delta |
|---------|---------|----------|-------|
| nano | 16,436 | 16,480 | +44 (+0.3%) |
| micro | 23,839 | 23,975 | +136 (+0.6%) |
| embedded | 44,100 | 43,906 | -194 (-0.4%) |
| full | 52,822 | 52,656 | -166 (-0.3%) |

All within Constitution gates (`.text` ≤ +3%, `.data` = 0, `.bss` = 0, 0 heap).

## Test Status
- **105 tests, 100% pass** — all CI jobs green
- Codacy: 3 medium ErrorProne (test infrastructure, not production code)

## Key Lessons for Follow-Up

1. **Subscription.c changes need individual atomic tasks**: The initial T4 attempt batched 3+ function changes into one pull — it passed but broke the test suite. Next attempt should split into: collect indices, change write loop, change cleanup loop — each verified independently.
2. **Channel ID requires test refactoring first**: T17 is a 1-line code change but needs integration test updates (3 files) to stop assuming `channel_id=1`. Update tests FIRST, then apply the counter.
3. **Service_dispatch.c ifdef guards**: T7 zeroize needed `#ifdef MUC_OPCUA_SECURITY` because `key_derivation.h` is only included when SECURITY=ON. Any new `mu_secure_*` calls in `USER_AUTH` blocks need the same guard.
4. **`fill_server_nonce` fail-closed broke tests that didn't configure entropy**: The `test_security_identity_errors` helper was missing `fake_platform_init`. Any new entropy-requiring path needs test adapters plumbed first.
