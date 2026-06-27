# Phase 0 Research — Optimization Audit Remediation

No `NEEDS CLARIFICATION` markers remained from the spec. This phase records the
design decision for each non-trivial finding cluster. Each decision states what was
chosen, why, and the alternatives rejected. Findings T-numbers reference
`docs/review/optimization-audit-2026-06-27.md`.

---

## D1 — Secure-path stack relocation (US1 / T1, FR-001)

**Decision**: Move the large OPN-path scratch arrays (`server.c` `respbody[5120]`,
`opn_buf[1024]`, and the asymmetric `plain`/`sign_buf`/`verify_buf` in
`asym_chunk.c`) out of the call-chain stack into a single `secure_scratch` region
owned by `struct mu_server`, guarded by `#ifdef MICRO_OPCUA_SECURITY`. Additionally
right-size `MU_ASYM_SCRATCH` from 4096 to the real RSA block size (2048-bit ⇒ 256 B).

**Rationale**: The server holds exactly one `mu_secure_channel` and processes one
chunk per `mu_server_poll` (verified in `server_internal.h` and the audit's
reentrancy review) — so a single shared static scratch is safe. A single-connection
server must provision worst-case stack regardless; trading ~6 KiB of peak stack for
fixed `.bss` is a net headroom win and removes a real overflow risk (no guard page
on M0+).

**Alternatives rejected**: (a) Encrypt directly into `send_buffer` with no scratch —
the cleanest end state but higher-risk first step; kept as a stretch goal.
(b) Caller-provided scratch via config — more API churn for no extra safety.

**Spec fidelity**: No wire change. OPC-10000-6 §6.7.2 framing unchanged.

---

## D2 — Per-channel cipher context (US3 / T2, FR-012)

**Decision**: Extend `mu_crypto_adapter_t` with an **optional, additive** cipher-
context lifecycle: `cipher_ctx_init(context, key, direction, void *ctx_storage)` /
`cipher_ctx_free`, plus AES encrypt/decrypt variants that accept the prepared
context. Per-channel context storage is a fixed-size opaque buffer
(`opcua_byte_t cipher_ctx[MU_CIPHER_CTX_SIZE]`) inside `mu_secure_channel_t`
(behind `MICRO_OPCUA_SECURITY`). If the adapter does not supply the context
functions, the codec falls back to the existing stateless `aes256_cbc_*` per-call
path (correctness identical, no caching).

**Rationale**: The stateless adapter re-runs the AES-256 key schedule on every MSG
chunk (and the host adapter `EVP_*_CTX_new/free` per op) — the dominant avoidable
per-message cost on a no-FPU M0+. Caching the schedule once per channel (at OPN, the
same place keys are derived — `service_dispatch.c:188/191`) eliminates it. Keeping it
optional preserves the Constitution's pluggable-crypto principle and backward
compatibility for existing adapters.

**Alternatives rejected**: (a) Make the adapter stateful internally keyed by pointer
— hidden global state, not no-heap-friendly. (b) Mandatory interface change —
breaks existing adapters; violates additive-compatibility goal.

**Spec fidelity**: Pure performance; ciphertext identical (SC-003).

---

## D3 — Bounded address-space index (US3 / T3, FR-009)

**Decision**: Build a static index of node indices sorted by a NodeId sort key at
`mu_address_space_validate`/server-init time, sized to a compile-time cap
`MU_MAX_ADDRESS_SPACE_NODES`. `mu_address_space_find_node` binary-searches the index
(O(log N)). Sort key: `(namespace_index, identifier_type, numeric|string-hash)`;
ties and string NodeIds resolve with a full `mu_nodeid_equal` confirm. If
`node_count > MU_MAX_ADDRESS_SPACE_NODES`, **fall back to the existing linear scan**
(correctness preserved, just unindexed).

**Rationale**: `mu_address_space_t` is a caller-provided **const** array (cannot be
reordered in place), and N is integrator-controlled — the audited O(M·N) bottleneck
under Read/Browse/Translate/sampling (3 reviewers). A separate index array respects
const-ness and the no-heap rule; the cap bounds RAM (`≤ N×2 B`); the fallback keeps
arbitrarily large address spaces correct.

**Alternatives rejected**: (a) Require caller-pre-sorted nodes — caller owns order,
fragile. (b) Numeric-only hash table — doesn't cover string NodeIds uniformly.
(c) Leave linear — the flagged bottleneck.

**Spec fidelity**: Lookup result identical; no wire change.

---

## D4 — ActivateSession field consumption (US1 / T22, FR-002)

**Decision**: In `handle_activate_session`, after reading `cert_count`, **consume
each `SignedSoftwareCertificate` entry** with bounded decoders (then ignore them —
the field is "Reserved for future use"); validate and **skip `token_body_len`** of
the UserIdentityToken ExtensionObject and consume the trailing `UserTokenSignature`,
so every subsequent field decodes from the correct offset. Guard
`token_type.identifier.numeric` on `identifier_type == MU_NODEID_NUMERIC` (FR-004).

**Rationale**: OPC-10000-4 §5.7.3.2 defines `clientSoftwareCertificates []`,
`localeIds []`, and `userIdentityToken` as real fields. The current "thin path
assumes empty" desyncs the decoder on any non-empty cert array or non-trivial token
(Codex-verified). Consuming-and-ignoring matches "Reserved for future use" — the
spec does not mandate rejection.

**Alternatives rejected**: Reject non-zero `cert_count` with a StatusCode — stricter
than the spec requires for a reserved field; could break conformant clients that
send software certificates.

**Spec fidelity**: OPC-10000-4 §5.7.3.2 (authoritative). New unit + fuzz vectors.

---

## D5 — OpenSecureChannel policy/mode handling (US1 / FR-005)

**Decision**: Validate the decoded `securityPolicyUri`/`securityMode` against the
server's active capability: pass the decoded policy into
`mu_secure_channel_open` (currently called with `NULL`) and reject an inconsistent
combination (e.g. a non-None policy/mode when no crypto adapter is configured) with
the StatusCode indicated by OPC-10000-4 §5.6.2.2 / OPC-10000-6 SecureChannel rules,
rather than silently accepting it.

**Rationale**: §5.6.2.2: "If the securityPolicyUri is None, the Server shall
ignore..." — implies the server must act on the policy, not discard it. Current code
parses and drops it. **Open item for /speckit-tasks**: confirm the exact rejection
StatusCode (candidate `Bad_SecurityPolicyRejected` / `Bad_SecurityModeRejected`) via
the OPC UA reference before implementing; behaviour for the existing None-only build
must remain working.

**Alternatives rejected**: Leave as-is — silent acceptance is a spec-fidelity and
security-honesty gap (Constitution V).

**Spec fidelity**: OPC-10000-4 §5.6.2.2. Confirm StatusCode in tasks phase.

---

## D6 — Secret zeroization (US2 / T5, FR-007/008)

**Decision**: Add a portable `mu_secure_zero(void*, size_t)` using `volatile`
byte stores (not removable by the optimizer). Call it on derived
`client_keys`/`server_keys` in `mu_secure_channel_init`/`_close`, and on stack
secret intermediates (KDF `material`/`a`/`block`, decrypted `plain`, MACs,
signatures, nonces) before every return in `sym_chunk.c`, `asym_chunk.c`,
`key_derivation.c`.

**Rationale**: No MMU on target; stale keys/plaintext persist across channel reuse
(`server.c` reuses the struct). Volatile-store zeroization is the portable C idiom;
no dependency on `memset_s`/`explicit_bzero` (not freestanding-guaranteed).

**Alternatives rejected**: Rely on `memset` — legal for the compiler to elide on a
soon-dead buffer.

**Spec fidelity**: No wire change; strengthens Constitution V security honesty.

---

## D7 — Single-pass Browse (US3 / T8, FR-011)

**Decision**: Rewrite `mu_browse_process` to resolve each node's references in one
pass into the reference pool, committing the node's `num_references`/`refs_used`
only if the whole node fits (backpatch the count), preserving the existing
all-or-nothing-per-node rejection. The encoded length is already emitted separately
(`browse.c:118-141`), so the prior count pass is removable.

**Rationale**: Two passes both call O(N) `mu_resolve_node` per target ⇒ 2·R·N
(also halved by D3's index). All-or-nothing semantics must be preserved.

**Alternatives rejected**: Cache target pointers between passes — more state than a
single pass needs.

**Spec fidelity**: OPC-10000-4 §5.9.2; SC-003 byte-identical output.

---

## D8 — Sticky-status codec + overflow-safe bounds (US4 / T6, T19; FR-003, FR-015)

**Decision**: Add a sticky `status` field to `mu_binary_reader_t`/`mu_binary_writer_t`
(in `encoding.h`); primitives no-op once tripped; handlers check once at the end.
Replace `position + count > length` bound checks with overflow-safe
`count > length - position`. Consolidate hand-rolled little-endian pack/unpack into
one inline helper and collapse signed/unsigned writer twins.

**Rationale**: ~294 per-call `if (s != GOOD) return s;` sites (~6 B each on Thumb-1)
dominate `service_dispatch.c` flash; sticky status removes them while keeping bounds
checks intact (`ensure_space`/`ensure_bytes` still run). Overflow-safe form closes
the latent foot-gun (T19).

**Alternatives rejected**: Macro-wrap the check — smaller change but keeps the branch
count; less flash benefit.

**Spec fidelity**: Encoding rules OPC-10000-6 §5.2 unchanged; SC-003 byte-identical.

---

## D9 — Unify dispatch + dedup handlers + size cleanups (US4 / T7, T9, T14, T15, T16; FR-015/016/018)

**Decision**: Add a `handler` function-pointer column to `g_supported_services[]`
(handlers already share one signature) and delete the parallel `switch`, keeping the
per-service `#ifdef` rows so `--gc-sections` still drops disabled services. Factor
the four per-id array handlers into one driver + per-item callback. Replace
`element_size` switch with a `static const` table. Gate `mu_status_name` behind
`MICRO_OPCUA_STATUS_STRINGS` (default off for embedded). Delete the dead, stale
`g_unsupported_services[]`/`mu_is_unsupported_service`. Wrap any service-specific
static helpers (e.g. Browse/Translate helpers) in their service `#ifdef` so
warnings-as-errors builds pass for every permutation (T17).

**Rationale**: Pure flash/maintainability wins, no behaviour change. The dispatch
unify also removes a "forgot the switch arm" bug class.

**Alternatives rejected**: Split handlers into per-service translation units — larger
diff; deferred (the audit notes it as a follow-on, not required here).

**Spec fidelity**: No wire change; full suite + SC-003 guard.

---

## D10 — Opt-in LTO + timer/memmove fixes (US3/US4 / T10, T11, T12; FR-013, FR-017)

**Decision**: Add `MICRO_OPCUA_LTO` CMake option using `INTERPROCEDURAL_OPTIMIZATION`
(default OFF for distributed `.a`, ON for firmware-from-source). Add a fast-path to
`advance_sample_timer` (`elapsed < interval` ⇒ single add, skip 64-bit divide). Use a
read cursor in `server.c` reassembly and compact once, avoiding per-message
whole-remainder `memmove`.

**Rationale**: LTO inlines the many tiny encoder leaf fns (several % flash); the
timer divide is software-emulated on M0+ (no HW divide); the per-message memmove is
O(N²) under pipelined reads.

**Alternatives rejected**: LTO on by default — toolchain coupling for distributed
archives.

**Spec fidelity**: No wire change.

---

## Open items carried to /speckit-tasks

1. **StatusCode confirmations (T016)**: confirm via the OPC UA reference the exact
   StatusCode + section for the three malformed/limit decisions before coding —
   (D5) rejected SecurityPolicy/mode; (FR-004) NodeId type confusion
   (`Bad_DecodingError` / `Bad_IdentityTokenInvalid`, OPC-10000-6 §5.2.2.9 +
   OPC-10000-4 §7.38.2); (FR-006) op-cap exceeded (`Bad_TooManyOperations`,
   OPC-10000-4 §7.38.2).
2. **D3**: choose the final `MU_MAX_ADDRESS_SPACE_NODES` default and string-NodeId
   sort-key hash (must be deterministic and collision-safe with `mu_nodeid_equal`
   confirm).
3. **D2**: choose `MU_CIPHER_CTX_SIZE` large enough for the supported backends'
   AES-256 key schedule (host OpenSSL vs MCU soft-AES) with a compile-time assert.
4. Confirm the CI stack-measurement mechanism (`-fstack-usage`/`-Wstack-usage`) and
   the exact ≤10 KiB threshold (SC-001).
