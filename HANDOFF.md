# Handoff — 2026-07-05 (end of session)

## Completed

### Audit Remediation (#240–#248)

| PR | What | Status |
|----|------|--------|
| #240 | 13 deferred audit findings (subscription hot-path + dispatch correctness/security/perf) + 12 style fixes (MISRA 15.6 + 10.4) | Merged |
| #241 | Source file splitting: subscription.c → 3 modules, service_dispatch.c → core + 7 handler modules. WriteResponse diagnosticInfos fix. | Merged |
| #242 | Interop test hardening: full encoder field coverage audit, write interop test | Merged |
| T17 direct | SecureChannel ID: incrementing counter. Integration tests refactored to read actual channel_id from OPN responses. | Merged |
| #243 | TypeDefinition cached on mu_node_t (audit T22). Browse O(R*T) → O(1) | Merged |
| #244 | 22 spec compliance fixes: 5 critical, 8 high, 9 medium. ClientNonce validation, ApplicationUri cert check, CloseSecureChannel handler, etc. | Merged |
| #245 | 15 missing OPC UA features: subscription lifecycle, read index_range/attributes, write timestamps, browse resultMask, GUID/Opaque NodeId, multi-chunk | Merged |
| #246 | Profile gating: multi-chunk + GUID/Opaque gated OFF in nano | Merged |
| #247 | Nano test fix for message_chunk_errors | Merged |
| #248 | Nano slim: gated non-mandatory features (session timeout, read index_range, extra attributes) behind profile flag (-597 B) | Merged |

### Spec Compliance Audit

`docs/audit/spec-compliance-2026-07-05.md` — comprehensive audit of all 22 implemented services against OPC-10000-4 and OPC-10000-6. All INCORRECT and NOT IMPLEMENTED findings resolved.

### Code Quality

| Metric | Value |
|--------|-------|
| Tests | 108/108 pass |
| ASAN/UBSan | 72/72 pass |
| All profiles | Green |
| `.data`, `.bss`, heap | 0 across all profiles |
| Interop | Both Python asyncua and .NET reference client pass |

### Profile Sizes (ARM Cortex-M0+)

| Profile | Text | vs Baseline (spec 032) |
|---------|------|----------------------|
| nano | 17,392 B | +834 B (+5.0%) |
| micro | 27,818 B | +3,633 B (+15%) |
| embedded | 52,525 B | +7,793 B (+17%) |
| full | 61,281 B | +7,799 B (+15%) |

Growth is from mandatory spec requirements (nonce validation, cert checks, variant arrays, compliance fixes). Nano aggressively trimmed — all non-mandatory features gated.

### Source Structure

| Before | After |
|--------|-------|
| `subscription.c` (1,825 lines) | `subscription_monitor.c` + `subscription_publish.c` + `subscription_aggregate.c` |
| `service_dispatch.c` (4,509 lines) | 7 `dispatch_*.c` handler modules + `service_dispatch.c` (2,156 lines, 52% reduced) |

## Remaining Backlog

None. All identified work complete:
- Five-lens audit findings: 42/42 remediated
- Codacy triage: 3,598 → ~0
- Spec compliance audit: INCORRECT + NOT IMPLEMENTED findings all resolved
- Source file splitting: complete
- Interop test hardening: complete

## Key Lessons

1. **Feature branches always** — even for small fixes. Committing T17 directly to main was wrong.
2. **Integration tests hardcode channel_id** — refactored to read from OPN responses (test_view_services.c pattern).
3. **WriteResponse missing diagnosticInfos[]** was undetected because interop had 0 write tests and encoder test stopped reading mid-response. All encoder tests now read full response to end-of-buffer.
4. **Audit findings can be false positives** — I7 (CreateSubscription sampling interval=-1) and I21 (ServerUri/EndpointUrl validation) were spec-incorrect. Always verify against primary sources.
5. **Nano profile gating** — MUC_OPCUA_MULTI_CHUNK flag serves as "not nano" proxy for gating non-mandatory features. Add profile-controlled CMake options for new features.
6. **BSS violations** — static read cache introduced 324 B of mutable global state. Fixed by disabling cache (caching is optional per spec).
7. **Merge after CI** — don't merge PRs before fresh CI run on latest commit.
