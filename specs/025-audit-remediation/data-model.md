# Phase 1 Data Model: Five-Lens Audit Remediation

This feature is protocol/firmware remediation, not a data-storage feature. The
"entities" are the C structs, tables, and config surfaces that change. Each entry
notes fields added/changed, invariants, and the finding it serves.

## 1. `mu_crypto_adapter_t` (include/muc_opcua/platform.h) — CHANGED

Add one optional function pointer:

```c
/* Returns MU_STATUS_GOOD iff the current time is within the certificate's
   [notBefore, notAfter] validity window. Optional: NULL means the backend
   cannot check validity, in which case secured policies MUST fail closed. */
opcua_statuscode_t (*verify_certificate_validity)(void *context,
        const opcua_byte_t *certificate, size_t length);
```

- **Invariant**: implemented by all three backends (mbedtls/wolfssl/host); may be
  `NULL` in a minimal custom adapter.
- **Consumers**: `mu_certificate_validate` (src/security/certificate.c).
- **Serves**: Finding 3 (cert validity). Fail-closed-when-NULL is the contract.

## 2. `mu_session_t` (src/services/session.h) — MINIMAL CHANGE / possibly none

Existing fields: `state`, `session_id`, `auth_token`,
`revised_session_timeout_ms`, `server_nonce[32]`, (`secure_channel_id` under
multi-conn).

- **Preferred design (Decision 1)**: **no new field** — ActivateSession verifies
  ClientSignature against the SecureChannel certificate, and CreateSession binds
  cert==OPN-cert, so the channel already holds the verification key for the
  session's lifetime.
- **Fallback (only if the channel cert is not reachable at activate time)**: add
  `opcua_byte_t client_cert_thumbprint[32]` (32 B × `MU_MAX_SESSIONS`, guarded by
  `#ifdef MUC_OPCUA_SECURITY`) to re-match the cert. Bounded, static, within gate.
- **Serves**: Findings 2, 5.

## 3. `secure_scratch` partition (src/core/server.c, server_internal.h, config.h) — CHANGED

Existing: `secure_scratch[MU_SECURE_SCRATCH_SIZE=12288]`, partitioned
`MU_SECURE_RESP_MAX=11264` + `MU_SECURE_OPN_REQ_MAX=1024`.

- **Change**: define a session-handshake sub-region (e.g. `MU_SECURE_SESSION_MAX`)
  used by CreateSession/ActivateSession for the relocated `to_sign`/`sig`/
  `verify_buf`/`decrypt_buf` data.
- **Invariant (guarded by `_Static_assert`)**: the session sub-region MUST NOT
  overlap any region live at the same time (specifically the response-building
  `respbody` region while a session response is encoded). Overlap analysis lands
  in the implementation; if the two are simultaneously live, either place the
  session region in an unused window or raise `MU_SECURE_SCRATCH_SIZE` by ≤2 KB
  (still within the +5% RAM gate for Embedded/full).
- **Serves**: Finding 4 (stack → scratch). RAM-neutral (reuses existing arena) or
  ≤+2 KB fallback; peak stack drops ~2 KB.

## 4. SecurityPolicy parameter table (src/security/security_policy.c) — NEW static table

Replace the ~6 parallel `switch (policy)` accessors
(`uri`, `signature_key_length`, `encryption_key_length`, `encryption_block_size`,
`signature_length`, `nonce_length`, `allows_username_password_tokens`) with:

```c
typedef struct {
    mu_security_policy_id_t id;
    const char *uri;
    opcua_uint16_t sig_key_len, enc_key_len, block_size, sig_len, nonce_len;
    opcua_byte_t allows_username;   /* 0/1 */
} mu_security_policy_params_t;
static const mu_security_policy_params_t POLICY_TABLE[] = { ... };
```

- **Invariant**: one row per policy id; accessors become an indexed lookup with
  the same return values as the prior switches (validated by existing tests).
- **Serves**: Finding 12 (architecture: table over parallel switches). Net-neutral
  or smaller `.text`.

## 5. `features.h` (include/muc_opcua/features.h) — NEW header, no runtime footprint

- Provides `#ifndef`/`#define` safe defaults for every `MUC_OPCUA_*` service and
  feature gate.
- Emits `#error` on illegal combinations (dependency matrix): e.g.
  `SUBSCRIPTIONS_STANDARD ⇒ SUBSCRIPTIONS`, `EVENTS ⇒ SUBSCRIPTIONS`,
  `EMBEDDED_PROFILE ⇒ SECURITY` (final matrix confirmed against the existing
  `#if` gates during implementation).
- Included at the top of `config.h`.
- **Serves**: Finding 9 (custom-profile guards).

## 6. `MU_SERVER_STORAGE_BYTES` (include/muc_opcua/config.h) — DEDUPLICATED

- Collapse the two near-identical `#ifdef MUC_OPCUA_SECURITY` on/off definitions
  into one, summing sub-total macros that already evaluate to 0 when their feature
  is off (`MU_SERVER_SECURITY_STORAGE_BYTES` already does this).
- **Invariant**: the computed total is identical to today for all four profiles
  (verified by `measure_size.sh` RAM figures unchanged).
- **Serves**: Finding 12 (config drift risk).

## 7. RP2040 adapter behavior (platform/pico/mu_pico_adapter.c) — CHANGED

- `pico_generate_random`: draw from the RP2040 ROSC-based TRNG (non-constant,
  non-zero output) instead of `memset(0)`.
- `pico_get_time` / `pico_get_tick_ms`: return `time_us_64()`-derived monotonic
  values instead of `0`.
- TCP hooks: either wired to a real (lwIP) transport or the example is restricted
  to SecurityPolicy None with a documented `PICO_STACK_SIZE`.
- **Invariant**: entropy output differs between successive calls and is non-zero;
  time is monotonic non-decreasing.
- **Serves**: Finding 1.

## 8. Decoder-defect state (src/encoding/binary_query.c, src/services/history.c) — HARDENED

- QueryFirst: unsupported filter operand MUST NOT leave `operands[j].type`
  uninitialized — reject (`Bad_NotSupported`) or zero-initialize before `continue`.
- HistoryUpdate: reject `length < 0` on the ExtensionObject length read with
  `Bad_DecodingError` (matching `binary_extension_object.c`'s existing convention).
- **Serves**: Finding 11.

## 9. Subscription numeric fields (src/services/subscription.h/.c) — LOGIC CHANGE, no struct change

- `deadband_value` / `last_reported_numeric` stay as-is on the struct; the
  *computation* branches on variant type to use int64 for integers.
- **Invariant**: identical report/suppress decisions vs the prior double impl on
  integer fixtures; Float/Double path unchanged.
- **Serves**: Finding 7.
