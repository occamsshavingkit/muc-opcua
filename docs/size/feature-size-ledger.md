# Feature Size Ledger

This document tracks the cumulative memory footprint of the server against the hard limits defined in the architecture plan.

## Global Budgets
- **Maximum Flash**: 128 KiB
- **Maximum Static RAM**: 32 KiB
- **Dynamic Allocation (Heap)**: 0 bytes (forbidden in hot path)

## Ledger

| Date | Feature/Component | Flash Impact (bytes) | RAM Impact (bytes) | Cumulative Flash | Cumulative RAM | Notes |
|------|-------------------|----------------------|--------------------|------------------|----------------|-------|
| 2026-06-25 | US1: Core API & Config | ~1000 | ~4096 | ~1000 | ~4096 | Storage size defined as 4KB. Actual Flash TBD. |
| 2026-06-25 | US2: Static Address Space | ~1500 | 0 | ~2500 | ~4096 | Zero RAM overhead. Address space is strictly `.rodata`. |
| 2026-06-25 | US3: Network & Core Services | ~26000 | 0 | ~28500 | ~4096 | Full `libmicro_opcua.a` text=28.5KB, data/bss=0. |
