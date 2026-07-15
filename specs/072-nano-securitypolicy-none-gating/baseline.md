<!-- markdownlint-disable MD013 -->

# Size Baseline (pre-change) — Feature 072

**Captured**: 2026-07-15 at commit `e868159` (before any crypto-gate code change), via `BUILD_ROOT=build/size-arm-baseline scripts/measure_size.sh all` (arm-none-eabi-gcc, Cortex-M0+, `-Os`).

| profile | archive text | elf_text | elf_bss |
| --- | --- | --- | --- |
| nano | 29442 | 27220 | 512 |
| micro | 41420 | 39952 | 516 |
| standard | 55400 | 63796 | 888 |
| embedded | 54485 | 60724 | 516 |
| full | 82689 | 87852 | 888 |

## Contract targets

- **SC-001 (C1/US1)**: nano archive `.text` → ~23,660 and elf_text → ~21,236 (**−~5.8 KB / −~6.0 KB**), `bss` unchanged (elf_bss stays 512).
- **SC-003 (C4/US2)**: micro/embedded/standard/full `.text` **byte-identical** to the rows above (0-byte delta) — the guard swap is a pure rename for secured builds.
