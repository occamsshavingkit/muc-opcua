# Research: mDNS Discovery

## Decision 1: Raw UDP multicast vs Avahi/Bonjour library

**Decision**: Use raw UDP multicast with manually constructed DNS-SD records.

**Rationale**:
- No external library dependency — keeps the embedded-first principle intact
- Avahi and Bonjour are Linux/macOS-specific; raw UDP works anywhere with POSIX sockets
- DNS-SD record format is simple (~150 bytes for a single PTR + SRV + TXT + A record)
- The adapter interface (`publish`/`unpublish`) allows integrators to swap in Avahi/Bonjour later

**Alternatives considered**:
- **Avahi client library**: Cleaner API but adds `libavahi-client-dev` dependency; platform-specific
- **Bonjour/mDNSResponder**: macOS-specific, non-portable
- **Integration-only (skip adapter)**: Integrators implement their own; less convenient

## Decision 2: Pointer storage vs inline adapter

**Decision**: `mu_mdns_adapter_t *` (pointer) in config, NULL = disabled.

**Rationale**:
- Consistent with `persistence_adapter` and `crypto_adapter` patterns
- Zero storage in the config struct when disabled (just a pointer)
- Adapter can be allocated statically by the integrator

## Decision 3: Publish at mu_server_init vs separate call

**Decision**: Publish during `mu_server_init` after server state is valid.

**Rationale**:
- Fire-and-forget: server is ready when init returns
- No additional API call required from integrator
- Consistent with other init-time behaviors (e.g., address space validation)

## Decision 4: UDP multicast address and port

**Decision**: mDNS multicast group `224.0.0.251:5353` per RFC 6762.

**Rationale**: Standard mDNS multicast address and port. All mDNS responders listen here.
