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

Fresh verification in the isolated worktree after the ownership correction:

- `python3 -m unittest discover -s scripts/profile_manifest -p 'test_*.py'`:
  33 tests passed (33/33).
- `python3 scripts/profile_manifest/validate.py --manifest-only`:
  `manifest: OK`.
- Live graph resolution preserved the raw Full default as `false`, resolved CU
  2867 to enabled for Full, and emitted `default y` in generated Kconfig data.
- Full-profile build with CU 2867 enabled: 150/150 CTest tests passed.

## Reverse Connect Failure-Path Hardening (2026-07-28)

After the CU 2867 ownership correction, focused tests were added to cover
lifecycle failure paths previously untested in the Reverse Connect
implementation:

- **Partial or zero-byte ReverseHello write**: The server rejects initialization
  when the non-blocking adapter does not transmit the complete mandatory first
  ReverseHello message.
- **Write-error cleanup**: When the write to the ReverseHello endpoint
  fails mid-transmission, the server tears down the partially-constructed
  connection state without leaking resources.
- **High-uptime first poll**: A server that initiates Reverse Connect after
  extended process uptime initializes the connection activity timestamp and
  does not close the connection on its first poll while waiting for Client Hello.

These tests do not broaden the claim to new CUs; they reconcile the
existing CU 2867 claim to observable evidence by hardening error and
edge-case paths required by OPC-10000-6 §7.1.3.

## Combined Discovery Behavior (2026-07-28)

Discovery Server behavior (OPC-10000-4 §5.4) retains independent dedicated
gates for FindServers and GetEndpoints. The combined
`MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS` gate now compiles and
enables both services. An aggregate-only test configuration confirms that
FindServers and GetEndpoints dispatch successfully when the combined gate is
enabled and the two dedicated gates are disabled.
