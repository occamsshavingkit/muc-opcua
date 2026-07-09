# Spec 055: Security Time Synchronization

**Status**: Implemented | **Branch**: `055-security-time-sync`
**Implements**: spec 050 draft (Project A / A1) — **corrected**, see note below
**OPC UA References**: OPC-10000-7 v1.05.02 `Security Time Synch - Configuration`
(Mandatory, Core 2017 Server Facet); OPC-10000-4 §7.28 RequestHeader; §5.6.2

## Correction to spec 050

Spec 050 stated the client timestamp is a `ClientTimestamp (Int64)` field in the
`OpenSecureChannelRequest` body **between `ClientNonce` and `RequestedLifetime`**,
and that the handler "already reads all fields up to RequestedLifetime." Both are
**wrong**: the OPC UA `OpenSecureChannelRequest` has no such field
(`osc_handler.c:81-89` reads `client_nonce` then `requested_lifetime` directly),
and reading a phantom Int64 there would corrupt the parse. The client's UTC time
is `RequestHeader.timestamp` (OPC-10000-4 §7.28), present on **every** request and
already decoded at OPN by `mu_request_header_decode` (`osc_handler.c:65`,
`req.timestamp`). This spec validates that field.

## Motivation

v1.05.02 made `Security Time Synch - Configuration` mandatory in the Core 2017
Server Facet. The existing `MUC_OPCUA_TIME_SYNC` flag only stamps `ServerTimestamp`
in response headers. The mandatory CU also requires (1) a configurable acceptable
clock-skew and (2) rejection of channel establishment when the client's timestamp
drifts beyond it — an anti-replay / freshness guard.

## Requirements

### FR-001: Configurable clock skew
`config.h` gains `#ifndef MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` defaulting to `300000`
(5 min), `-D`-overridable, only meaningful when `MUC_OPCUA_TIME_SYNC` is defined.

### FR-002: OPN timestamp validation
When `MUC_OPCUA_TIME_SYNC` is defined, `handle_open_secure_channel` compares the
already-decoded `RequestHeader.timestamp` against the server's current time (from
the time adapter). If `abs(server_time - client_timestamp)` exceeds
`MU_TIME_SYNC_MAX_CLOCK_SKEW_MS`, it rejects with `Bad_SecurityChecksFailed`.
Both times are `opcua_datetime_t` (Int64, 100 ns ticks since 1601-01-01); the
skew is converted ms → ticks (`* 10000`) with overflow-safe comparison.

### FR-003: Skip when a reference time is unavailable
Validation is skipped (channel allowed) when **either** side's timestamp is 0:
- server time 0 → no time adapter / clockless server, nothing to validate against;
- client timestamp 0 → OPC UA's "unknown time" sentinel, e.g. a clockless Nano
  client. This keeps low-end clockless devices able to open channels while still
  enforcing freshness whenever both peers supply real times.

### FR-004: When TIME_SYNC is OFF, behavior is unchanged
No skew check is compiled in; `RequestHeader.timestamp` is decoded as today and
ignored. No wire-format change in any configuration (the field was always read).

## Out of Scope
- NTP/OS or UA-based time sourcing (optional CUs); the adapter supplies UTC.
- Validating `RequestHeader.timestamp` on non-OPN service requests (this spec
  scopes the check to SecureChannel establishment).
- CTT verification.

## Files Changed
| File | Change |
|------|--------|
| `include/muc_opcua/config.h` | Add `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` (gated on `MUC_OPCUA_TIME_SYNC`) |
| `src/core/service_dispatch/osc_handler.c` | Skew check on `req.timestamp` vs server time; `Bad_SecurityChecksFailed` on drift |
| `tests/unit/test_time_sync.c` | OPN cases: within-skew accept, far-future reject, far-past reject, client-0 skip, server-0 skip, TIME_SYNC-off no-op |
| `docs/conformance/profile-nano.md`, `docs/conformance/status.md` | Mark `Security Time Synch - Configuration` targeted |

## Size Impact
Flash ~+150 B when `MUC_OPCUA_TIME_SYNC=ON` (adapter call already present at OPN;
adds an abs-diff compare + one constant). Zero RAM (stack-local). Zero when OFF.
