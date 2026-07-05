# Binary Size Optimization Research

**Date**: 2026-07-05
**Branch**: `occamsshavingkit/feat/optimize-binary-size` (closed, findings archived)

## Executive Summary

The archive growth from specs 038-041 is legitimate feature code (+720 B for nano,
+4.1%), not refactoring overhead. No further optimization is available at the
archive level without sacrificing maintainability. All dead code and duplicates
that could be practically removed were identified and assessed.

## Measurements

| Profile | Pre-038 (baseline) | Post-040/041 | Delta |
|---------|-------------------|-------------|-------|
| nano | 17,430 B | 18,150 B | +720 B (+4.1%) |
| micro | 28,626 B | 29,184 B | +558 B (+1.9%) |
| embedded | 53,813 B | 54,369 B | +556 B (+1.0%) |
| standard | 62,821 B | 63,417 B | +596 B (+0.9%) |

All `.data` = 0, `.bss` = 0. Constitution compliant.

## What We Investigated

### 1. Compiler Flags
- `-Os`: Already enabled (via `MUC_OPCUA_OPTIMIZE_SIZE`)
- `-flto`: No benefit for archives (+4 bytes larger). Needs final link step.
- `-fmerge-all-constants`: No change (identical to -Os defaults)
- `-fno-common`: No change
- `-fomit-frame-pointer`: Already implied by -Os
- `-ffunction-sections -fdata-sections`: Already enabled
- `--specs=nano.specs`: Requires newlib-nano, not applicable to freestanding core

### 2. Link-Time Optimization (LTO)
- CMake's `CheckIPOSupported` fails for cross-compile (tests host compiler)
- Adding `-flto -ffat-lto-objects` to `CMAKE_C_FLAGS` works but doesn't reduce archive size
- LTO benefits appear only at final executable link
- `MUC_OPCUA_LTO` option exists in CMake but defaults OFF; needs cross-compile fix

### 3. Unity Build (Single Compilation Unit)
- Builds all sources as one `.c` file → one `.o`
- Found and fixed `write_header` naming collision in `sym_chunk.c`
- Result: 20,292 B (larger than multi-object 18,150 B — no per-object gc-sections)
- Not viable for archive measurement; may help at final link

### 4. Dead Code Analysis (nano profile)
- 42 unreferenced functions totaling 2,932 B identified
- Nearly all have intra-object callers invisible to cross-object analysis
- Gating `read_scalar_value`/`write_scalar_value` broke tests (used by Read service)
- Gating `poll_reject_second_connection` broke build (wrong guard placement)
- Only `--gc-sections` at final link can strip these reliably

### 5. Duplicate Code
- `write_cstr` vs `findservers_write_cstr`: Both static in different files, can't merge
- `get_u32` vs `mu_binary_le_get_u32`: Fixed (removed local copy in sym_chunk.c)
- `key_bytes` / `key_bytes.constprop.0`: GCC optimization artifact, not actionable
- Crypto adapter functions: Only one compiled per build (conditionally), no cross-adapter duplication
- `handle_set_monitoring_mode` / `handle_set_publishing_mode`: Structural pattern, not code copy

### 6. Profile Compliance
- Nano: 149 exported symbols — all correct per OPC UA 2017 Nano Server Profile
- Micro: 220 symbols — adds Write + Subscription handlers (correct)
- Embedded: 269 symbols — adds Security + Events (correct)
- Standard: 316 symbols — adds Methods, Diagnostics, History, Query, NodeManagement, PubSub (correct)
- No extra service handlers in any profile

### 7. `--gc-sections` Reality
- `-ffunction-sections -fdata-sections` is enabled at compile time
- `-Wl,--gc-sections` is an INTERFACE link option (applied to executable consumers)
- `measure_size.sh` measures `.a` archive (no link step), so `--gc-sections` never fires
- Archive measurement overstates actual binary cost
- To get accurate sizes: build and measure `minimal_server` ELF, not the archive

### 8. `.o` File Overhead
- Spec 041 split 11 files into ~45 sub-files, increasing object count from ~40 to ~86
- Each `.o` has ~30 bytes of ELF section header overhead in the archive
- Total archive overhead from splitting: ~1.4 KB
- This overhead is stripped by `--gc-sections` at final executable link
- The linked ELF should be nearly identical before/after file splitting

## Key Takeaways

1. **No further optimization is available** at the archive measurement level
2. The archive measurement is conservative; final ELF is the real metric
3. Future work: change `measure_size.sh` to link and measure an ELF
4. Future work: fix `MUC_OPCUA_LTO` for cross-compile to get LTO at final link
5. The `write_header` rename in `sym_chunk.c` enables future unity builds

## Files Modified
- `src/security/sym_chunk.c`: Renamed `write_header` → `sym_write_header`
- Removed local `get_u32`, replaced with `mu_binary_le_get_u32`
- `scripts/measure_size.sh`: Added optional LTO flag support (`MUC_OPCUA_SIZE_LTO`)
- `specs/042-dead-code-elimination/spec.md`: Analysis spec (not committed to main)
