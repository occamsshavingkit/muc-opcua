# ADR 0004: Single-Threaded Cooperative Poll Loop

**Date**: 2026-07-05
**Status**: Accepted

## Context

Embedded platforms (particularly bare-metal ARM Cortex-M targets) typically lack preemptive threading, and even where an RTOS is present, introducing mutexes, task stacks, and context switches to the OPC UA server adds flash, RAM, and latency that conflicts with the project's size and determinism goals. The OPC UA server must handle TCP connection acceptance, message reading, service dispatch, encoding, writing, session timeout scanning, and subscription publishing — all without blocking the CPU or relying on a thread scheduler.

## Decision

All server work is driven by a single-threaded cooperative poll loop. The integrator calls `mu_server_poll(server)` in a loop — typically from `main()` or a timer ISR — at a cadence they control. Each poll call performs a bounded unit of work: accepts one new connection if a slot is free, reads available bytes from one connection, handles idle timeout, and processes one incoming message through the dispatch chain. Background tasks (session expiry scan, subscription publish cycle with keep-alive and late-publish detection) run via `mu_server_poll_background` at the end of each poll iteration. The server is never blocking: all adapter I/O functions (`accept`, `read`, `write`) are non-blocking; when no data is available the poll call returns immediately. The integrator owns the scheduling policy — they may yield to an RTOS task, call `sleep_ms`, or spin as their application requires.

## Consequences

The poll model eliminates all threading primitives, dynamic stacks, and synchronization from the core library. Flash and RAM savings are substantial on bare-metal targets (no FreeRTOS dependency). The single-threaded design also simplifies reasoning about concurrent access: there is exactly one execution context, so transient pointers into `receive_buffer` (e.g., `opn_pending_security_policy` and `opn_pending_client_cert`) are safe for the duration of a poll cycle. The trade-off is that the integrator bears responsibility for calling poll frequently enough to meet the OPC UA Subscriptions publishing interval contract — a slow poll cycle directly translates to late publish responses and subscription timeouts.
