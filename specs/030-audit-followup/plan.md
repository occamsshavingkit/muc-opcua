# Implementation Plan: Audit Follow-Up

**Branch**: `030-audit-followup` | **Date**: 2026-07-04 | **Spec**: [spec.md](./spec.md)
**Source**: `docs/review/five-lens-audit-2026-07-04.md`, Codacy HIGH findings

## Summary

Fix 4 remaining audit findings carried forward from 029: ActivateSession nonce
fail-closed (T1), password decrypt buffer zeroization (T7), channel ID entropy (T17),
and server self-cert validation (T44). All were blocked in 029 due to test
compatibility or structural issues that are now addressed.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged)
**Primary Dependencies**: None new
**Testing**: Fix existing test `fake_entropy` callback; add valgrind coverage for zeroize
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Performance Goals**: N/A (security fixes, negligible impact)
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap
**Scale/Scope**: 4 findings; ~3 source files touched; ~2 test file fixes

## Embedded Size Budget

**Flash Impact**: ~+100 B (status checks, zeroize calls, counter logic)
**RAM Impact**: 0 bytes (no new allocations)
**Heap Use**: None

## Constitution Check

All 7 principles PASS. No violations. This fixes the only remaining zeroization
gap (T7), strengthens fail-closed posture (T1, T44), and addresses channel
predictability (T17) — all aligned with Constitution V (Security Honesty).

## Project Structure

```
src/core/service_dispatch.c  — T1 fill_server_nonce, T7 decrypt_buf zeroize
src/core/server.c             — T44 self-cert validation
src/services/secure_channel.c — T17 channel ID counter
tests/unit/test_dispatch_services.c — fix fake_entropy signature
tests/integration/test_view_services.c — tolerate non-1 channel IDs (if needed)
```

## Key Differences from 029

| Issue | 029 Blockage | 030 Solution |
|-------|-------------|-------------|
| T1 fake_entropy | Wrong param order in test | Fix test callback signature |
| T7 zeroize | `mu_secure_zero` not compiled in nano | Restructure `#ifdef` to guard calls |
| T17 channel ID | Integration tests assume ID=1 | Update tests or make counter restartable |
| T44 cert check | Tests don't set up cert at init | Guard: only fail-closed when adapter configured |
