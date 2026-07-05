# ADR 0001: Zero-Heap Design for the Protocol Hot Path

**Date**: 2026-07-05
**Status**: Accepted

## Context

Micro-opcua targets resource-constrained embedded platforms (ARM Cortex-M0+ with as little as 264 KB SRAM) where heap fragmentation and `malloc` failure are unacceptable failure modes on the protocol hot path. Each `read`/`write`/`publish` cycle must complete with deterministic, bounded resource usage: no allocation, no reclamation, and no possibility of out-of-memory termination mid-message. The OPC UA binary protocol (OPC-10000-6 §7) requires variable-length structures, but compliance must not compromise the embedded memory contract.

## Decision

All memory for protocol processing is caller-provided and statically allocated. The `mu_server_t` struct is placed in caller-owned storage via `mu_server_init(void *storage, size_t storage_size, ...)`, which checks alignment and size at init time. Transport buffers (`receive_buffer`, `send_buffer`), static session tables (`sessions[MU_MAX_SESSIONS]`), connection tables (`conns[MU_MAX_CONNECTIONS]`), subscription and monitored-item pools, and security channel state are all compile-time sized arrays embedded within the server struct or its sub-structs. The library never calls `malloc`/`calloc`/`free` on the protocol hot path (accept, read, dispatch, encode, write). Where burst allocation is unavoidable (e.g., variant array encoding), callers must free explicitly, and any remaining `calloc` sites are gated behind `MUC_OPCUA_ALLOW_HEAP` for host-only or bring-up use.

## Consequences

The zero-heap model guarantees O(1) startup, no malloc-linked-heap code on embedded targets, and deterministic per-poll memory use. The trade-off is that all capacity limits (`MU_MAX_SESSIONS`, `MU_MAX_CONNECTIONS`, `MU_MAX_SUBSCRIPTIONS`, `MU_MAX_MONITORED_ITEMS`, transport buffer sizes) are compile-time constants chosen by the integrator; exceeding any limit produces a hard error at init or runtime rather than dynamic growth. Buffer sizing is a conscious integrator responsibility documented in the getting-started guide.
