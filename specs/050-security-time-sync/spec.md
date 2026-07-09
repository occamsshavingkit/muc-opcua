# Spec 050: Security Time Synchronization

**Status**: Draft | **Target**: Nano 2017+ (Core 2017 Server Facet)
**OPC UA References**: OPC-10000-7 §6.6.151 (Security Time Synchronization), OPC-10000-4 §A.2
**OPC UA Conformance Units**:
  - `Security Time Synch - Configuration` (Mandatory)
  - `Security Time Synch - NTP / OS Based support` (Optional)
  - `Security Time Synch - UA based support` (Optional)

## Motivation

OPC-10000-7 v1.05.02 made Security Time Synchronization mandatory in the Core 2017
Server Facet. The existing `MUC_OPCUA_TIME_SYNC` flag only populates ServerTimestamp
in response headers — a necessary but insufficient part of the conformance unit. The
mandatory `Security Time Synch - Configuration` CU requires:

1. An acceptable clock-skew configuration parameter
2. Rejection of OpenSecureChannel requests where the client's timestamp differs from
   server time by more than the configured skew

The OPC UA TCP OpenSecureChannel request (OPC-10000-4 §5.6.2.2 Table 11) includes
a `clientTimestamp` field that the server **must** validate against local time when
time synchronization is enabled.

## Requirements

### FR-001: Configurable clock skew

`config.h` gains `#ifndef MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` defaulting to 300000 (5 minutes).
Overridable with `-D`. Only compiled when `MUC_OPCUA_TIME_SYNC` is defined.

### FR-002: OPN timestamp validation

When `MUC_OPCUA_TIME_SYNC` is defined, `handle_open_secure_channel` reads the
`clientTimestamp` field from the OPN request body (after the `clientNonce`, before
`requestedLifetime`) and rejects with `Bad_SecurityChecksFailed` if
`abs(server_time - client_timestamp) > MU_TIME_SYNC_MAX_CLOCK_SKEW_MS`.

The OPC UA Binary layout for OpenSecureChannel request (OPC-10000-4 §5.6.2.2):
  - RequestHeader
  - ServerProtocolVersion (uint32)
  - RequestType (uint32)
  - SecurityMode (MessageSecurityMode enum)
  - ClientNonce (ByteString)
  - **ClientTimestamp (Int64)** ← this field exists per spec but was previously unread
  - RequestedLifetime (uint32)

### FR-003: When TIME_SYNC is OFF, behavior is unchanged

The `clientTimestamp` field is skipped via the existing reader advancement pattern
(mu_binary_read_int64 with discard) since it's always on the wire per OPC UA
Binary encoding. When `MUC_OPCUA_TIME_SYNC` is undefined, the value is read and
discarded without validation. Zero-server-timestamp behavior (FR-005/006 from
spec 044) is unchanged.

### FR-004: OPC UA Binary reader wire compatibility

The `clientTimestamp` field is always present in the OPN request body. The existing
handler already reads all fields after ClientNonce up to RequestedLifetime — it just
never decoded this field. Adding the read aligns with OPC-10000-6 §5.2.2.17 (Int64
encoding) and is a pure addition (no wire breakage).

## Out of Scope

- NTP client implementation: The `mu_time_adapter_t` abstraction means the platform
  adapter provides UTC. The library validates clock skew, not sources time.
- UA-based time sync service (part of the optional `Security Time Synch - UA based
  support` CU)
- CTT verification

## Files Changed

| File | Change |
|------|--------|
| `include/muc_opcua/config.h` | Add `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` (gated on `MUC_OPCUA_TIME_SYNC`) |
| `src/core/service_dispatch/osc_handler.c` | Read `clientTimestamp` field; validate vs server time when TIME_SYNC enabled |
| `tests/unit/test_time_sync.c` | Add tests: (a) OPN with timestamp within skew → accepted, (b) OPN with timestamp far in future → rejected, (c) OPN with timestamp far in past → rejected, (d) TIME_SYNC off → no validation |
| `docs/conformance/profile-nano.md` | Update: Security Time Synchronization targeted |
| `docs/conformance/status.md` | Mark `Security Time Synch - Configuration` targeted |

## Size Impact

- **Flash**: ~200 bytes when `MUC_OPCUA_TIME_SYNC=ON` (adapter call + comparison +
  new constant string). Zero when OFF.
- **RAM**: 0 (all validation is stack-local).
