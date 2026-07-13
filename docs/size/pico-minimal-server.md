# Pico Minimal Server Size Report

Historical placeholder retained from the initial Pico size-report skeleton:

| Target | Flash (text) | RAM (data+bss) |
|---|---|---|
| rp2040 | 64KB | 16KB |

## Feature 020 US4 Pico Minimal Server Evidence

Measured date: 2026-06-30.

Feature: `020-audit-hardening`, task T089a with T102b blocker-remediation
refresh. This entry records the current Pico minimal-server flash/RAM evidence
state for FR-019 and SC-007.

Feature 020 resource budget from `specs/020-audit-hardening/plan.md`:

- Nano/embedded profile `.text` growth must stay under +4 KiB.
- Default minimal profile must not require new static RAM beyond existing
  server/session/channel storage unless justified.
- Protocol hot path must not add mandatory heap allocation.
- Size evidence must distinguish measured results from blocked or advisory
  checks.

### Current T102b Result

The current embedded-profile Pico build passes after increasing
`MU_CONNECTION_BASE_STORAGE_BYTES` to cover `mu_connection_t` storage outside
`rx_buffer`.

```sh
cmake --build build/t089a-pico-embedded
arm-none-eabi-size build/t089a-pico-embedded/platform/pico/pico_minimal_server.elf
```

| Artifact | Current evidence | Status |
|---|---:|---|
| `build/t089a-pico-embedded/src/libmuc_opcua.a` | 703,784 B file size | PASS |
| `build/t089a-pico-embedded/platform/pico/pico_minimal_server.elf` | 1,014,188 B file size; `text=73,428`, `data=0`, `bss=119,340`, `dec=192,768` | PASS |
| `build/t089a-pico-embedded/platform/pico/pico_minimal_server.uf2` | 138,752 B file size | PASS |

Budget and gate assessment for the current build:

- Pico embedded build artifact: **PASS**. The library archive, ELF, and UF2 are
  produced.
- Pico `.data` check: **PASS** for the ELF measurement (`data=0`). Firmware
  `.bss=119,340 B` includes caller-owned server/example storage and transport
  buffers, not core archive `.bss`.
- Protocol hot-path heap check: covered by
  `docs/size/feature-size-ledger.md`; no core protocol hot-path allocator calls
  were found.
- Release-gate honesty: this file records measured Pico artifact evidence. It
  does not provide external OPC Foundation CTT evidence. T102c separately closes
  the source-ID ledger gap in
  `docs/validation/audit-hardening-triage-ledger.md`.

### Historical Commands Run

Toolchain inventory:

```sh
arm-none-eabi-gcc --version | head -n 1
arm-none-eabi-size --version | head -n 1
cmake --version | head -n 1
```

Result: exit code `0`; transcript
`/tmp/muc-opcua-size/t089a-pico-toolchain.txt`.

Observed tools:

- `arm-none-eabi-gcc (15:13.2.rel1-2) 13.2.1 20231009`
- `GNU size (2.42-1ubuntu1+23) 2.42`
- `cmake version 3.28.3`

Legacy documented Pico command attempt:

```sh
cmake -S . -B build/t089a-pico \
  -DMUC_OPCUA_PLATFORM=pico \
  -DPICO_SDK_FETCH_FROM_GIT=ON \
  -DMUC_OPCUA_EMBEDDED_PROFILE=ON \
  -DMU_MAX_SUBSCRIPTIONS=2 \
  -DMU_MAX_MONITORED_ITEMS=100 \
  -DMU_MAX_PUBLISH_REQUESTS=5 \
  -DMU_MONITORED_QUEUE_DEPTH=2 \
  -DMU_MAX_TRIGGER_LINKS=4 \
  -DMUC_OPCUA_BUILD_EXAMPLES=ON
cmake --build build/t089a-pico
```

Result:

- Configure exit code: `0`; transcript
  `/tmp/muc-opcua-size/t089a-pico-configure.log`.
- Build exit code: `2`; transcript
  `/tmp/muc-opcua-size/t089a-pico-build.log`.
- No `build/t089a-pico/src/libmuc_opcua.a`,
  `build/t089a-pico/platform/pico/pico_minimal_server.elf`, or
  `build/t089a-pico/platform/pico/pico_minimal_server.uf2` was produced.
- Cache note: because examples are enabled and `MUC_OPCUA_PROFILE` was not set,
  this tree resolved to `MUC_OPCUA_PROFILE=full`; the current CMake profile
  selector is `MUC_OPCUA_PROFILE`, not the legacy embedded-profile boolean by
  itself.

Current embedded-profile Pico command attempt:

```sh
cmake -S . -B build/t089a-pico-embedded \
  -DMUC_OPCUA_PLATFORM=pico \
  -DPICO_SDK_FETCH_FROM_GIT=ON \
  -DMUC_OPCUA_PROFILE=embedded \
  -DMUC_OPCUA_OPTIMIZE_SIZE=ON \
  -DMU_MAX_SUBSCRIPTIONS=2 \
  -DMU_MAX_MONITORED_ITEMS=100 \
  -DMU_MAX_PUBLISH_REQUESTS=5 \
  -DMU_MONITORED_QUEUE_DEPTH=2 \
  -DMU_MAX_TRIGGER_LINKS=4 \
  -DMUC_OPCUA_BUILD_EXAMPLES=ON
cmake --build build/t089a-pico-embedded
```

Result:

- Configure exit code: `0`; transcript
  `/tmp/muc-opcua-size/t089a-pico-embedded-configure.log`.
- Build exit code: `2`; transcript
  `/tmp/muc-opcua-size/t089a-pico-embedded-build.log`.
- Artifact check command:
  `find build/t089a-pico build/t089a-pico-embedded -maxdepth 5 \( -name 'libmuc_opcua.a' -o -name 'pico_minimal_server.elf' -o -name 'pico_minimal_server.uf2' \) -printf '%p\n' | sort`.
  Exit code `0`; transcript
  `/tmp/muc-opcua-size/t089a-pico-artifact-find.log`; no artifact paths were
  printed.

Profile/toolchain knobs for the current embedded-profile attempt:

- `MUC_OPCUA_PLATFORM=pico`
- `MUC_OPCUA_PROFILE=embedded`
- `MUC_OPCUA_OPTIMIZE_SIZE=ON`
- `MUC_OPCUA_BUILD_EXAMPLES=ON`
- `MUC_OPCUA_SECURITY=ON`
- `MUC_OPCUA_SUBSCRIPTIONS=ON`
- `MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON`
- `MUC_OPCUA_BASE_NODES=ON`
- `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`
- `MUC_OPCUA_MULTIPLE_CONNECTIONS=ON`
- `MUC_OPCUA_EVENTS=ON`
- `MU_MAX_SUBSCRIPTIONS=2`
- `MU_MAX_MONITORED_ITEMS=100`
- `MU_MAX_PUBLISH_REQUESTS=5`
- `MU_MONITORED_QUEUE_DEPTH=2`
- `MU_MAX_TRIGGER_LINKS=4`
- `PICO_PLATFORM=rp2040`
- `PICO_BOARD=pico`
- `PICO_GCC_TRIPLE=arm-none-eabi`
- `CMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc`
- `CMAKE_BUILD_TYPE=Release`
- `CMAKE_C_FLAGS=-mcpu=cortex-m0plus -mthumb`

### Historical T089a Result

The original T089a Pico attempts were blocked before `muc_opcua` compiled:

```text
src/core/server_internal.h:45:1: error: static assertion failed: "MU_CONNECTION_BASE_STORAGE_BYTES must cover mu_connection_t fields outside rx_buffer"
```

That status is superseded by the current T102b result above, which produces the
Pico archive, ELF, and UF2 artifacts.

| Artifact | Current result | Flash/RAM evidence | Budget status |
|---|---|---|---|
| `build/t089a-pico/platform/pico/pico_minimal_server.elf` | Not produced | N/A | Blocked |
| `build/t089a-pico-embedded/platform/pico/pico_minimal_server.elf` | Not produced | N/A | Blocked |
| `build/t089a-pico/src/libmuc_opcua.a` | Not produced | N/A | Blocked |
| `build/t089a-pico-embedded/src/libmuc_opcua.a` | Not produced | N/A | Blocked |

### Historical Budget and Gate Assessment

- Nano/embedded +4 KiB `.text` comparison: **blocked**. No current Pico archive
  or ELF exists, and no Pico pre-change baseline is recorded in
  `docs/size/audit-hardening-baseline.md`.
- No-new-default-static-RAM check: **blocked** for Pico. There is no current
  Pico `.data`/`.bss` output to compare.
- Protocol hot-path heap check: **not established by this Pico build attempt**.
  Source-level heap review is tracked separately in
  `docs/size/feature-size-ledger.md`; this entry does not add Pico-specific heap
  evidence.
- Release-gate honesty: this historical T089a entry did not satisfy SC-007. The
  current T102b result above supersedes this blocked status.

## Feature 020 US4 Pico Minimal Server Stack Evidence

Measured date: 2026-06-30.

Feature: `020-audit-hardening`, task T089b with T102b blocker-remediation
refresh. This entry records the current Pico minimal-server stack evidence
state for FR-019 and SC-007.

Feature 020 stack budget and honesty rules from
`specs/020-audit-hardening/plan.md`, `docs/validation/audit-hardening.md`, and
`docs/size/audit-hardening-baseline.md`:

- Stack evidence must record the command, exit code, estimate, threshold,
  baseline comparison where available, and transcript/log path.
- The secured OpenSecureChannel stack threshold is `10240` bytes.
- The available pre-change stack baseline is `3040` bytes from the host
  `build/audit-embedded-stack` flow, not from a Pico minimal-server build.
- Historical blocked stack checks remain below as pre-remediation evidence; the
  current T102b stack check is recorded as measured evidence.

### Current T102b Stack Result

```sh
cmake --build build/t089b-pico-stack
bash scripts/check_stack_usage.sh --build-dir build/t089b-pico-stack --threshold 10240
```

Result: PASS. The build produced Pico stack-usage artifacts and the stack
checker reported secured OPN stack estimate `2,776 B` against the `10,240 B`
threshold. `find build/t089b-pico-stack -type f -name '*.su' | wc -l` found
`35` `.su` files.

| Frame / phase | Bytes |
|---|---:|
| `src/security/asym_chunk.c:mu_asym_chunk_unwrap` | 152 B |
| `src/security/asym_chunk.c:mu_asym_chunk_wrap` | 2,728 B |
| `src/security/asym_chunk.c:max_helper` | 48 B |
| `src/core/service_dispatch.c:mu_service_dispatch` | 64 B |
| `src/core/service_dispatch.c:handle_open_secure_channel` | 168 B |
| `src/services/secure_channel.c:mu_secure_channel_open` | 24 B |
| `src/security/sym_chunk.c:mu_sym_keys_derive` | 192 B |
| `src/security/key_derivation.c:mu_p_sha256` | 208 B |
| asymmetric unwrap/wrap phase | 2,776 B |
| OpenSecureChannel dispatch/KDF phase | 632 B |

Current stack assessment: **PASS** for the Pico secured-OPN stack threshold. It
does not provide external CTT evidence. T102c separately closes the source-ID
ledger gap in `docs/validation/audit-hardening-triage-ledger.md`.

### Historical Commands Run

Toolchain inventory:

```sh
arm-none-eabi-gcc --version | head -n 1
arm-none-eabi-size --version | head -n 1
cmake --version | head -n 1
```

Result: exit code `0`; transcript
`/tmp/muc-opcua-size/t089b-pico-stack-toolchain.txt`.

Observed tools:

- `arm-none-eabi-gcc (15:13.2.rel1-2) 13.2.1 20231009`
- `GNU size (2.42-1ubuntu1+23) 2.42`
- `cmake version 3.28.3`

Pico embedded stack-usage attempt:

```sh
cmake -S . -B build/t089b-pico-stack \
  -DMUC_OPCUA_PLATFORM=pico \
  -DPICO_SDK_FETCH_FROM_GIT=ON \
  -DMUC_OPCUA_PROFILE=embedded \
  -DMUC_OPCUA_OPTIMIZE_SIZE=ON \
  -DMUC_OPCUA_STACK_USAGE=ON \
  -DMUC_OPCUA_STACK_USAGE_LIMIT=10240 \
  -DMU_MAX_SUBSCRIPTIONS=2 \
  -DMU_MAX_MONITORED_ITEMS=100 \
  -DMU_MAX_PUBLISH_REQUESTS=5 \
  -DMU_MONITORED_QUEUE_DEPTH=2 \
  -DMU_MAX_TRIGGER_LINKS=4 \
  -DMUC_OPCUA_BUILD_EXAMPLES=ON
cmake --build build/t089b-pico-stack
bash scripts/check_stack_usage.sh --build-dir build/t089b-pico-stack --threshold 10240
find build/t089b-pico-stack -type f \( -name '*.su' -o -name 'libmuc_opcua.a' -o -name 'pico_minimal_server.elf' -o -name 'pico_minimal_server.uf2' \) -printf '%p\n' | sort
```

Result:

- Configure exit code: `0`; transcript
  `/tmp/muc-opcua-size/t089b-pico-stack-configure.log`.
- Build exit code: `2`; transcript
  `/tmp/muc-opcua-size/t089b-pico-stack-build.log`.
- Stack-check exit code: `2`; transcript
  `/tmp/muc-opcua-size/t089b-pico-stack-check.log`.
- Artifact-find exit code: `0`; transcript
  `/tmp/muc-opcua-size/t089b-pico-stack-artifacts.log`.
- The artifact find produced only
  `build/t089b-pico-stack/src/CMakeFiles/muc_opcua.dir/core/server.c.su`;
  that `.su` file is `0` bytes because compilation stopped at the static
  assertion. No `libmuc_opcua.a`, `pico_minimal_server.elf`, or
  `pico_minimal_server.uf2` was produced.

Profile/toolchain knobs from `build/t089b-pico-stack/CMakeCache.txt`:

- `MUC_OPCUA_PLATFORM=pico`
- `MUC_OPCUA_PROFILE=embedded`
- `MUC_OPCUA_OPTIMIZE_SIZE=ON`
- `MUC_OPCUA_STACK_USAGE=ON`
- `MUC_OPCUA_STACK_USAGE_LIMIT=10240`
- `MUC_OPCUA_BUILD_EXAMPLES=ON`
- `MUC_OPCUA_SECURITY=ON`
- `MUC_OPCUA_SUBSCRIPTIONS=ON`
- `MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON`
- `MUC_OPCUA_BASE_NODES=ON`
- `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`
- `MUC_OPCUA_MULTIPLE_CONNECTIONS=ON`
- `MUC_OPCUA_EVENTS=ON`
- `MU_MAX_SUBSCRIPTIONS=2`
- `MU_MAX_MONITORED_ITEMS=100`
- `MU_MAX_PUBLISH_REQUESTS=5`
- `MU_MONITORED_QUEUE_DEPTH=2`
- `MU_MAX_TRIGGER_LINKS=4`
- `PICO_PLATFORM=rp2040`
- `PICO_BOARD=pico`
- `PICO_GCC_TRIPLE=arm-none-eabi`
- `CMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc`
- `CMAKE_BUILD_TYPE=Release`
- `CMAKE_C_FLAGS=-mcpu=cortex-m0plus -mthumb`

### Historical T089b Result

The original T089b Pico stack-usage attempt was blocked before usable
stack-usage records were available:

```text
src/core/server_internal.h:45:1: error: static assertion failed: "MU_CONNECTION_BASE_STORAGE_BYTES must cover mu_connection_t fields outside rx_buffer"
```

The stack checker failed closed because the failed Pico build did not emit the
required frames:

```text
error: missing stack-usage entry for src/core/service_dispatch.c:mu_service_dispatch
```

Historical stack evidence:

| Artifact / check | Current result | Stack estimate | Threshold | Baseline comparison | Budget status |
|---|---|---:|---:|---|---|
| `build/t089b-pico-stack` configure | Exit code `0` | N/A | 10,240 B | N/A | Configure only |
| `cmake --build build/t089b-pico-stack` | Exit code `2`; static assertion failure | N/A | 10,240 B | No Pico estimate available | Blocked |
| `scripts/check_stack_usage.sh --build-dir build/t089b-pico-stack --threshold 10240` | Exit code `2`; missing required frame | N/A | 10,240 B | Cannot compare to host baseline `3,040 B` | Blocked |
| `build/t089b-pico-stack/src/CMakeFiles/muc_opcua.dir/core/server.c.su` | Produced but empty (`0` bytes) | N/A | 10,240 B | Not usable evidence | Blocked |
| `build/t089b-pico-stack/platform/pico/pico_minimal_server.elf` | Not produced | N/A | 10,240 B | No Pico firmware stack evidence | Blocked |

### Historical Budget and Gate Assessment

- Pico secured-OPN stack estimate: **blocked**. The static assertion stops the
  build before required `.su` frames are emitted.
- Threshold comparison: **blocked**. There is no Pico stack estimate to compare
  with the `10240` byte threshold.
- Baseline comparison: **blocked/incomplete** for Pico. The available
  `3040 bytes` baseline in `docs/size/audit-hardening-baseline.md` comes from
  the host `build/audit-embedded-stack` flow. It is useful context but is not a
  current Pico minimal-server stack measurement.
- Release-gate honesty: this historical T089b entry did not satisfy SC-007. The
  current T102b stack result above supersedes this blocked status.
