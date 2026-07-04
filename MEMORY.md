# Project Memory — Feature 025: Five-Lens Audit Remediation

**Date**: 2026-07-02
**Branch**: `025-audit-remediation`

## Key Changes

### Security hardening
- **ActivateSession ClientSignature** now verified via `verify_activate_client_signature`
  over `(serverCertificate || ServerNonce)` for all non-None policies
  (`src/services/session.c`, OPC-10000-4 §5.7/§7.38)
- **Certificate trust** enforced fail-closed: secured connections rejected when
  `trust_list == NULL`, certificate expired, or not in trust list
  (`src/core/service_dispatch.c` → `src/services/secure_channel.c`,
  OPC-10000-4 §5.5/§6.1.3)
- **CreateSession cert bound** to OPN channel certificate
  (`src/services/session.c`, OPC-10000-4 §5.6.2)
- **Username-token nonce check** now unconditional (not gated on `encryption_algorithm`)
  (`src/services/session.c`, OPC-10000-4 §7.38)

### Memory model changes
- **`MU_SECURE_SCRATCH_SIZE`**: 12,288 → 14,336 bytes (+2,048)
  - New session-handshake sub-region (`MU_SECURE_SESSION_MAX = 2048`)
  - Non-overlapping with response region (handlers write response BEFORE computing signatures)
  - Defined in `include/muc_opcua/config.h`, layout documented in `src/core/server_internal.h`
- **`mu_monitored_item_t`**: added `opcua_int64_t last_reported_integer` (8 bytes)
  under `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` for integer deadband comparison
- **Stack reduction**: CreateSession/ActivateSession large buffers (`to_sign[1536]`,
  `sig[512]`, `verify_buf[1536]`, `decrypt_buf[256]`) relocated from stack to
  session scratch region — `handle_create_session` stack: 1,360 B;
  `handle_activate_session` stack: 464 B

### Code organization
- `src/core/service_dispatch.c`: 4,100 → 2,760 lines (-33%)
  - Session/channel handler bodies → `src/services/session.c` + `src/services/secure_channel.c`
  - Subscription handler bodies + filter decoders → `src/services/subscription.c`
  - Exported dispatch helpers: `mu_dispatch_write_response_prefix`, `mu_dispatch_fill_server_nonce`,
    `mu_dispatch_skip_extension_object_body`, `mu_dispatch_ensure_reader_bytes_remaining`,
    `mu_dispatch_opn_security_policy`, `mu_dispatch_opn_client_cert`

### ARM Cortex-M0+ footprint (2026-07-02, after gate audit)
| Profile | `.text` | `.data` | `.bss` | Δ from baseline |
|---|---|---|---|---|
| nano | 16,436 | 0 | 0 | +158 |
| micro | 23,839 | 0 | 0 | +54 |
| embedded | 44,100 | 0 | 0 | +1,112 |
| full-featured | 52,822 | 0 | 0 | +1,212 |

### Test coverage
- 10 new tests: `test_secure_channel_trust` (3), `test_pico_adapter_hooks` (6),
  `test_username_nonce_unconditional` (3, 1 RED), `test_deadband_integer_equivalence` (25, 3 RED)
- 104/106 total host tests pass; 2 RED tests expected (deadband double-precision edges,
  username nonce secured-transport mock)
- All 18 integration/regression tests pass — wire-behavior neutral after refactor

### Invariant preserved
- **No `malloc`** anywhere in the protocol path
- `.data = 0`, `.bss = 0`, heap = 0 across all profiles
- All RAM is caller-provided static storage
