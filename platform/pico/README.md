# Pico Platform Adapter

This directory contains the RP2040 platform adapter (`mu_pico_adapter.c/.h`)
and a minimal server example.

## Networking

The `pico_tcp_*` functions in `mu_pico_adapter.c` are **compile-time stubs
only**. They satisfy the transport interface at link time but do not perform
any real I/O.

Real TCP networking on RP2040 requires one of:
- **LWIP** (lightweight IP stack, typically via the Pico SDK's `pico_cyw43_arch_lwip` library)
- **CYW43** (Infineon CYW43439 Wi-Fi driver, configured through `pico_cyw43_arch`)

Integrators must supply their own transport implementation that calls into the
chosen stack. See `pico_minimal_server.c` for a starting point.

## Purpose

This adapter serves as a **compile-validation target** — it verifies that the
core muc-opcua library builds correctly against RP2040 platform headers,
configuration macros, and memory constraints. It is not a runnable server
without transport integration.

## Entropy / Randomness

Production code should seed a software DRBG (e.g., CTR_DRBG from mbedTLS) from
the RP2040 ROSC entropy source rather than using the raw ROSC bits directly.
