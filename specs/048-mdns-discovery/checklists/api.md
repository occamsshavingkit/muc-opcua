# API Interface Quality Checklist: mDNS Discovery

**Purpose**: Validate the quality, consistency, and integration-fit of the `mu_mdns_adapter_t` API design
**Created**: 2026-07-07
**Feature**: [spec.md](../spec.md), [contracts/api.md](../contracts/api.md)

## API Completeness

- [x] CHK001 — Does the `publish` callback signature accept all parameters needed to construct a valid DNS-SD record: service type, hostname, port, TXT key/value pairs? [Completeness, Spec §FR-001]
- [x] CHK002 — Is there a matching `unpublish` / `stop` / `close` callback to tear down the published record on server shutdown? [Completeness, Spec §FR-004]
- [x] CHK003 — Is there a no-op or NULL state defined so platforms without mDNS can use the server without forcing an adapter implementation? [Completeness, Spec §FR-005]
- [x] CHK004 — Is the adapter lifecycle documented: who calls publish, when, how many times, and what happens on re-init? [Completeness, Spec §FR-003]

## API Consistency (with Existing Patterns)

- [x] CHK005 — Is the adapter struct layout consistent with existing adapters (e.g., `mu_tcp_adapter_t`, `mu_udp_adapter_t`) — first field is `void *context`, callbacks are function pointers? [Consistency, Spec §FR-001, platform.h]
- [x] CHK006 — Is the storage model (pointer in config, NULL when disabled) consistent with `mu_persistence_adapter_t *` and `mu_crypto_adapter_t *`? [Consistency, Spec §FR-002]
- [x] CHK007 — Are the callback naming conventions consistent with the project's existing adapters (verb-noun, lowercase with underscores)? [Consistency]
- [x] CHK008 — Do the callback parameter types and order match the project's existing convention (first parameter is always `void *context`)? [Consistency, platform.h]

## API Granularity & Responsibility

- [x] CHK009 — Is the adapter's responsibility correctly scoped — does it handle just mDNS record publishing, or does it conflate with other network concerns (TCP, UDP, etc.)? [Scope, Spec §FR-001]
- [x] CHK010 — Are the callback signatures sufficiently abstract to allow different backend implementations (raw UDP, Avahi, Bonjour) without requiring structural API changes? [Granularity, Gap]
- [x] CHK011 — Is the error model appropriate — publish/unpublish are void-returning (fire-and-forget), which matches the best-effort nature of mDNS; should any failure be propagated to the caller? [Responsibility, Spec §FR-003]

## Integration Points

- [x] CHK012 — Are the server integration points clearly specified: where in `mu_server_init` is publish called, and where in `mu_server_close` is unpublish called? [Integration, Spec §FR-003, FR-004]
- [x] CHK013 — Is the URL extraction logic (hostname, port from `endpoint_url`) documented as a server-side concern, separate from the adapter's network responsibilities? [Integration, Spec §FR-003]
- [x] CHK014 — Is the behavior on adapter failure (e.g., UDP socket creation failure in `mu_host_mdns_adapter_init`) consistent with the project's pattern for adapter init failures — should it abort server init or just log? [Integration, Gap]
- [x] CHK015 — Is the TXT record format (key=value pairs, which keys are required vs optional) specified at the API level or left to each adapter implementation? [Integration, Spec §OPC-002]

## Portability & Constraints

- [x] CHK016 — Are the platform assumptions of the host POSIX adapter (Linux socket API, not Windows, not RTOS) clearly documented as constraints on that implementation, not on the interface itself? [Portability, Spec §FR-006]
- [x] CHK017 — Is the host adapter's use of `INADDR_ANY` vs specific interface binding documented as a design choice that platform integrators may override? [Portability, Edge Cases]
- [x] CHK018 — Does the adapter interface leave room for a future Avahi/Bonjour-based implementation without breaking the callback signatures? [Portability, Gap]
