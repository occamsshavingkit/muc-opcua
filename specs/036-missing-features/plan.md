# Implementation Plan: Missing OPC UA Features

**Branch**: `036-missing-features` | **Date**: 2026-07-05 | **Spec**: [spec.md](./spec.md)
**Source**: `docs/audit/spec-compliance-2026-07-05.md` (NOT IMPLEMENTED §3)

## Summary

Implement 15 missing-but-practical OPC UA features across subscription lifecycle,
Read/Write/Browse enhancements, and encoding extensions. Excludes out-of-scope items:
audit events (N1 — micro server), PKI infrastructure (N24-N26 — architectural),
poll-model limitations (N8 — cannot fix without threading).

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged)
**Primary Dependencies**: None new
**Testing**: Existing ctest suite (108 tests). New tests for subscription lifecycle and encoding features.
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap

## Size Budget

| Area | Est. Impact |
|------|-------------|
| Session timeout | ~+50 B flash (time check in poll) |
| Subscription lifetime | ~+30 B flash (counter check) |
| Read attributes + index_range | ~+200 B flash |
| NodeId GUID/Opaque | ~+60 B flash + 20 bytes/struct |
| Multi-chunk | ~+200 B flash (buffer management) |
| **Total estimated** | ~+600 B flash, +20 B RAM (mu_nodeid_t growth) |

## Constitution Check

- **I. Spec Fidelity**: PASS — all features cite exact OPC UA sections
- **II. Embedded-First C Core**: PASS — no heap, no OS changes
- **IV. Protocol Correctness Gates**: PASS — new features test-first
- **VII. Size Discipline**: PASS — within budget

**Result: PASS.**
