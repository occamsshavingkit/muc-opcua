# Feature Specification: mDNS Discovery (F1)

**Feature Branch**: `048-mdns-discovery`  
**Created**: 2026-07-07  
**Status**: Draft  
**Input**: TODO.md F1 — "mDNS discovery (server-side)"

## User Scenarios & Testing

### User Story 1 — Auto-Discovery on Server Start (Priority: P1)

An integrator deploying the OPC UA server on a local network wants OPC UA clients to
auto-discover the server without manual endpoint URL entry. When the server starts, it
publishes a DNS-SD (RFC 6763) record advertising `_opcua-tcp._tcp` on the local
subnet so standard OPC UA clients (UA-.NET Standard, Prosys, UaExpert) can find it.

**Independent Test**: Start the server, verify `avahi-browse _opcua-tcp._tcp` or
`dns-sd -B _opcua-tcp._tcp` shows the server with correct hostname, port, and
application URI.

**Acceptance Scenarios**:

1. **Given** a server configured with an mDNS adapter and `endpoint_url` set to
   `opc.tcp://192.168.1.100:4840`, **When** `mu_server_init` completes, **Then**
   the mDNS adapter's `publish` callback is invoked with the extracted hostname
   (`192.168.1.100`), port (`4840`), and TXT record containing `path=/discovery`
   and `applicationUri=<configured URI>`.
2. **Given** a server configured with `mdns_adapter = NULL`, **When**
   `mu_server_init` completes, **Then** no mDNS publishing occurs — server
   operates normally without discovery.
3. **Given** a server with an mDNS adapter, **When** `mu_server_close` is called,
   **Then** the adapter's `unpublish` callback is invoked to remove the DNS-SD
   record.
4. **Given** a server with the no-op mDNS adapter, **When** publish/unpublish
   are called, **Then** both return successfully without side effects.

---

### Edge Cases

- **NULL/empty endpoint_url**: Server init returns `MU_STATUS_BAD_INTERNALERROR`
  before reaching the mDNS publish step — the existing config validation rejects
  NULL config. Empty `endpoint_url` is treated as a config validation failure.
- **URL with no hostname** (`opc.tcp://:4840`): The hostname portion is empty after
  parsing. The adapter receives an empty hostname string; the adapter's publish
  callback must handle this gracefully (no-op or return immediately).
- **NULL/empty application_uri**: If NULL or empty, the TXT record omits the
  `applicationUri` field. mDNS publish still proceeds with `path=/discovery` alone.
- **Hostname vs IP**: The adapter receives the hostname string as-is extracted from
  the URL; DNS resolution is the adapter's responsibility. IPv6 addresses in
  brackets (e.g., `[::1]`) are extracted without the brackets for the hostname
  parameter; the adapter may need to add them back for SRV/A record construction.
- **Non-standard port**: The adapter publishes whatever port is extracted from the URL.
  If no port is present, the OPC UA default 4840 is assumed.
- **Publish failure**: mDNS publish is best-effort. If the adapter's publish fails
  (port in use, network unavailable), the server logs the condition at the adapter
  level and continues operating. The server does not abort init on mDNS failure.
- **Network not ready**: The adapter's publish is best-effort; the server continues
  operating even if the network interface is not ready at init time.
- **Multi-interface**: The host POSIX adapter binds to `INADDR_ANY` and sends
  multicast on all available interfaces. Platform-specific adapters may restrict
  to specific interfaces.
- **Periodic re-announcement**: Out of scope. The adapter sends a single publish
  at server start. Long-running servers should use the adapter's `publish`/
  `unpublish` callbacks to integrate with a system-level mDNS daemon (Avahi,
  Bonjour) that handles re-announcement.

## Requirements

### Functional Requirements

- **FR-001**: Define `mu_mdns_adapter_t` in `include/muc_opcua/platform.h` with
  `void *context`, `void (*publish)(void *, const char *, uint16_t, const char *, const char *)`,
  and `void (*unpublish)(void *)` fields.
- **FR-002**: Add `mu_mdns_adapter_t *mdns_adapter` field to `mu_server_config_t`
  in `include/muc_opcua/server.h` (NULL = disabled, zero storage impact).
- **FR-003**: In `mu_server_init`, after server is running, extract hostname,
  port, and application URI from `config->endpoint_url` and
  `config->application_uri`. Hostname is the string between `://` and `:<port>`
  or the end of string (IPv6 bracket-wrapped addresses have brackets stripped).
  If no port is present in the URL, default to 4840. Invoke
  `config->mdns_adapter->publish` if non-NULL. Publish failure does not abort
  init — the server continues operating.
- **FR-004**: In `mu_server_close`, invoke `config->mdns_adapter->unpublish`
  if non-NULL before shutting down.
- **FR-005**: Provide `mu_mdns_adapter_noop` — a static `extern` adapter with
  NULL callbacks that acts as a safe default for platforms without mDNS
  support. Assigning `mdns_adapter = &mu_mdns_adapter_noop` is equivalent to
  `mdns_adapter = NULL` (the server checks for NULL callbacks, not NULL pointer).
- **FR-006**: Provide `mu_host_mdns_adapter_init(mu_mdns_adapter_t *)` using
  standard POSIX UDP multicast (group `224.0.0.251:5353` per RFC 6762) to
  send DNS-SD records. The adapter sends PTR, SRV, TXT, and A records in a
  single UDP datagram. Available only on POSIX platforms (Linux, macOS).
- **FR-007**: The host adapter MUST send a "goodbye" packet (TTL=0) on unpublish
  per RFC 6762 §10.1 to expedite record removal by caches.

### OPC UA Normative Scope

- **OPC-001**: Targets the LDS-ME (Local Discovery Server - Multicast Extension)
  conformance unit per OPC-10000-12 §6.3.
- **OPC-002**: Publishes `_opcua-tcp._tcp` DNS-SD service type with TXT record
  containing `path=/discovery` (OPC-10000-12 §6.3.3).

### Scope Boundaries

- **In Scope**: mDNS adapter interface, no-op default, host POSIX adapter,
  integration into server lifecycle.
- **Out of Scope**: mDNS-SD responder daemon (LDS), DNS-SD over unicast DNS,
  periodic re-announcement, Avahi/Bonjour library dependencies (the POSIX adapter
  uses raw UDP multicast).

## Success Criteria

- **SC-001**: Server configured with the host mDNS adapter publishes an
  `_opcua-tcp._tcp` record visible to `avahi-browse` / `dns-sd` on the same host.
  Verification is manual (no CI path for UDP multicast on headless runners); see
  [quickstart.md](./quickstart.md) for the manual verification procedure.
- **SC-002**: Server configured with `mdns_adapter = NULL` operates normally
  with no mDNS activity.
- **SC-003**: `mu_server_close` un-publishes the DNS-SD record.
- **SC-004**: All existing profile tests continue to pass (zero regressions against
  the baseline established in the previous spec's final ctest run: 124 full, 96 micro,
  124 standard, 116 embedded).

## Key Entities

- **mu_mdns_adapter_t**: Platform adapter with `publish(hostname, port, app_uri,
  path)` and `unpublish()` callbacks.
- **endpoint_url**: Source for hostname and port extraction (e.g.
  `opc.tcp://192.168.1.100:4840`).
