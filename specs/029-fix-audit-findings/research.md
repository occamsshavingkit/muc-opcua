# Research: Five-Lens Audit Findings Cleanup

**Feature**: 029-fix-audit-findings
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md`
**Date**: 2026-07-04

## Decision 1: ActivateSession nonce fail-closed (T1)

**Decision**: `fill_server_nonce()` returns a StatusCode; `handle_activate_session` rejects with `MU_STATUS_BAD_SECURITYCHECKSFAILED` on failure.

**Rationale**: CreateSession and OPN already fail-closed on entropy failure. ActivateSession was the only auth path that silently fell back to zero nonce. This is inconsistent and creates a replay vulnerability. OPC-10000-4 §7.38.2 requires ServerNonce to be cryptographically random.

**Alternatives considered**:
- Keep zero-fallback with a logged warning: rejected — silent security degradation is unacceptable per Constitution V.
- Use a compile-time static nonce: rejected — still replayable, just different value.

## Decision 2: Password buffer location (T2)

**Decision**: Zeroize the existing stack `decrypt_buf[256]` with `mu_secure_zero()` at both exit points. Do NOT relocate to scratch region in this feature.

**Rationale**: The audit report suggests moving to `MU_SECURE_SESSION_MAX` scratch, but that requires additional layout validation and risks buffer overlap issues. Zeroizing the stack buffer is a 1-line fix that achieves the same security goal with zero risk. Relocation can happen in a follow-up if scratch capacity permits.

**Alternatives considered**:
- Move to scratch sub-region: deferred — adds complexity for no additional security benefit beyond zeroization.
- Ignore: rejected per Constitution V.

## Decision 3: Browse reference assignment (T3)

**Decision**: Before the `continue` in the `too_many_refs` path at `browse.c:390`, set `res->references` and `res->num_references` from the pool.

**Rationale**: The references have already been written into the pool. The only bug is that the result struct isn't informed. A two-line fix. Per OPC-10000-4 §5.9.2, the server must return references that fit plus a continuation point.

**Alternatives considered**:
- Return partial results only when a continuation point is supported: rejected — explicit continuation-point support is out of scope; current behavior returns the fit references plus signals continuation-needed.

## Decision 4: Subscription single-pass scan (T4)

**Decision**: Collect reportable item indices into a fixed-size index array (not a bitmap) during the sampling pass, then iterate only over those indices in encoding and cleanup passes.

**Rationale**: The current triple-scan calls `count_reportable_items()`, `write_data_change_notification()`, and `clear_reported_items()` each scanning the full array. An index array of `MU_MAX_MONITORED_ITEMS` `uint16_t` values takes at most ~200 bytes on the stack in the `mu_subscriptions_tick` function (which already has stack budget for item processing). A bitmap would be smaller but requires bit-twiddling that adds code on M0+.

**Alternatives considered**:
- Bitmap: rejected for M0+ code-size impact of bit operations.
- Skip count pass only: leaves 2 passes — incomplete fix.

## Decision 5: fabs removal (T5, T24)

**Decision**: Replace `fabs((double)numeric - (double)item->last_reported_numeric)` with `opcua_double_t diff = numeric - item->last_reported_numeric; if (diff < 0.0) diff = -diff;`. Remove `#include <math.h>`.

**Rationale**: `double` subtraction, comparison, and negation are compiler built-ins (from compiler-rt/libgcc), not `<math.h>`. Only `fabs()` pulls in the math library. The inline version is bit-identical for IEEE 754 doubles. On Cortex-M0+, this removes the ~12 KB double-precision math emulation library from the link.

**Alternatives considered**:
- Switch to `float` + `fabsf()`: rejected — still pulls in math library for `fabsf()`.
- Use integer-only deadband for all types: rejected — would change behavior for Float/Double types; this feature preserves wire-identical output.

## Decision 6: 64-bit division avoidance (T6)

**Decision**: In `advance_sample_timer`, the common case (`elapsed < interval`) already returns without division. For the catch-up path, replace `opcua_uint64_t steps = (elapsed / interval) + 1u` with a repeated-subtraction loop capped at a small maximum (e.g., 100 iterations).

**Rationale**: `steps` is typically 1-5 in practice (short catch-up). A 64-bit `__aeabi_uldivmod` call costs ~300 cycles on M0+. Repeated subtraction with small iteration count is faster and avoids pulling in the 64-bit division runtime.

**Alternatives considered**:
- Use 32-bit division: rejected — `elapsed` and `interval` are `uint64_t` for ms rollover safety; truncation risks.
- Leave as-is: rejected — found on hot path by performance audit.

## Decision 7: OAEP explicit parameters (T7)

**Decision**: Add `EVP_PKEY_CTX_set_rsa_oaep_md(pc, EVP_sha1())` and `EVP_PKEY_CTX_set_rsa_mgf1_md(pc, EVP_sha1())` to the OpenSSL host adapter's `rsa_oaep()` function.

**Rationale**: The existing `rsa_oaep_sha256()` already does this correctly. The SHA-1 variant relies on OpenSSL defaults, which could change in future versions. This is a defensive fix — no behavior change, just explicit specification.

**Alternatives considered**: None. This is purely defensive.

## Decision 8: Session create ordering (T8)

**Decision**: Move `write_response_prefix()` before `mu_session_create_with_identifiers()`.

**Rationale**: If the response encoding fails after the session is created, the session is orphaned. Moving the prefix write before session creation means encoding failures happen before state is mutated. The prefix write is a status-code write and a NodeId write — both bounded and non-failable under normal conditions.

**Alternatives considered**:
- Add cleanup on failure: more code, same effect.
- Leave as-is: rejected — dangling session on encode failure is a correctness bug.

## Decision 9: DeleteNodes deleteTargetReferences (T9)

**Decision**: Guard the reference-cleanup loop with `if (items[i].delete_target_references)`.

**Rationale**: The field is already decoded from the wire. The fix is a one-line guard. Per OPC-10000-4 §5.8.5.2, when the flag is false, only the node should be removed — references from surviving nodes must remain.

**Alternatives considered**:
- Remove the field decode: rejected — needed for the guard to work correctly.

## Decision 10: Value source type coverage (T11)

**Decision**: Replace the narrow `switch` in `mu_value_source_read` with a broad default that returns the value directly for all scalar built-in types. Keep the array rejection.

**Rationale**: The current switch only covers Boolean, Int32, UInt32, Float, String. Types like Byte, Double, UInt16, Int64, DateTime, StatusCode are incorrectly rejected with `BAD_NOTREADABLE`. Per OPC-10000-3 §5.6, any valid built-in type's Value attribute must be readable.

**Alternatives considered**:
- Add each missing type to the switch: more verbose, same result, harder to maintain.
- Keep as-is: rejected — violates OPC UA spec.

## Decision 11: Deadband None notification spam (T27)

**Decision**: When `deadband_type == MU_DEADBAND_TYPE_NONE`, fall through to the value-equality comparison (already done in the caller `monitored_item_sample_changed`). Do NOT unconditionally return `true`.

**Rationale**: Deadband None means "report on any change." Currently it reports on EVERY sample tick regardless of change. The fix preserves the semantics: None = report when value or status actually changes. The value-equality comparison in the caller already handles this correctly.

**Alternatives considered**:
- Add explicit equality check in `monitored_item_change_reportable`: would duplicate logic from the caller.

## Decision 12: Encoding mask validation (T29)

**Decision**: Change `if ((encoding_mask & 0x01) == 0)` to `if (encoding_mask != 0x01)` in the HistoryRead ExtensionObject decoder.

**Rationale**: Per OPC-10000-6 §5.2.2.15, encoding masks MUST be 0x00 (NoBody), 0x01 (ByteString), or 0x02 (XmlElement). The current check accepts 0x03 (both bits set), which is invalid. The stricter check rejects all invalid masks.

**Alternatives considered**:
- Check `encoding_mask & 0xFE`: equivalent, less readable.

## Decisions 13-42: Tier 2/3 items

All Tier 2 and Tier 3 items have straightforward, single-file fixes as described in the audit report. No design trade-offs require research. The documentation-only items (T14 Pico README, T16 trust model docs, T33 session ID constraint, T34 DER-comparison) are prose additions to existing `docs/` or source comments.

## Unresolved questions

None. All 42 findings have clear fixes documented in the audit report.
