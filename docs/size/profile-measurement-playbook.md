# Profile Binary Measurement Playbook

## Single source of truth

Use `scripts/measure_size.sh` — it cross-compiles with `arm-none-eabi-gcc` for
Cortex-M0+ / RP2040 Thumb code using the `arduino-skeleton` platform (no host
dependencies, no Pico SDK). Running it wrong (host x86-64, missing `-Os`,
`-c`-only without link-stage garbage collection) produces meaningless numbers.

## Profiles

| Script arg | CMake profile | What it gates |
|------------|--------------|---------------|
| `nano` | `MUC_OPCUA_PROFILE=nano` | Discovery + Session + Read + Browse (None) |
| `micro` | `MUC_OPCUA_PROFILE=micro` | nano + data-change subscriptions |
| `embedded` | `MUC_OPCUA_PROFILE=embedded` | micro + Basic256Sha256 + Base Type System + Standard DataChange capacities + MultiConnection |
| `standard` | `MUC_OPCUA_PROFILE=standard` | embedded surface + StandardUA2017 advertised-profile marker and Standard capacity minima |
| `full` | `MUC_OPCUA_PROFILE=full` | everything ON |

The script accepts `full-featured` as a legacy alias for the real `full` profile.

## How to run

```bash
# All profiles at once
bash scripts/measure_size.sh all

# Single profile
bash scripts/measure_size.sh nano
bash scripts/measure_size.sh standard
```

## What to report

For each profile, report **four** numbers from the archive and one compile-time constant:

| Metric | Source | What it means |
|--------|--------|---------------|
| `.text` | `arm-none-eabi-size -t build/size-arm/<profile>/src/libmuc_opcua.a` | Flash (code) |
| `.data` | same | Initialised static data (should always be 0) |
| `.bss` | same | Uninitialised static data (should always be 0) |
| `sizeof(struct mu_server)` | `arm-none-eabi-size` on a compiled test TU that includes `server_internal.h` | RAM required at init |
| `MU_SERVER_STORAGE_BYTES` | `grep MU_SERVER_STORAGE_BYTES build/size-arm/<profile>/CMakeCache.txt` or compile a TU that includes `config.h` and prints it | Total caller-provided storage bytes |

Example table:

| Profile | .text | .data | .bss | sizeof(server) | MU_SERVER_STORAGE_BYTES |
|---------|-------|-------|------|----------------|-------------------------|
| nano | 17,430 | 0 | 0 | 664 | 1,280 |
| micro | 27,864 | 0 | 0 | ... | ... |
| standard | ... | 0 | 0 | ... | ... |
| embedded | 52,927 | 0 | 0 | 85,040 | 98,520 |
| full | 61,903 | 0 | 0 | ... | ... |

## Common mistakes

1. **Host build instead of ARM**: `cmake -DMUC_OPCUA_PLATFORM=host` produces x86-64
   code. The docs report Cortex-M0+ sizes. Always use the script.

2. **Missing `MUC_OPCUA_OPTIMIZE_SIZE=ON`**: Without `-Os` the compiler emits
   unoptimised code. The script defaults this ON.

3. **Compile-only without linking**: `arm-none-eabi-gcc -c` measures every static
   function including those stripped by `--gc-sections`. The script builds a
   `.a` archive through CMake which lets the linker eliminate dead code.

4. **Confusing library size with binary size**: The docs quote `minimal_server`
   binary sizes (linker strips unused library symbols). The script measures
   the library archive (everything compiled for the profile). Both are valid
   but not directly comparable.

5. **Skipping the `standard` profile**: Standard 2017 is a first-class profile
   (spec 037). Always measure it alongside the others.

6. **Not reporting `MU_SERVER_STORAGE_BYTES`**: Flash size is half the story.
   Static RAM (caller-provided storage) is the other half.
