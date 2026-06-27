# micro-opcua — Consolidated Optimization Audit (2026-06-27)

Seven independent reviewers: 6 skill-driven subagents (memory-safety, embedded-memory-budget,
arm-cortex code-size, code-dedup/complexity, algorithm-opt, systems-perf) + Google Antigravity.
Findings below are ranked by **cross-source agreement × impact**. Each row notes how many
independent reviewers raised it (confidence).

## Verified POSITIVES (do not "fix")
- Decoder bounds discipline is uniform: every primitive read passes `ensure_bytes()`; wire
  lengths validated before pointer math/memcpy. **No reachable spatial over-read/write found.**
- No unaligned memory access anywhere — all (de)serialization is byte-at-a-time + shifts (M0+ safe).
- No soft-float in hot path — float/double kept as raw IEEE bits, integer-compared.
- Symmetric keys derived **once per channel** and cached; MSG decrypt is **in-place** (no scratch copy).
- Heap usage = 0, no mutable globals (.bss/.data ≈ 0). `--gc-sections` reaches firmware. Good baseline.

## ADDED BY CODEX (verified by hand)

### T22. ActivateSession does not consume ClientSoftwareCertificates / identity-token body  [Codex, VERIFIED REAL — unique]
`service_dispatch.c:385-399` reads `cert_count` (`:386`) but never loops/skips the array
("thin path assumes empty"), and reads the UserIdentityToken ExtensionObject header (`:399`)
without skipping `token_body_len` or consuming the `UserTokenSignature`. A client sending a
non-empty ClientSoftwareCertificates array (or a non-trivial identity token) **desyncs the
decoder** for every field after it. **Fix:** reject non-zero `cert_count` or skip each entry with
bounded decoders; validate `token_body_len <= remaining` and consume the token body + signature.
Also guard `token_type.identifier.numeric` (`:411`) on `identifier_type == MU_NODEID_NUMERIC`.

### Codex notes
- **OPN SecurityPolicyUri/security_mode not validated** when crypto inactive (`server.c:253`,
  `service_dispatch.c:157`) — plausible, **verify** (may be partly by-design for the no-crypto build).
- **FALSE POSITIVE:** variant array "unaligned typed-pointer deref" — real encode at
  `binary_variant.c:164-170` is byte-at-a-time; no fault. Codex cited `:538/621` (file is 175 lines).
- **Reliability:** ~half of Codex's `file:line` citations point past EOF; trust findings, not its lines.

## CONFLICTS RESOLVED
- **copy_monitored_node_id OOB (Antigravity HIGH) → FALSE POSITIVE.** Code at
  `service_dispatch.c:644` already sets `string.length = length` after the clamp. Verified by hand.
- **Integer overflow in `position + count` bound check:** Antigravity rated HIGH/exploitable;
  memory-safety agent rated LOW (position always small/validated → latent, not reachable today).
  Resolution: **latent foot-gun, cheap fix, low urgency.**

---

## TIER 1 — Highest leverage (multi-source, real risk)

### T1. ~12.8 KB nested stack on the secured OpenSecureChannel path  [4 sources: mem-safety, mem-budget, systems-perf, AGY]
`server.c:331-332` (`respbody[5120]` + `opn_buf[1024]`) stays live across `mu_asym_chunk_wrap`
which adds `plain[2048]` + `sign_buf[4096]` + `sig[512]` (`asym_chunk.c:136,147,151`), and the
inbound `mu_asym_chunk_unwrap` adds `plain[2048]` + `verify_buf[4096]` (`asym_chunk.c:243,260`).
Peak ≈ 12.8 KB in one pre-auth call chain — real stack-overflow risk on a Cortex-M0+ (few-KB stack,
no guard page). **Fix:** single-threaded/single-connection server → move these scratch buffers to
static `mu_server` storage (or encrypt directly in `send_buffer`). Confirmed safe: one chunk per
`mu_server_poll`, sessions multiplexed on one channel (no reentrancy).
Bonus (mem-budget M2): `MU_ASYM_SCRATCH=4096` is oversized — 2048-bit RSA = 256-byte blocks;
size to real key or union with `plain[2048]`. Together: required stack ~16 KB → ~9-10 KB.

### T2. Per-message AES-256 key schedule + per-op crypto context alloc  [systems-perf HIGH]
`platform.h:101-106` crypto adapter is stateless: `sym_chunk.c:108,143` passes raw key+IV every call,
so soft-AES re-runs the **AES-256 key schedule on every MSG chunk**; `host_crypto_adapter.c:38-52`
does `EVP_*_CTX_new/free()` per op (pattern leaks to MCU ports). **Fix:** opaque per-channel cipher
context; build key schedule once at OPN, reuse per MSG. Dominant avoidable per-message cost.

### T3. Address space lookup is O(N), making Read/Browse/sampling O(M·N)  [2 sources: algorithm HIGH, AGY]
`node_id.c:47-61` `mu_address_space_find_node` is a linear scan — the primitive under every
Read (`read.c:151-160`, M≤32), Browse, Translate, and the recurring sampling tick. **Fix:** build a
sorted index (or numeric-id table) once at init → binary search O(log N). No heap needed.

### T4. Cache the resolved node pointer in each MonitoredItem at create  [algorithm HIGH]
`subscription.c:55-69,826-849` re-resolves each item's node every sampling round → O(items·N) on a
**recurring timer path**. The node never changes. **Fix:** store `const mu_node_t *` at create →
lookup cost drops to zero. Cheapest win on the recurring path.

---

## TIER 2 — Strong (size + correctness)

### T5. Secret material not zeroized  [2 sources: mem-safety MED, AGY MED]
KDF/crypto intermediates left on stack (`sym_chunk.c:17-24`, `key_derivation.c:25-49`); derived
`client_keys`/`server_keys` not cleared on channel close/reuse (`secure_channel.c:6-21,60-66`;
struct reused at `server.c:471`). **Fix:** `secure_zero()` (volatile loop) on temporaries before
return and on keys in init/close.

### T6. Sticky-error anti-pattern bloats flash  [code-size HIGH ~1.5-2.5 KiB]
~294 `if (s != GOOD) return s;` sites after each encode/decode (217 in `service_dispatch.c` alone),
~6 B each on Thumb-1. **Fix:** add a sticky `status` to `mu_binary_writer/reader_t`; primitives
no-op once tripped; check once per handler. Est. ~7-10% of the 24 KB Micro core. Bounds checks
unaffected (`ensure_space` still runs).

### T7. Dual dispatch: parallel table + switch  [2 sources: code-size MED, dedup HIGH]
`service_dispatch.c:34-68` (`g_supported_services[]`) and `:1702-1758` (switch) encode the service
list twice behind matching `#ifdef`s. **Fix:** add a `handler` fn-pointer column (handlers already
share one signature), delete the switch. ~200-400 B + removes a "forgot the switch arm" bug class;
keep `#ifdef` rows so gc-sections still drops disabled services.

### T8. Browse scans each node's references twice  [3 sources: algorithm HIGH, dedup MED, AGY HIGH]
`browse.c:215-286` — count pass + write pass, both calling O(N) `mu_resolve_node` per target → 2·R·N.
The encoded length is emitted separately (`browse.c:118-141`), so the count pass is unnecessary.
**Fix:** single pass into the pool; commit counts only if the node fits (preserve all-or-nothing).

### T9. Four per-id array handlers are structural clones  [dedup HIGH ~90-110 lines]
`service_dispatch.c:801-848,1050-1105,1107-1156,1158-1198` (set_publishing/set_monitoring/
delete_monitored_items/delete_subscriptions) share the header→count→loop→trailer skeleton.
**Fix:** one driver + ~6-line per-item callback. Largest raw line reduction; no behavior change.

### T10. Enable opt-in LTO  [2 sources: code-size MED, AGY MED]
`-Os` + sections present, but no LTO. The encoder is dozens of tiny leaf fns called hundreds of
times — LTO inlines them. **Fix:** `MICRO_OPCUA_LTO` → `-flto`; default OFF for distributed `.a`,
ON for firmware-from-source. Est. several %–20%.

---

## TIER 3 — Worthwhile

| # | Source(s) | File:line | Issue | Fix |
|---|---|---|---|---|
| T11 | AGY MED/HIGH | `subscription.c:97-98` | 64-bit `/` in `advance_sample_timer` → soft `__aeabi_uldivmod` per item (no HW divide) | Fast-path `elapsed < interval` (common case) before dividing |
| T12 | systems-perf MED | `server.c:562-566` | Per-message full-remainder `memmove` → O(N²) under pipelined reads | Parse cursor; compact lazily |
| T13 | dedup HIGH | `binary_writer.c:40-102`, `sym/asym_chunk.c`, `subscription.c:244`, `server.c` | LE u16/u32/u64 pack/unpack hand-rolled in 6 files | One inline LE header; collapse signed/unsigned twins into `write_le(v,n)` |
| T14 | 3 sources | `binary_variant.c:121-144` | `element_size` = 18-case switch | `static const uint8_t` size table (~20 B rodata) |
| T15 | 2 sources | `status.c:5-58` (`mu_status_name`) | ~50-case enum→string, only caller is host example | Gate behind `MICRO_OPCUA_STATUS_STRINGS`/`_DEBUG` (~1-1.5 KB MCU flash) |
| T16 | 2 sources | `status.c:55-80` | `g_unsupported_services[]`+`mu_is_unsupported_service` — **no caller, and stale** (lists BrowseNext/Translate/RegisterNodes as unsupported though implemented) | Delete table+fn+decl |
| T17 | AGY MED | browse helpers `service_dispatch.c:1390-1530` | `browse_name_equals`/`handle_translate_browse_paths` may be unguarded by `#ifdef` → `-Werror` build break when BROWSE=OFF | Verify + wrap helpers in the service flag |
| T18 | systems-perf MED | `asym_chunk.c:107,227-237`, `host_crypto_adapter.c:153-167` | Own-cert thumbprint (constant) re-hashed; peer cert re-parsed per OPN | Cache own thumbprint/key-bits at init; parse peer cert once |
| T19 | mem-safety LOW | `binary_reader.c:15`, `binary_writer.c:15` | `position+count > length` not overflow-safe (latent) | `count > length - position` |
| T20 | mem-safety LOW | `service_dispatch.c:404-411` | `handle_activate_session` reads token `numeric` without checking `identifier_type` | Guard on `MU_NODEID_NUMERIC` |
| T21 | mem-safety LOW | `subscription.c:326,838-847` | MonitoredItem caches value string by shallow pointer → stale/dangling at publish if source is transient | Deep-copy bounded value strings or document callback lifetime |

## Suggested sequencing
1. **T1 + T2** (stack relocation + per-channel cipher ctx) — biggest safety + perf wins, both in the secure path.
2. **T3 + T4** (address-space index + MonitoredItem node cache) — one structural change fixes Read/Browse/Translate/sampling.
3. **T6 + T7 + T10** (sticky-error + table dispatch + LTO) — combined ≈10-15%+ flash on the Micro core.
4. **T5** zeroization, then Tier-3 cleanups (T16 dead code & T15 status strings are free wins).
