# Feature Size Ledger

Measured footprint of the micro-opcua server across the three build profiles. The
library holds **no mutable global state** (`.data` and `.bss` are zero in every
translation unit), so all RAM is caller-provided; "Heap" is zero everywhere (no
`malloc` in the protocol path).

- **Measured**: 2026-06-27 (Micro profile complete: subscriptions + ≥2 sessions)
- **Toolchain**: `arm-none-eabi-gcc` 13.2.1 (RP2040, Cortex-M0+), `gcc` (host)
- **How**: cross-compile the portable core (`src/**/*.c` minus the host POSIX/OpenSSL
  adapters) with `-Os -mcpu=cortex-m0plus -mthumb -ffunction-sections -fdata-sections`,
  sum `.text` with `size -t`; `sizeof(struct mu_server)` from the headers. Profiles map
  to `make nano` / `make micro` / `make embedded` (the `MICRO_OPCUA_*` options).

## Summary — core library `.text` (flash), ARM Cortex-M0+ Thumb `-Os`

| Profile | Services | Core `.text` | vs nano | `.data` | `.bss` | Heap |
|---|---|---|---|---|---|---|
| **nano** | Core + View + Read, None | **16.9 KiB** (17,283 B) | — | 0 | 0 | 0 |
| **micro** | nano + Data-Change Subscriptions | **23.6 KiB** (24,200 B) | +6.7 KiB | 0 | 0 | 0 |
| **embedded** | micro + Basic256Sha256 | **27.7 KiB** (28,330 B) | +10.8 KiB | 0 | 0 | 0 |

- **Subscriptions (Micro)** cost **~6.7 KiB** of flash — the engine object
  `subscription.c` is ~4.4 KiB; the rest is the Subscription/MonitoredItem dispatch
  handlers + DataChangeNotification encoding.
- **Basic256Sha256** adds a further **~4.1 KiB** of portable crypto (key derivation,
  asym/sym chunk, certificate). The host build additionally links an OpenSSL adapter; an
  MCU replaces it with a smaller mbedTLS/PSA backend.

## RAM (all caller-provided; the library adds 0 static RAM)

| Profile | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` | + RX/TX buffers |
|---|---|---|---|
| nano | **672 B** | 1024 | 2 × 8192 = 16 KiB |
| micro | **2784 B** | 3072 | 2 × 8192 = 16 KiB |
| embedded | **2952 B** | 3072 | 2 × 8192 = 16 KiB |

- The subscription engine adds **~2.1 KiB** to the server context (fixed-size
  subscription / MonitoredItem / parked-Publish arrays; capacities are `-D`-overridable:
  `MU_MAX_SUBSCRIPTIONS`=2, `MU_MAX_MONITORED_ITEMS`=8, `MU_MAX_PUBLISH_REQUESTS`=4).
  `MU_SERVER_STORAGE_BYTES` is raised to 3072 automatically when
  `MICRO_OPCUA_SUBSCRIPTIONS` is defined.
- **Static RAM for the protocol ≈ 16.7 KiB (nano) / 18.7 KiB (micro/embedded)**
  (context + the two 8 KiB transport buffers).
- Peak stack: **~5.5 KiB** plaintext (Read/Browse with the 32-deep dispatch arrays);
  **~12 KiB** on the secured path (response buffer + dispatch arrays). A secure build
  should provision ≥ 16 KiB of stack.

### Heap
- **0 bytes.** No `malloc` in the protocol hot path; storage and buffers are
  caller-owned. The subscription engine is fixed-size, no allocation.

## Budget (specs/001-minimal-embedded-server/plan.md)

| Budget | Target | Measured (micro) | Status |
|---|---|---|---|
| Flash (core, excl. board TCP/IP + crypto backend) | ≤ 128 KiB | ~23.6 KiB | ✅ well under |
| RAM (static + peak stack, excl. platform TCP/IP buffers) | ≤ 32 KiB | ~19 KiB + stack | ✅ under |
| Heap | no mandatory heap | 0 | ✅ |

On the RP2040's 264 KiB RAM, a full Micro-profile server (subscriptions + ≥2 sessions)
leaves roughly **240 KiB** for the application and network stack.

## Host reference (x86-64, `gcc -Os`)

Host figures are inflated by libc/printf/the POSIX TCP adapter (and OpenSSL for
embedded) and are *not* representative of an MCU; included for CI comparison.

| Profile | `libmicro_opcua.a` `.text` | `minimal_server` ELF `.text` |
|---|---|---|
| nano | ~32 KiB | ~36 KiB |
| micro | ~43 KiB | ~47 KiB |
| embedded | ~54 KiB | ~63 KiB |

## Secured-path stack (after in-place decrypt)

MSG chunks are decrypted in place in the receive buffer, so the secured request path
does not copy into multi-KiB scratch. Measured frames (`-fstack-usage`, host gcc):
`handle_data_chunk_secure` **6.5 KiB** (`respbody[5120]` + a 1 KiB OPN buffer),
`mu_sym_chunk_unwrap` **176 B** (was ~8.3 KiB). **Peak secured-path stack ≈ 12 KiB**
(during a Read/Browse dispatch: the response buffer plus the 32-deep
`nodes`/`results`/`ref_pool` arrays), down from ~25 KiB. Present only when a crypto
adapter is configured; the None path peaks at ~5.5 KiB.

**Update (2026-06-27, feature 004-optimization-fixes, US1/FR-001):** the secured
OpenSecureChannel path's `respbody[5120]` + `opn_buf[1024]` were relocated off the
stack into a server-owned `secure_scratch` region (`struct mu_server`, guarded by
`MICRO_OPCUA_SECURITY`). Worst-case secured-OPN stack measured by
`scripts/check_stack_usage.sh` (`-fstack-usage`, host gcc) dropped from **13,664 B
to 7,536 B** (CI gate `test_stack_budget`, threshold 10,240 B). The ~6 KiB moves
from stack to caller storage (`MU_SERVER_STORAGE_BYTES` grows by
`MU_SECURE_SCRATCH_SIZE` on security builds) — a deliberate stack→static trade for a
single-connection server.

**Update (2026-06-27, feature 004-optimization-fixes, US4/FR-015..018):** footprint
work — sticky-status binary codec (collapsing redundant per-call error checks),
table-driven service dispatch (removed the parallel switch), shared subscription
id-array handler driver, `element_size` lookup table, shared little-endian
pack/unpack helper, removal of the dead `mu_is_unsupported_service` table, and
gating `mu_status_name` strings behind `MICRO_OPCUA_STATUS_STRINGS`. Measured Micro
core `.text` (host `gcc -Os`, `libmicro_opcua.a`, subscriptions on / security off):
**44,561 B → 42,232 B = −2,329 B (~5.2%)** vs the post-US3 core. Net vs the
pre-remediation baseline (`1f84905`, 43,084 B) is **−852 B (~2%)** — US1–US3 added
parser-robustness / address-space-index / perf code that offsets part of the US4
reduction. NOTE: SC-005's ≥8% target is **not fully met** (US4 isolated ~5.2%); the
sticky-status conversion was applied conservatively (≈48 of ~294 call sites, to
guarantee byte-identical output), leaving further flash on the table, and opt-in
`MICRO_OPCUA_LTO` (default OFF) is available for additional reduction. Figures are a
host `-Os` proxy; ARM Thumb code density differs. All response bytes remain
byte-identical (golden-vector regression test).

## Notes
- `.text` figures predating 2026-06-27 (e.g. a 14.5 KiB core) were measured before the
  Nano View Service Set + Base Information node set were completed; nano core is now
  16.9 KiB.
- Levers if RAM/stack-constrained: lower the `MU_MAX_*` subscription capacities, shrink
  `MU_RETRANSMIT_BYTES` (Republish buffer, 256 B/subscription), or lower
  `MU_DISPATCH_MAX_READ_NODES` (32) and advertise a smaller `MaxNodesPerRead`.
