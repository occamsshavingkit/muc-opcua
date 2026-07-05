# ADR 0002: Platform Adapter Pattern

**Date**: 2026-07-05
**Status**: Accepted

## Context

Micro-opcua must run on host development machines (Linux/macOS/Windows), bare-metal ARM Cortex-M microcontrollers (RP2040 via Pico SDK), and Arduino/PlatformIO environments. Each platform provides different TCP, time, entropy, storage, and crypto primitives. Embedding platform-specific implementations directly in the core library would make porting to each new target an invasive, error-prone exercise in `#ifdef` proliferation.

## Decision

All platform dependencies are injected through narrow, function-pointer-based adapter interfaces (vtable structs), never through conditional compilation. The mandatory adapters are `mu_tcp_adapter_t` (listen, accept, read, write, close, shutdown), `mu_time_adapter_t` (UTC time via `get_time`, monotonic ticks via `get_tick_ms`), and `mu_entropy_adapter_t` (`generate_random`). Optional adapters include `mu_crypto_adapter_t` (SHA-256, HMAC, AES-CBC encrypt/decrypt, RSA sign/verify/encrypt/decrypt, certificate validation) and `mu_persistence_adapter_t` (key-value read/write for certificate storage). Each adapter carries an opaque `void *context` for platform state. The server config struct (`mu_server_config_t`) holds adapter instances by value; `mu_server_init` validates all mandatory function pointers before starting. Feature 025 (audit remediation) added the `verify_certificate_validity` and `verify_certificate_application_uri` crypto-adapter hooks — when these optional hooks are NULL, the secured path fails closed rather than silently skipping checks.

## Consequences

Porting to a new platform requires implementing only the adapter functions and providing a server config; no core library source changes are needed. The vtable dispatch has negligible overhead (one indirect call per I/O operation) and is never on a per-byte hot path. The trade-off is that API-breaking changes to adapter signatures affect every platform provider simultaneously and must be coordinated across the Pico, host (POSIX sockets), and Arduino adapter implementations in `src/adapters/`.
