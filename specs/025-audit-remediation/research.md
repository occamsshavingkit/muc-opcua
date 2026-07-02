# Phase 0 Research: Five-Lens Audit Remediation

The spec had no `[NEEDS CLARIFICATION]` markers; the open questions are all
*design* decisions about how to remediate each finding within the constitution's
constraints (heap-free, static memory, size discipline, test-first, spec fidelity).
Each decision below records what was chosen, why, and the alternatives rejected.

---

## Decision 1 — Verify ClientSignature against the SecureChannel certificate, not a per-session copy

**Decision**: In `handle_activate_session`, verify the decoded `ClientSignature`
over `serverCert ‖ serverNonce` using `crypto->rsa_sha256_verify(...)` with the
**certificate that opened the SecureChannel** as the public-key source. Do not
store a copy of the CreateSession certificate on `mu_session_t`.

**Rationale**: `mu_crypto_adapter_t.rsa_sha256_verify` already takes
`(certificate, certificate_length, data, data_length, signature, signature_length)`
— it extracts the public key from a DER certificate. The channel already retains
the client certificate (used for the OPN signature check). Because Decision 2
binds the CreateSession certificate to equal the OPN certificate, the channel cert
*is* the correct verification key. This keeps RAM flat on the `MU_MAX_SESSIONS`
(=2) pool — storing a full DER cert (~1–1.5 KB) per session would violate Size
Discipline for no benefit. `serverNonce` is already on `mu_session_t`
(`server_nonce[32]`), and `serverCert` is available via
`crypto->get_own_certificate`.

**Alternatives considered**:
- *Store full client DER cert on the session*: rejected — up to ~3 KB static RAM
  for 2 sessions, no functional gain once cert==OPN-cert is enforced.
- *Store only the 32-byte thumbprint and re-fetch cert*: unnecessary; the channel
  already holds the cert bytes for the channel lifetime, which spans the session.

**Spec cite**: OPC-10000-4 §7.38.2 (ActivateSession ClientSignature),
§5.6.3/§5.7.3 (SignatureData over serverCert‖serverNonce).

---

## Decision 2 — Bind CreateSession certificate to the OPN certificate; make trust fail-closed

**Decision**: In `handle_create_session`, require that the presented
ClientCertificate byte-equals the certificate that opened the current
SecureChannel; reject with `Bad_SecurityChecksFailed` on mismatch. In
`mu_certificate_validate` and the OPN path, make trust-list matching **mandatory**
for any non-None policy: if `config.trust_list == NULL`, reject the secured
connection rather than accepting it.

**Rationale**: Closes findings 5 and 3's fail-open half. Binding cert==OPN-cert is
also what licenses Decision 1. Fail-closed trust is the Security-Honesty-compliant
default (Constitution §V: security MUST NOT be silently weakened).

**Alternatives considered**:
- *Keep trust optional but warn*: rejected — a silent accept is exactly the
  finding; a warning on an MCU is invisible.
- *Validate CreateSession cert against trust list independently of the channel
  cert*: redundant once they must be equal; the single equality + the channel's
  existing trust check covers it.

**Spec cite**: OPC-10000-4 §5.6.3 (CreateSession certificate handling), §5.5
(SecureChannel certificate/trust), OPC-10000-6 §6.7.4.

---

## Decision 3 — New optional crypto-adapter hook `verify_certificate_validity`, fail-closed when absent

**Decision**: Add one function pointer to `mu_crypto_adapter_t`:
`opcua_statuscode_t (*verify_certificate_validity)(void *context, const opcua_byte_t *certificate, size_t length)`
returning `Good` if the current time is within `[notBefore, notAfter]`, else a
certificate-time StatusCode. Call it from `mu_certificate_validate` for non-None
policies. If the pointer is `NULL`, the secured path treats validity as
**unverifiable → reject** (fail-closed).

**Rationale**: The three backends (mbedTLS `mbedtls_x509_crt`, wolfSSL, OpenSSL)
all expose notBefore/notAfter parsing; the host adapter already parses these in
its cert *generator*. A dedicated hook keeps platform-specific X.509 date logic in
the adapter (Constitution §II — no platform leakage into core). Fail-closed on a
missing hook preserves Security Honesty rather than silently skipping.

**Alternatives considered**:
- *Parse X.509 dates in the portable core*: rejected — pulls ASN.1/time parsing
  into freestanding C11 core, a boundary violation and size cost.
- *Reuse `get_certificate_key_bits` to also return dates*: rejected — overloads a
  single-purpose hook and complicates all three backends' signatures.
- *Fail-open when hook absent*: rejected — reintroduces the finding.

**Spec cite**: OPC-10000-4 §5.5 (certificate validation), StatusCode
`Bad_CertificateTimeInvalid` (OPC-10000-4 Table of certificate StatusCodes).

---

## Decision 4 — Relocate handshake buffers into a carved sub-region of `secure_scratch`

**Decision**: Move `to_sign[1536]`/`sig[512]` (CreateSession) and
`verify_buf[1536]`/`decrypt_buf[256]` (ActivateSession) out of their stack frames
into named sub-regions of the existing `server->secure_scratch[12288]`, following
the same partitioning pattern already used for the OPN path
(`MU_SECURE_RESP_MAX` / `MU_SECURE_OPN_REQ_MAX`). Add `_Static_assert`s proving
the session sub-region does not overlap the response-building region that is live
at the same time.

**Rationale**: `secure_scratch` is already-allocated static RAM (Size Discipline
neutral) and this *reduces* peak stack by ~2 KB on the attacker-reachable path —
directly fixing the RP2040 2 KB-stack HardFault (finding 4). The OPN path proves
the pattern works. Overlap analysis is the one real risk and is guarded by
`_Static_assert` (matching `server.c:169`).

**Open item resolved in design**: confirm the CreateSession/ActivateSession scratch
usage does not overlap `respbody = secure_scratch[0..11264]` while a response is
being built. If it would, carve the session buffers from the tail region or bump
`MU_SECURE_SCRATCH_SIZE` (RAM +≤2 KB, still within the +5% gate for Embedded/full
whose base RAM is ~6 KB) — recorded in data-model.md as the fallback.

**Alternatives considered**:
- *Raise `PICO_STACK_SIZE` only, leave buffers on stack*: rejected as the primary
  fix — the buffers still spike stack for every secured handshake; documenting
  `PICO_STACK_SIZE` is done *in addition* (FR-008), not instead.
- *Shrink the buffers*: rejected — 1536 is sized to the max cert+nonce; shrinking
  risks truncating valid inputs.

**Spec cite**: OPC-10000-6 §6.7 (SecureChannel framing sizes); no wire change.

---

## Decision 5 — Address-space cap: build-time assertion as primary, indexed path as stretch

**Decision**: Make exceeding `MU_MAX_ADDRESS_SPACE_NODES` a **loud failure** rather
than a silent linear fallback. Primary: when the integrator's static node_count
exceeds the cap, fail at registration with a clear StatusCode
(`Bad_ConfigurationError` / a compile-time `_Static_assert` where the count is a
constant). Also fix the Query correctness bug so a *supported* (≤cap) space is
never empty due to the fallback flag.

**Rationale**: The finding is a *silent* cliff (perf + empty Query). The
constitution favors predictable failure over ambiguous degradation (§VII: "small
firmware that fails ambiguously is more expensive"). A loud cap is the minimal,
correct fix and is size-free. Raising the cap or adding a general binary-search
path over arbitrary user spaces is a larger change with its own sorting-invariant
risk (the complexity audit flagged the "sorted" precondition) and is deferred to a
follow-up unless it falls out cheaply.

**Alternatives considered**:
- *Silently raise the cap*: rejected — just moves the cliff.
- *General O(log n) binary search over user spaces now*: deferred — needs a
  validated "sorted by NodeId sort key" invariant per Decision, more test surface;
  out of proportion to a medium finding. Recorded as a stretch task.

**Spec cite**: OPC-10000-4 §5.9 (Query), §5.11 (Read/Browse node resolution).

---

## Decision 6 — Integer deadband via int64 for integer variant types; double only for real Float/Double

**Decision**: In `monitored_item_change_reportable` and the aggregate min/max/sum
paths, branch on the variant type: for the integer built-ins (SByte…UInt64) compute
`|new − last| >= deadband` and aggregates in `int64_t` (with overflow-safe
subtraction), reserving `double` for genuine `Float`/`Double` variants. Keep the
existing `double` results bit-for-bit for the float path.

**Rationale**: Removes `__aeabi_d*` soft-float from the common integer-sensor path
on the no-FPU core (finding 7), and aligns with the project's own stated
integer-only policy (quoted in `session.c`). Equivalence on integer fixtures is the
correctness gate. Overflow safety: compute the absolute difference using unsigned
width or a widened compare so UInt64 near range limits does not wrap.

**Alternatives considered**:
- *Fixed-point for all types*: rejected — needlessly changes Float/Double behavior
  and risks conformance drift on §7.22 semantics.
- *Leave as double, just gate out of Nano/Micro*: already gated out; the cost is
  on Embedded/full which the finding targets — must actually remove the soft-float.

**Spec cite**: OPC-10000-4 §7.22.2 (DataChangeFilter absolute deadband).

---

## Decision 7 — Single `features.h` with defaults + `#error` dependency guards, included first

**Decision**: Add `include/muc_opcua/features.h` that (a) provides a safe default
`#define` for every `MUC_OPCUA_*` service/feature gate when not set by CMake, and
(b) `#error`s on illegal combinations (`SUBSCRIPTIONS_STANDARD` without
`SUBSCRIPTIONS`, `EVENTS` without `SUBSCRIPTIONS`, `EMBEDDED_PROFILE` without
`SECURITY`, etc.). It is included at the top of `config.h` (already included
widely) so every TU sees the guards.

**Rationale**: Turns the architecture finding's "silent half-wired feature" into a
loud compile error (finding 9), and gives non-CMake `-D` consumers header-level
defaults. No runtime footprint. Including via `config.h` avoids touching every
source file.

**Alternatives considered**:
- *Put guards in CMake only*: rejected — a direct `-D` consumer bypasses CMake and
  the audit explicitly flagged that path.
- *Scatter `#if !defined` guards per file*: rejected — that is the current
  implicit-spaghetti state the finding calls out.

**Spec cite**: N/A (build configuration; Constitution §VI Toolchain Discipline).

---

## Decision 8 — Behavior-preserving extraction; table-driven SecurityPolicy; validated by regression fixtures

**Decision**: Story 8 moves `handle_create_session` / `handle_activate_session` /
`handle_open_secure_channel` bodies into `session.c` / `secure_channel.c`, and the
monitored-item/filter decoders into `subscription.c` (or a new
`binary_monitoring.c` encoder unit), leaving `service_dispatch.c` as routing +
guards. Story 12 replaces the ~6 parallel `switch (policy)` accessors in
`security_policy.c` with one `static const` parameter table. Both are validated
byte-for-byte against the existing `test_response_regression` / interop fixtures;
no wire behavior changes.

**Rationale**: Directly addresses architecture findings 1 and 8 without changing
behavior. The `--gc-sections` per-service model already in use means extra TUs do
not add flash. The regression fixtures are the safety net that lets a
reorganization ship with confidence (Constitution IV test-first — here the tests
already exist and must stay green).

**Alternatives considered**:
- *Leave the god-file as-is*: rejected — the finding is a real maintainability tax
  and the extraction is low-risk with existing regression coverage.
- *Redesign dispatch entirely*: rejected — out of scope; this is remediation, not
  redesign (spec Assumptions).

**Spec cite**: no wire change (OPC-004); refactor only.

---

## Cross-cutting: test-first order

Per Constitution IV, for every wire-visible or security change the RED fixture is
written and failing before the fix:
- ActivateSession bad/replayed/good signature fixtures (integration).
- Expired / untrusted / no-trust-list certificate cases (unit + integration).
- Username token without encryption still nonce-checked (integration).
- Negative HistoryUpdate length → `Bad_DecodingError` (unit).
- QueryFirst unsupported operand leaves no uninitialized type (unit).
- Integer deadband equivalence vs the prior double results (unit, golden).
- Illegal custom-profile combination fails to compile (build test).

Refactors (Stories 8/12) rely on the *existing* regression suite staying green,
plus a size/speed compare, rather than new RED tests.
