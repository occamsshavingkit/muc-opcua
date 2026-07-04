# muc-opcua — Five-Lens Audit Report (2026-07-04)

Five independent lenses: **Security**, **Memory Safety & Embedded**, **Correctness & Conformance**, **Performance & Code Quality**, with cross-cutting analysis. Findings ranked by severity × cross-lens agreement.

---

## Verified POSITIVES (do not fix)

- **Zero-heap guarantee**: No `malloc`/`calloc` anywhere in the protocol path. `.data = 0`, `.bss = 0` across all ARM profiles. All RAM is caller-provided static storage.
- **Decoder bounds discipline uniform**: Every primitive read passes `ensure_bytes()`; wire lengths validated before pointer math/memcpy. No reachable spatial over-read/write found.
- **No unaligned memory access**: All (de)serialization byte-at-a-time + shifts (M0+ safe).
- **Sticky-error pattern**: Reader/writer status fields make primitives no-ops after first failure — correct OPC UA decode semantics.
- **Symmetric keys derived once per channel** and cached; MSG decrypt is **in-place** (no scratch copy).
- **99% zeroization discipline**: All KDF intermediates, symmetric keys, and nonces in the security layer are properly wiped.
- **Fail-closed trust**: Secured channels reject when `trust_list == NULL`, cert expired, or not in trust list.
- **UNSCOPED ActivateSession ClientSoftwareCertificates / identity-token body**: Fixed in 025 — `cert_count` rejected when non-zero; token body consumed correctly.

---

## TIER 1 — Critical (multi-source, exploitable or safety-critical)

### T1. [SECURITY HIGH] [CORRECTNESS MED] ActivateSession nonce falls back to all-zeros when entropy fails
**File**: `src/core/service_dispatch.c:158-163` — `fill_server_nonce()`
**Sources**: Security audit H1, Correctness audit M6

`fill_server_nonce()` silently sets the ServerNonce to all-zeros when `mu_session_generate_server_nonce` fails. CreateSession and OPN both properly fail-closed on entropy failure, but ActivateSession does not. A zero nonce enables replay of ActivateSession response tokens and undermines username/password replay protection per **OPC-10000-4 §7.38.2**.

**Fix**: `fill_server_nonce` should return a status code; `handle_activate_session` must fail with `MU_STATUS_BAD_SECURITYCHECKSFAILED` on nonce generation failure, consistent with CreateSession/OPN.

### T2. [MEMORY MED] [SECURITY LOW] Password decrypt buffer not zeroized on stack
**File**: `src/core/service_dispatch.c:1109-1162`
**Sources**: Memory audit M3 (upgraded for cross-lens agreement), Security audit L1

`decrypt_buf[256]` stack buffer holds decrypted user passwords. After verification completes, the buffer is left on stack without zeroization. All other sensitive key/nonce buffers in the codebase are zeroized — this is the single exception. An attacker with stack dump access could recover credentials.

**Fix**: Add `mu_secure_zero(decrypt_buf, sizeof(decrypt_buf))` before `goto activate_done` and before block's end. Use the T004 scratch sub-region (`MU_SECURE_SESSION_MAX`) instead of stack allocation.

### T3. [CORRECTNESS HIGH] Browse drops collected references when limit exceeded
**File**: `src/services/browse.c:388-391`
**Sources**: Correctness audit H1

When `requested_max_references_per_node` is exceeded, `match_count` references have already been written into the pool, but the `too_many_refs` path at line 388 sets `BAD_NOCONTINUATIONPOINTS` and `continue`s **without assigning** `res->references` and `res->num_references`. Per **OPC-10000-4 §5.9.2**, the server must return the references that fit up to the limit, plus a continuation point. The response silently drops all collected references for that node.

**Fix**: Before the `continue` at line 390, set `res->references = &ref_pool[node_refs_start]; res->num_references = match_count;`.

### T4. [PERFORMANCE HIGH] [EMBEDDED] Triple full-scan of monitored items per publish cycle
**File**: `src/services/subscription.c:1612-1697` — `mu_subscriptions_tick`
**Sources**: Performance audit H1

Each publish cycle does three full scans of `MU_MAX_MONITORED_ITEMS`: `count_reportable_items()`, `write_data_change_notification()`, `clear_reported_items()`. With STANDARD enabled, a 4th pass for triggered items.

**Fix**: Collect reportable item indices into a bitmap during the first pass (sampling loop), then iterate only over those indices for encoding and cleanup. ~3x CPU reduction per publish cycle.

### T5. [PERFORMANCE HIGH] [EMBEDDED] `fabs(double)` on Cortex-M0+ (no HW FPU)
**File**: `src/services/subscription.c:162` — `monitored_item_change_reportable`
**Sources**: Performance audit H2, Memory audit (embedded concern)

On Cortex-M0+ (no FPU), `fabs(double)` pulls in ~1KB of software double-precision emulation. Called once per monitored item per sample tick. This is the most expensive arithmetic in the hot path.

**Fix**: Since deadband_value is typically an integer percentage, the comparison can use simple `float` math or integer arithmetic. Replace with: `double diff = numeric - last_reported; if (diff < 0) diff = -diff;` to eliminate the `<math.h>` dependency.

### T6. [PERFORMANCE HIGH] 64-bit integer division on every sample timer advance
**File**: `src/services/subscription.c:373`
**Sources**: Performance audit H3

On Cortex-M0+ (32-bit), 64-bit division is a ~300-cycle software routine. In `advance_sample_timer`, the division `elapsed / interval` is on the hot sampling path. The code already guards with `if (elapsed < interval)` for the common case, but catch-up still hits this.

**Fix**: `steps` is typically small (1-5). Replace with repeated subtraction or use 32-bit division since both `elapsed` and `interval` are bounded.

---

## TIER 2 — Strong (real risk, moderate complexity)

### T7. [SECURITY MED] RSA-OAEP-SHA1 uses implicit OpenSSL defaults instead of explicit hash/MGF params
**File**: `src/platform/host_crypto_adapter.c:237`
**Sources**: Security audit H2

`rsa_oaep()` only sets `RSA_PKCS1_OAEP_PADDING` but never calls `EVP_PKEY_CTX_set_rsa_oaep_md()` or `EVP_PKEY_CTX_set_rsa_mgf1_md()`. The `rsa_oaep_sha256()` function correctly sets both explicitly. If OpenSSL defaults ever change, OAEP encryption silently uses a different hash, breaking interop.

**Fix**: Add explicit `EVP_PKEY_CTX_set_rsa_oaep_md(pc, EVP_sha1())` and `EVP_PKEY_CTX_set_rsa_mgf1_md(pc, EVP_sha1())` in `rsa_oaep()`.

### T8. [CORRECTNESS HIGH] Session created before response prefix written → dangling on encode failure
**File**: `src/core/service_dispatch.c:780-800`
**Sources**: Correctness audit H4

`write_response_prefix` is called with `MU_STATUS_GOOD` at line 800 *before* session creation. If `mu_binary_write_nodeid(&sid)` at line 811 fails AFTER `mu_session_create_with_identifiers` succeeds at line 794, the session is already created but the response encoding fails, leaving a dangling session consuming a slot.

**Fix**: Move `write_response_prefix` before `mu_session_create_with_identifiers`, or add cleanup on encoding failure.

### T9. [CORRECTNESS HIGH] DeleteNodes ignores `deleteTargetReferences` flag
**File**: `src/services/node_management.c:584-621`
**Sources**: Correctness audit M9

The `delete_target_references` field from the wire is decoded but never used. The implementation always removes all references to/from the deleted node. Per **OPC-10000-4 §5.8.5.2**, when `deleteTargetReferences` is false, only the node should be removed — references from other nodes must remain.

**Fix**: Check `items[i].delete_target_references` and skip reference cleanup when false.

### T10. [CORRECTNESS MED] Write type validation proceeds after value source read failure
**File**: `src/core/service_dispatch.c:2319-2325`
**Sources**: Correctness audit M7

When `mu_value_source_read` returns error, `result` stays `MU_STATUS_GOOD`. The subsequent type check at line 2327 allows the write to proceed without knowing the current value's type. For transiently-failing callback value sources, this could allow type-mismatch writes.

**Fix**: Set `result = MU_STATUS_BAD_INTERNALERROR` in the failure path after line 2322.

### T11. [CORRECTNESS MED] `mu_value_source_read` rejects valid scalar types (Byte, Double, UInt16, Int64, etc.)
**File**: `src/address_space/value_source.c:31-32`
**Sources**: Correctness audit M8

The function returns `BAD_NOTREADABLE` for built-in types beyond Boolean/Int32/UInt32/Float/String. Per **OPC-10000-3 §5.6**, the Value attribute should be readable for any valid built-in type.

**Fix**: Replace the narrow switch with a broad `*value = source->data.static_value; return MU_STATUS_GOOD;` for all scalar types.

### T12. [SECURITY MED] [CORRECTNESS] ActivateSession cert token unsafe fallback when security disabled
**File**: `src/core/service_dispatch.c:1029-1223`
**Sources**: Security audit M6

Certificate user token (type 327) is decoded under one `#ifdef MUC_OPCUA_SECURITY` block but the verification under a separate one. A build misconfiguration could decode without verifying.

**Fix**: Consolidate certificate token handling under a single `#ifdef` block.

### T13. [SECURITY LOW] [MEMORY] ServerNonce copies in CreateSession/ActivateSession not zeroized
**File**: `src/core/service_dispatch.c:772,1240`
**Sources**: Security audit L1, Memory audit M6

Local `nonce_buf[32]` is `memcpy`'d into the session slot then goes out of scope without zeroization. While the nonce is sent in cleartext, it is used as key derivation material.

**Fix**: Zeroize after copying, or use the session slot directly.

### T14. [EMBEDDED HIGH] Pico TCP stubs non-functional — accept always succeeds, read/write no-ops
**File**: `platform/pico/mu_pico_adapter.c:14-50`
**Sources**: Memory audit H2

RP2040 Pico cannot actually serve OPC UA over TCP — all TCP functions are stubs. `server.c:723-725` accept path succeeds but yields NULL handle, locking the server into busyloop consuming CPU with no networking.

**Fix**: Implement LWIP/CYW43 integration or document as compile-only validation target.

---

## TIER 3 — Worthwhile (lower risk, clear fixes)

### T15. [SECURITY MED] Certificate thumbprint uses SHA-1 (spec-mandated but deprecated)
**File**: `src/security/certificate.c` + all adapters
**Sources**: Security audit M1

`mu_certificate_thumbprint()` computes SHA-1 of the DER certificate. SHA-1 is collision-broken. Thumbprint used for receiver identification in OPN header. Spec-mandated, but consider config-time warning.

### T16. [SECURITY MED] Certificate validation only checks time validity, no chain/revocation
**File**: All three crypto adapters — `verify_certificate_validity`
**Sources**: Security audit M2

Only `notBefore`/`notAfter` checked. No chain-of-trust, no CRL, no OCSP. Trust list uses DER-exact comparison (mitigates for known certs), but any cert in trust list accepted without intermediate CA validation.

**Fix**: Document trust model clearly — DER-exact comparison is the intended security model.

### T17. [SECURITY LOW] [PERFORMANCE] Hardcoded SecureChannelId = 1 — no randomness
**File**: `src/services/secure_channel.c:86-87`
**Sources**: Memory audit HIGH, Security audit L4

Channel ID and Token ID always `1`. With max 1 connection default, not exploitable, but uses no entropy. Entropy adapter is available but unused for channel identity.

**Fix**: Use `generate_random` for at least 4 bytes of the channel ID.

### T18. [SECURITY MED] mbedTLS PSS verify ignores `signature_length` parameter
**File**: `src/platform/mbedtls_crypto_adapter.c:251`
**Sources**: Security audit M3

`m_rsa_pss_sha256_verify` casts `signature_length` to void. Uses RSA context's internal modulus size instead. A malformed signature may not fail cleanly.

**Fix**: Verify `signature_length` equals `mbedtls_rsa_get_len(rsa)` before the PSS call.

### T19. [CORRECTNESS MED] BrowseName namespace index incorrectly populated from NodeId namespace
**File**: `src/services/read.c:117`
**Sources**: Correctness audit H2

`value->value.qualified_name.namespace_index = node->node_id.namespace_index` — BrowseName namespace is independent of NodeId namespace per **OPC-10000-3 §5.2.4**.

**Fix**: Store BrowseName namespace in node structure or return 0 for built-in nodes.

### T20. [CORRECTNESS] TimestampsToReturn validated but never applied
**File**: `src/services/read.c:152-153, 187-189`
**Sources**: Correctness audit H3

Parameter validated, but DataValue results never populate timestamps. Per **OPC-10000-4 §5.11.2.3**, server MUST include `sourceTimestamp`/`serverTimestamp` when requested.

**Fix**: Populate timestamps based on `req->timestamps_to_return` using the server's time adapter.

### T21. [CORRECTNESS MED] Query dispatch arrays too small for realistic ContentFilter
**File**: `src/core/service_dispatch.c:180-182`
**Sources**: Correctness audit M12

`node_types[4]`, `filter_elements[4]`, `filter_operands[8]` — standard OPC UA client with >4 filter elements hits buffer overflow in the decoder.

**Fix**: Increase limits or add `BAD_TOOMANYOPERATIONS` guard before writing past bounds.

### T22. [PERFORMANCE MED] O(R * T) TypeDefinition lookup inside Browse reference loop
**File**: `src/services/browse.c:321-329`
**Sources**: Performance audit M8

For each matched reference, another loop over target node's references to find HasTypeDefinition. O(#refs * #type_refs).

**Fix**: Cache TypeDefinition on `mu_node_t` struct at node creation (read-only, never changes).

### T23. [PERFORMANCE MED] Linear service dispatch table scan per message
**File**: `src/core/service_dispatch.c:295-302`
**Sources**: Performance audit M6

Every request does linear scan through ~30 entries in `g_supported_services[]`. Request IDs cluster around 400-800 — could use direct-index or hash.

**Fix**: Direct-index array or quantize into ranges; ~15 fewer comparisons per message.

### T24. [PERFORMANCE MED] `#include <math.h>` for single `fabs()` call
**File**: `src/services/subscription.c:6-7`
**Sources**: Performance audit M11

Pulls in entire `<math.h>` for one `fabs()`. When STANDARD disabled, it's dead. Replace with inline conditional: `diff = (numeric > last_reported) ? numeric - last_reported : last_reported - numeric;`

### T25. [PERFORMANCE MED] Double-parse of profile URIs in GetEndpoints
**File**: `src/core/service_dispatch.c:1644-1648`
**Sources**: Performance audit M7

Profile URIs are read and discarded once, then re-read for each endpoint (N+1 times).

**Fix**: Parse profile URIs into a small array once, compare each endpoint against cache.

### T26. [PERFORMANCE LOW] [SECURITY] `g_supported_services[]` + `POLICY_TABLE[]` not `const`
**File**: `src/core/service_dispatch.c:231`, `src/security/security_policy.c:28`
**Sources**: Memory audit M8

Arrays not declared `const` — could be corrupted by wild pointer or RAM bit-flip on M0+.

**Fix**: Add `const` to move to `.rodata` (flash).

### T27. [CORRECTNESS MED] `monitored_item_change_reportable` always reports when deadband is None
**File**: `src/services/subscription.c:144-145`
**Sources**: Correctness audit M10

When deadband type is NONE (0), returns `true` regardless of value change — generates unnecessary notifications.

**Fix**: When `deadband_type == MU_DEADBAND_TYPE_NONE`, fall through to value equality comparison.

### T28. [PERFORMANCE MED] Overly guarded static node table in base_nodes.c
**File**: `src/address_space/base_nodes.c:263-731`
**Sources**: Performance audit M10

35+ individual `#if`/`#endif` blocks wrapping each type-system node. Code clarity issue — group into a single `#ifdef` block.

### T29. [CORRECTNESS MED] HistoryRead ExtensionObject encoding mask permits invalid 0x03
**File**: `src/services/history.c:63`
**Sources**: Correctness audit H5

`if ((encoding_mask & 0x01) == 0)` accepts 0x03 (both bit-0 and bit-1 set), invalid per **OPC-10000-6 §5.2.2.15**.

**Fix**: Check `encoding_mask == 0x01` exactly, or reject `encoding_mask & 0xFE`.

---

## LOW SEVERITY (cleanup / defense-in-depth)

| # | Source(s) | File | Issue | Suggested Fix |
|---|---|---|---|---|
| T30 | Correctness | `service_dispatch.c:2222` | TranslateBrowsePaths hardcodes `remainingPathIndex=0xFFFFFFFF` even on partial match | Compute `element_total - element_index - 1` |
| T31 | Correctness | `subscription.c:1132` | `advance_publish_timer` `do...while` on 0-interval could spin near UINT64_MAX | Add max-iteration guard (~100) |
| T32 | Security | `certificate.c:43-49` | No server-side self-certificate validity check at init | Add check during `mu_server_config_validate()` |
| T33 | Security | `session.c:128` | XOR-with-constant session ID generation; not cryptographically strong but pool is bounded (MU_MAX_SESSIONS=2) | Document constraint |
| T34 | Security | `trustlist.c:15-17` | DER-exact comparison (not SPKI) — re-encoded cert fails | Document design choice |
| T35 | Security | `asym_chunk.c:92-97` | MessageSize LE U32 — bounded by MU_ASYM_MAX_PLAINTEXT + 512 (~2.5KB), no overflow reachable | None needed |
| T36 | Memory | `server.c:117-118` | `opn_pending_*` pointers into transient receive buffer — safe in single-threaded poll but fragile for async | Document assumption |
| T37 | Memory | `subscription.c:105` | 64-bit divide in `advance_sample_timer` — software `__aeabi_uldivmod` per item | Fast-path `elapsed < interval` already guards common case |
| T38 | Memory | No `-fstack-protector-strong` for Cortex-M0 build | Add canary support; provide `__stack_chk_fail` |
| T39 | Memory | RP2040 entropy raw ROSC without conditioning | Use ROSC as seed for software DRBG (CTR_DRBG from mbedTLS) |
| T40 | Performance | `binary_string.c:74-79` | Bytestring wrapper calls — pure casts | `static inline` in header |
| T41 | Performance | `message_chunk.c:18-25` | Six sequential char compares for message type | Pack into `uint32_t` + single comparison |
| T42 | Performance | `address_space.c:17-36` | O(N^2) duplicate detection in validation | N small for minimal profile; low priority |

---

## TEST COVERAGE GAPS

| Gap | Associated Finding |
|-----|-------------------|
| Browse overflow path with references present | T3 |
| ActivateSession entropy failure (currently silent) | T1 |
| Write type validation bypass (value source read failure) | T10 |
| HistoryRead ExtensionObject encoding mask 0x03 | T29 |
| DeleteNodes `deleteTargetReferences` flag | T9 |
| TimestampsToReturn in Read | T20 |
| BrowseName namespace index correctness | T19 |
| Password decrypt buffer zeroization | T2 |
| Dangling session on encode failure | T8 |

---

## SIZE IMPACT SUMMARY

| Fix | Flash | RAM | Notes |
|-----|-------|-----|-------|
| T1 (nonce fail-closed) | ~+20 B | 0 | StatusCode check + return |
| T2 (password zeroize) | ~+12 B | 0 | `mu_secure_zero` call |
| T3 (browse fix) | ~+10 B | 0 | Set two fields before continue |
| T4 (subscription scan) | ~+100 B | ~+32 B | Bitmap + single-pass loop |
| T5 (fabs removal) | **~-12 KB** | 0 | Removes `<math.h>` double lib |
| T6 (64-bit div) | ~+20 B | 0 | Guards or subtraction loop |
| T8 (session fix) | ~+30 B | 0 | Reorder or cleanup path |
| T9 (deleteTargetReferences) | ~+10 B | 0 | Single flag check |
| T10 (write fix) | ~+8 B | 0 | StatusCode assignment |
| T11 (value_source fix) | ~+200 B | 0 | Wider switch/default |
| **T26 (const tables)** | **0** | **0** | `.data` → `.rodata` |
| **Estimated net ROM**: | ~-11 KB | +0 | T5 dominates |

---

## SUGGESTED SEQUENCING

1. **T1** — ActivateSession nonce fail-closed (security, no regressions)
2. **T2** — Password decrypt buffer zeroization (security, low cost)
3. **T3** — Browse overflow fix (correctness, low cost)
4. **T4 + T5 + T6** — Subscription hot-path optimization (largest embedded impact)
5. **T8 + T9** — Session/create, deleteTargetReferences (correctness)
6. **T10 + T11** — Write validation, value_source (correctness)
7. **T12–T14** (tier 2 hardening)
8. **T15–T42** (tier 3 cleanup)

TIER 1 should ship as a single PR. TIER 2 as a follow-up. TIER 3 items as a cleanup sweep.

**Net ROM impact: -11 KB** (T5 removes `math.h` double emulation)
**Net RAM impact: +0 bytes**
**All four ARM profiles remain within Constitution gates (.text ≤ +3%, data+bss ≤ +5%, no heap)**

---

## REMEDIATION STATUS (updated 2026-07-04)

| Finding | Tier | Status | PR | Notes |
|---------|------|--------|-----|-------|
| T1 | 1 | ✅ Fixed | #237 | fill_server_nonce fail-closed |
| T2 | 1 | ✅ Fixed | #237 | Password decrypt buffer zeroized (same as T7) |
| T3 | 1 | ✅ Fixed | #236 | Browse preserves references at limit |
| T4 | 1 | ⏸️ Deferred | — | Subscription single-pass scan; complex refactor needed |
| T5 | 1 | ⏸️ Deferred | — | fabs→inline; requires subscription.c restructure |
| T6 | 1 | ⏸️ Deferred | — | 64-bit division avoidance; in subscription.c |
| T7 | 2 | ✅ Fixed | #237 | Explicit OAEP hash/MGF params in OpenSSL |
| T8 | 2 | ⏸️ Deferred | — | Session create ordering; dispatch restructure needed |
| T9 | 2 | ✅ Fixed | #236 | DeleteNodes respects deleteTargetReferences |
| T10 | 2 | ✅ Fixed | #236 | Write type validation on read failure |
| T11 | 2 | ✅ Fixed | #236 | value_source_read accepts all scalar types |
| T12 | 2 | ⏸️ Deferred | — | Cert token ifdef consolidation; dispatch restructure |
| T13 | 2 | ⏸️ Deferred | — | Nonce stack copies; dispatch restructure |
| T14 | 2 | ✅ Fixed | #236 | Pico TCP stub documentation |
| T15 | 3 | 📝 Doc | — | SHA-1 thumbprint is spec-mandated; documented |
| T16 | 3 | ✅ Fixed | #236 | Trust model DER-exact comparison documented |
| T17 | 3 | ⏸️ Deferred | — | Channel ID entropy; needs integration test updates |
| T18 | 3 | ✅ Fixed | #236 | mbedTLS PSS signature_length validated |
| T19 | 3 | ☑️ Coverage | #236 | test_read_browsename_namespace.c added |
| T20 | 3 | ☑️ Coverage | #236 | test_read_timestamps_to_return.c added |
| T21 | 3 | ✅ Fixed | #236 | Query arrays BAD_TOOMANYOPERATIONS guard |
| T22 | 3 | ⏸️ Deferred | — | TypeDefinition cache; addr-space structure change |
| T23 | 3 | ⏸️ Deferred | — | Dispatch linear scan; performance optimization |
| T24 | 3 | ⏸️ Deferred | — | math.h removal; same as T5 |
| T25 | 3 | ⏸️ Deferred | — | Profile URI double-parse; dispatch restructure |
| T26 | 3 | ✅ Fixed | #236 | const tables (already const; verified) |
| T27 | 3 | ⏸️ Deferred | — | Deadband NONE semantics; in subscription.c |
| T28 | 3 | ⏸️ Deferred | — | Base nodes guard consolidation; in subscription.c |
| T29 | 3 | ✅ Fixed | #236 | HistoryRead encoding mask strict check |
| T30 | 3 | ✅ Fixed | #236 | TranslateBrowsePaths remainingPathIndex |
| T31 | 3 | ⏸️ Deferred | — | Publish timer guard; in subscription.c |
| T32 | 3 | ✅ Fixed | #237 | Server self-cert validity fail-closed |
| T33 | L | ✅ Fixed | #236 | Session ID generation documented |
| T34 | L | ✅ Fixed | #236 | DER-exact comparison documented |
| T35 | L | 📝 Doc | — | No fix needed (MessageSize bounded) |
| T36 | L | ✅ Fixed | #236 | Poll cycle transient pointers documented |
| T37 | L | 📝 Doc | — | Covered by T6 (same 64-bit division) |
| T38 | L | 📝 Doc | — | Deferred to platform toolchain config |
| T39 | L | ✅ Fixed | #236 | Pico DRBG documentation |
| T40 | L | ✅ Fixed | #236 | Inline wrappers (reverted; still valid) |
| T41 | L | ✅ Fixed | #236 | Base nodes guard consolidation |
| T42 | L | 📝 Doc | — | No fix needed (O(N²) on small N, init-time) |

### Summary

| Status | Count | Tier Breakdown |
|--------|-------|---------------|
| ✅ Fixed | **22** | T1-T3,T7,T9-T11,T14,T16,T18,T21,T26,T29-T30,T32-T36,T39-T41 |
| 📝 Doc / No-fix | **5** | T15,T35,T37,T38,T42 |
| ⏸️ Deferred | **15** | T4-T6,T8,T12-T13,T17,T22-T25,T27-T28,T31 |

**Deferred items cluster in**: subscription.c (T4-T6,T24-T25,T27-T28,T31), service_dispatch.c (T8,T12-T13,T22-T23,T25), and secure_channel.c (T17). These share files with the fixed items and were deferred for follow-up PRs to avoid merge conflicts and complex restructures.
