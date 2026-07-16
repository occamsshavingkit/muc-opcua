<!-- markdownlint-disable MD013 -->

# Feature Specification: Nano Time-Sync Support — configurable clock skew

**Feature Branch**: `075-nano-time-sync`
**Created**: 2026-07-16
**Status**: Draft
**Input**: Nano's last unreconciled *required* CU is `opc_cu_5793` (Time Sync – Support). Grounding + domain guidance: the CU is satisfied once OS-based (`2478`) or NTP (`2786`) time is available, and the muc-opcua responsibility is to provide (a) the adapter API/ABI for an integrator's synchronized clock — already present as `mu_time_adapter_t.get_time` — and (b) a **configurable acceptable-clock-skew** knob. The actual NTP/PTP/802.1AS protocol implementations are out of scope (integrator's, via the adapter).

## Context and Grounding

`opcua_datetime_t` is 100 ns ticks. Today the OpenSecureChannel timestamp
freshness / anti-replay check (`mu_opn_time_sync_allows`, osc_handler.c) uses a
**compile-time** constant `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` (default 300000 ms =
300 s). Two limits block claiming Time-Sync:

1. It is **milliseconds**, so it cannot express the sub-microsecond skew that an
   IEEE-802.1AS / PTP-disciplined server needs (~100s of ns).
2. It is a build constant, not a **runtime-settable** knob, so `opc_cu_3802`
   (Configure Clock Skew) cannot be claimed.

The time adapter (`get_time`) already lets an integrator plug an OS/NTP/PTP/
802.1AS-synchronized clock into the server; no protocol code is needed on our
side. Retyping the skew knob to nanoseconds (stored/compared in native 100 ns
ticks) and making it a config field closes the muc-opcua-side gap.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Configurable clock skew across 9 orders of magnitude (Priority: P1)

As an integrator, I set an acceptable clock skew appropriate to my time source —
~5 s for an unsynchronized/loosely-synced deployment, or ~200 ns for an 802.1AS
server — and the OpenSecureChannel freshness check enforces exactly that window.

**Why this priority**: It is the one muc-opcua-owned piece of Time-Sync support; it unlocks nano's last required CU.

**Independent Test**: Set the skew config to a coarse (seconds) and a fine (sub-microsecond) value; assert OPN accepts a request within the window and rejects one outside, at both scales.

**Acceptance Scenarios**:

1. **Given** a server with `acceptable_clock_skew_ns` set to 5 s, **When** a client's RequestHeader.timestamp is within 5 s of server time, **Then** OpenSecureChannel succeeds; beyond 5 s it returns `Bad_SecurityChecksFailed`.
2. **Given** a server with `acceptable_clock_skew_ns` set to 200 ns, **When** the timestamp differs by 100 ns, **Then** OPN succeeds; by 500 ns, **Then** it returns `Bad_SecurityChecksFailed` (sub-microsecond precision, impossible with the old ms knob).
3. **Given** either peer reports time 0 (clockless), **Then** the check is skipped (unchanged behaviour).
4. **Given** `acceptable_clock_skew_ns` is 0/unset, **Then** the server falls back to the existing `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` default (backward compatible).

### User Story 2 - Nano claims Time-Sync (Priority: P2)

As a maintainer, I want nano's required `opc_cu_5793` and the OS/NTP + configure-skew CUs reconciled once the adapter + configurable skew exist, so nano reaches full required-CU coverage honestly.

**Acceptance Scenarios**:

1. **Given** the time adapter + configurable skew, **Then** `opc_cu_5793` (Time Sync – Support), `opc_cu_2478` (OS-based), `opc_cu_2786` (NTP), and `opc_cu_3802` (Configure Clock Skew) are reconciled with backing tests; nano goes to 18/18 required.
2. **Given** IEEE-1588 PTP (`2479`), 802.1AS (`2480`), UA-based (`5505`) — the protocol implementations remain **out of scope / unclaimed** (the adapter supports an integrator implementing them, but muc-opcua does not).

## Requirements *(mandatory)*

- **FR-001**: Add a runtime config field (working name `acceptable_clock_skew_ns`, `opcua_uint64_t`, nanoseconds) to `mu_server_config_t`, gated with the time-sync surface.
- **FR-002**: The OPN freshness check MUST use the configured skew and compare **in nanoseconds** (the 100 ns-tick timestamp difference is scaled to ns), replacing the compile-time-only constant, so a host-class (x86_64/aarch64) deployment is not floored at 100 ns by our choice. The residual granularity is the OPC `DateTime` wire resolution of the compared timestamps (100 ns), a spec limit, not this knob.
- **FR-003**: When the config field is 0/unset, the check MUST fall back to `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` (backward compatible; no behaviour change for existing integrators).
- **FR-004**: The clockless (time 0) skip MUST be preserved.
- **FR-005**: The time-sync adapter (`mu_time_adapter_t.get_time`) MUST be documented as the integration point for an OS/NTP/PTP/802.1AS-synchronized clock; no protocol implementation is added.
- **FR-006**: Reconcile `opc_cu_5793`/`2478`/`2786`/`3802` for nano (satisfied by adapter + configurable skew, with backing tests); leave `2479`/`2480`/`5505` unclaimed. The change MUST be gated so non-time-sync profiles are unaffected.
- **FR-007**: Test-first, wire-visible: a test drives OPN with the configured skew at both seconds and sub-microsecond scales (FR-002 is a security-adjacent, wire-visible behaviour change).

## Success Criteria *(mandatory)*

- **SC-001**: OPN accepts/rejects by the configured skew at both ~5 s and ~200 ns scales (test-proven).
- **SC-002**: Default (unset) behaviour is byte-for-byte the pre-change freshness behaviour.
- **SC-003**: Nano reaches **18/18 required** CUs; `validate.py --all`, gating, and the per-profile matrix pass.

## Scope

- **In scope**: the runtime `acceptable_clock_skew_ns` config + 100 ns-tick freshness check; documenting the adapter as the time-sync integration point; reconciling 5793/2478/2786/3802 for nano.
- **Out of scope**: implementing NTP/PTP/802.1AS/UA-based synchronization protocols; a Time-Sync address-space node model beyond what the CUs need; changing the default skew window.

## Dependencies and Assumptions

- Assumes `opc_cu_5793` is satisfied by OS-based or NTP time availability (integrator domain guidance), and that the adapter + configurable skew is the complete muc-opcua-side obligation.
- The freshness check is security-adjacent (anti-replay); the change preserves it exactly for the default and only widens the configurable range.
