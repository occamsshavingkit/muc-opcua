# Implementation Plan: mDNS Discovery (F1)

**Branch**: `048-mdns-discovery` | **Date**: 2026-07-07 | **Spec**: [spec.md](./spec.md)

## Summary

Add optional mDNS-based DNS-SD discovery to the server: define a lightweight
`mu_mdns_adapter_t` platform adapter with publish/unpublish callbacks, wire it
into the server lifecycle, provide a no-op default, and ship a POSIX host adapter
that sends RFC 6763-compliant DNS-SD records via UDP multicast.

## Technical Context

**Language/Version**: C11 (freestanding with POSIX extensions for host adapter)
**Primary Dependencies**: None (POSIX socket API for host adapter only)
**Storage**: Single pointer in `mu_server_config_t` (8 bytes when enabled)
**Testing**: Manual verification via `avahi-browse`/`dns-sd`; existing CTest for regression
**Target Platform**: Host (Linux) for mDNS adapter; embedded platforms use no-op
**Performance Goals**: Publish happens once at server init, unpublish once at close
**Constraints**: Zero impact on existing profiles when `mdns_adapter = NULL`
**OPC UA References**: OPC-10000-12 §6.3 (LDS-ME)

## Constitution Check

- **Spec Fidelity**: PASS. Directly cites OPC-10000-12 §6.3.
- **Embedded C Core**: PASS. Adapter uses callback pattern; embedded platforms
  pass NULL or the no-op adapter. Host adapter is POSIX-only, compiled via
  `MUC_OPCUA_PLATFORM=posix` gate.
- **Memory Model**: PASS. Single pointer addition to config struct.
  No heap allocation. No static buffers.
- **Minimal OPC UA Surface**: PASS. Optional adapter; NULL when not needed.
  DNS-SD record content per OPC-10000-12 §6.3.3.
- **Correctness Gates**: PASS. Manual DNS-SD browse verification. Existing
  CTest suite must remain green.
- **Toolchain Discipline**: PASS. CMake build, POSIX adapter gated behind
  `MUC_OPCUA_SERVICE_DISCOVERY` or platform check.

## Files Changed

| File | Change |
|------|--------|
| `include/muc_opcua/platform.h` | Add `mu_mdns_adapter_t` definition |
| `include/muc_opcua/server.h` | Add `mdns_adapter` to `mu_server_config_t` |
| `src/core/server/init.c` | Call `publish` after init |
| `src/core/server/close.c` | Call `unpublish` |
| `src/platform/mdns_noop.c` | No-op adapter implementation |
| `src/platform/posix_mdns.c` | Host POSIX adapter (UDP multicast DNS-SD) |
| `src/platform/posix_mdns.h` | Host adapter header |
| `src/CMakeLists.txt` | Add `platform/mdns_noop.c` unconditionally, `platform/posix_mdns.c` on host, `MUC_OPCUA_MDNS_DISCOVERY` compile definition |
