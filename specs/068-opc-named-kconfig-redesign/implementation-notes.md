# Implementation Notes

## T009 Pre-Redesign Size Baseline

Recorded before generator changes for the OPC-named Kconfig redesign.

Command:

```sh
BUILD_ROOT=build/size-baseline-068 scripts/measure_size.sh standard
BUILD_ROOT=build/size-baseline-068 scripts/measure_size.sh full
```

Environment:

- Measurement target: Cortex-M0+ Thumb via `arm-none-eabi-gcc`
- Script: `scripts/measure_size.sh`
- Build root: `build/size-baseline-068`
- Date: 2026-07-11

| Profile | Archive `.text` | Archive `.data` | Archive `.bss` | Archive `dec` | ELF `.text` | ELF `.data` | ELF `.bss` | ELF `dec` | ELF RAM `.data + .bss` |
|---------|----------------:|----------------:|---------------:|--------------:|------------:|------------:|-----------:|----------:|------------------------:|
| standard | 52,023 | 0 | 0 | 52,023 | 60,436 | 1,336 | 884 | 62,656 | 2,220 |
| full | 80,863 | 0 | 0 | 80,863 | 86,100 | 1,336 | 884 | 88,320 | 2,220 |

The `build/size-baseline-068/size-report.json` file is produced by the script per run; because the two profiles were measured as separate invocations, the final JSON file contains the last run. The table above records both command outputs for T063 comparison.

## T060-T061 Final CTest Counts

The final standard-profile build configured 116 tests because profile-gated
tests for full-only features are not added in `tests/unit/CMakeLists.txt` and
`tests/integration/CMakeLists.txt` unless their feature symbols are enabled.
The command required by T060 passed with 116/116 tests.

The final full-profile build configured and passed the full 132-test suite.

## T063 Final Size Comparison

Command:

```sh
BUILD_ROOT=build/size-final-068 scripts/measure_size.sh standard
BUILD_ROOT=build/size-final-068 scripts/measure_size.sh full
```

| Profile | Archive `.text` | Archive `.data` | Archive `.bss` | Archive `dec` | ELF `.text` | ELF `.data` | ELF `.bss` | ELF `dec` | ELF RAM `.data + .bss` | Delta vs T009 |
|---------|----------------:|----------------:|---------------:|--------------:|------------:|------------:|-----------:|----------:|------------------------:|---------------|
| standard | 52,023 | 0 | 0 | 52,023 | 60,436 | 1,336 | 884 | 62,656 | 2,220 | no change |
| full | 80,863 | 0 | 0 | 80,863 | 86,100 | 1,336 | 884 | 88,320 | 2,220 | no change |

Result: no increase attributable to Kconfig renaming or menu restructuring.

## Post-Implementation Reverse Connect Ownership Correction

Reverse Connect is now owned directly by OPC Foundation CU 2867 instead of the
project aggregate `opc_cu_reverse_connect`:

- Kconfig symbol: `MUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER`
- OPC source: OPC-10000-6 §7.1.3
- Backing test: `test_reverse_connect`
- Implementation state: `claimed`

The active aggregate owner and the legacy `MUC_OPCUA_CU_REVERSE_CONNECT` and
`MUC_OPCUA_REVERSE_CONNECT` symbols were removed rather than retained as aliases,
as required by FR-004 and FR-013. Historical documentation still names the old
symbol where it explains behavior before spec 065.

Fresh verification after regeneration and the ownership correction:

- `python3 -m unittest discover -s scripts/profile_manifest -p 'test_*.py'`:
  39 tests passed.
- `python3 -m pytest scripts/profile_manifest -q`: 50 tests and 5 subtests
  passed.
- `python3 scripts/profile_manifest/validate.py --all`: `manifest: OK`.
- `bash scripts/test_profile_gating.sh`: 114 checks passed.
- Full-profile build with CU 2867 enabled: 150/150 CTest tests passed.
- Full-profile build with CU 2867 explicitly disabled: 150/150 CTest tests
  passed.
