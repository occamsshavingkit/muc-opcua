# Feature Size Ledger

Measured footprint of the micro-opcua server across the three build profiles. The core
has **no mutable global state** (`.data`, `.bss`, and heap are all zero — no `malloc` in
the protocol path), so all RAM is caller-provided. (Feature 004 briefly introduced ~156 B
of file-static state; issue #197 relocated it into the caller-provided server storage,
restoring `.bss = 0`.)

- **Measured**: 2026-06-28 (Embedded 2017 profile completion)
- **Toolchain**: `arm-none-eabi-gcc` 13.2.1 (RP2040, Cortex-M0+), `gcc` (host)
- **How**: cross-compile the portable core (`src/**/*.c` minus the host POSIX/OpenSSL
  adapters) with `-Os -mcpu=cortex-m0plus -mthumb -ffunction-sections -fdata-sections`,
  sum archive sections with `arm-none-eabi-size -t`; `sizeof(struct mu_server)` from the
  headers. Reproduce with `scripts/measure_size.sh all`.

## Summary — core library `.text` (flash), ARM Cortex-M0+ Thumb `-Os`

Refreshed 2026-06-28 after feature `006-implement-memory-and`.

| Profile | Services | Core `.text` | vs nano | `.data` | `.bss` | Heap |
|---|---|---|---|---|---|---|
| **nano** | Core + View + Read, None | **16.1 KiB** (16,481 B) | -232 B vs previous | 0 | 0 | 0 |
| **micro** | nano + Embedded DataChange Subscriptions | **22.4 KiB** (22,917 B) | +6.4 KiB | 0 | 0 | 0 |
| **embedded** | micro + Basic256Sha256 + Standard DataChange 2017 + Base Info Type System + required methods | **34.8 KiB** (35,598 B) | +19.1 KiB | 0 | 0 | 0 |

- **Subscriptions (Micro)** cost **~6.0 KiB** of flash (engine `subscription.c` plus the
  Subscription/MonitoredItem dispatch handlers + DataChangeNotification encoding).
- **Nano and Micro are unchanged** by feature 005/006: the current ARM Thumb totals are smaller than the previous ledger because the new Embedded 2017 work is profile-gated.
- **Embedded 2017** grows from the former security-only embedded build by **+7,797 B**
  (27,801 B -> 35,598 B). That is the mandated Standard DataChange 2017 facet, Call
  service methods, and Base Info Type System table.
- **`.bss` = 0 (no mutable global state).** Feature 004 briefly introduced ~156 B of
  file-static state (the address-space lookup index cache + the OPN policy hand-off);
  **issue #197 relocated it into the caller-provided server storage**, restoring the
  zero-mutable-globals property (verified: `nm` shows no `.bss` symbols in the core).
  The relocation moved the index into `struct mu_server`, so
  `MU_SERVER_STORAGE_BYTES` now also includes `MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES`.
  The trade cost ~170 B of `.text` per profile (the index cache is now threaded rather
  than file-static). Net core flash is still flat-to-down vs the pre-feature baseline
  (nano 16.9 → 16.1 KiB) despite the added parser-robustness, address-space index, OPN
  validation, and per-channel cipher context.

## RAM (all caller-provided; the library adds 0 static RAM)

`sizeof` measured on host (x86-64, 8-byte pointers); on Cortex-M0+ (4-byte pointers)
the context is smaller. `MU_SERVER_STORAGE_BYTES` is the arch-independent caller block
the integrator must provide (and is the value `mu_server_init` checks against).

| Profile | `sizeof(struct mu_server)` (host) | `sizeof(struct mu_server)` (ARM) | `MU_SERVER_STORAGE_BYTES` | + RX/TX buffers |
|---|---|---|---|---|
| nano | **832 B** | **576 B** | **1,280 B** | 2 × 8192 = 16 KiB |
| micro | **2,920 B** | **2,472 B** | **3,328 B** | 2 × 8192 = 16 KiB |
| embedded | **39,376 B** | **32,696 B** | **44,736 B** | 2 × 8192 = 16 KiB |

- The subscription engine adds the fixed-size subscription / MonitoredItem / parked-Publish
  arrays (capacities `-D`-overridable: `MU_MAX_SUBSCRIPTIONS`=2, `MU_MAX_MONITORED_ITEMS`=8,
  `MU_MAX_PUBLISH_REQUESTS`=4); each MonitoredItem also caches its resolved node (FR-010).
  `MU_SERVER_STORAGE_BYTES` rises to 3,328 automatically when `MICRO_OPCUA_SUBSCRIPTIONS`
  is defined.
- **Embedded 2017 grows to `MU_SERVER_STORAGE_BYTES`=44,736** because the Standard
  DataChange facet requires 100 monitored items, queue depth 2, trigger links, and five
  parked Publish requests. Thanks to our optimizations, this is **960 bytes smaller** than before.
- **Static RAM for the protocol ≈ 17.3 KiB (nano) / 19.3 KiB (micro) / 48.7 KiB (embedded)**
  (the caller-provided server context + the two 8 KiB transport buffers). The core
  library itself contributes 0 static RAM (`.data`/`.bss` = 0).
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
| nano | 32,169 B | 36,213 B |
| micro | 42,552 B | 47,072 B |
| embedded | 65,664 B | 79,264 B |

### Feature 005 US1 host proxy (Standard DataChange facet)

Measured after enabling `MICRO_OPCUA_SUBSCRIPTIONS_STANDARD=ON` with the Standard-facet
capacity overrides (`MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
`MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`). This is a host `gcc -Os`
proxy, not the final RP2040 US4 measurement.

| Build | `libmicro_opcua.a` `.text` | `.data` | `.bss` | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` |
|---|---:|---:|---:|---:|---:|
| default subscriptions/security host build | 93,145 B | 2,176 B | 0 B | 10,392 B | 10,496 B |
| Standard US1 capacity build | 101,955 B | 2,200 B | 0 B | 41,960 B | 45,696 B |
| delta | +8,810 B | +24 B | 0 B | +31,568 B | +35,200 B |

The RAM increase is caller-provided server storage for 100 fixed MonitoredItems, queue depth 2,
trigger-link fields, and five parked Publish requests. The library still contributes no `.bss`
and uses no heap. `MU_SERVER_STORAGE_BYTES` is intentionally conservative so it remains above
`sizeof(struct mu_server)` across host and embedded ABIs.

### Feature 005 US2 host proxy (Base Info Type System)

Measured with `MICRO_OPCUA_BASE_TYPE_SYSTEM=ON`, without the Standard DataChange capacity
overrides. This is a host `gcc -Os` proxy for the gated namespace-0 type-system table and browse
NodeClassMask correction, not the final RP2040 US4 measurement.

| Build | `libmicro_opcua.a` `.text` | `.data` | `.bss` | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` |
|---|---:|---:|---:|---:|---:|
| default subscriptions/security host build | 93,129 B | 2,176 B | 0 B | 10,392 B | 10,496 B |
| Base Type System build | 96,466 B | 5,520 B | 0 B | 10,392 B | 10,496 B |
| delta | +3,337 B | +3,344 B | 0 B | 0 B | 0 B |

The Base Type System slice adds gated standard type/reference nodes, HasSubtype links,
HasTypeDefinition references, and Embedded 2017 `ServerProfileArray` contents. It does not grow
the caller-provided server context, leaves `.bss` at zero, and introduces no heap use.

### Feature 005 US3 host proxy (Method Call: GetMonitoredItems + ResendData)

Measured with `MICRO_OPCUA_SUBSCRIPTIONS_STANDARD=ON`, `MICRO_OPCUA_BASE_TYPE_SYSTEM=ON`, and
the Standard-facet capacity overrides (`MU_MAX_MONITORED_ITEMS=100`,
`MU_MAX_SUBSCRIPTIONS=2`, `MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`,
`MU_MAX_TRIGGER_LINKS=4`). This is a host `gcc -Os` proxy for the gated Call service,
method-node metadata, and resend/enumeration helpers.

| Build | `libmicro_opcua.a` `.text` | `.data` | `.bss` | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` |
|---|---:|---:|---:|---:|---:|
| default subscriptions/security host build | 93,129 B | 2,176 B | 0 B | 10,392 B | 10,496 B |
| Standard+BaseType+Call capacity build | 109,297 B | 6,008 B | 0 B | 41,960 B | 45,696 B |
| delta vs default | +16,168 B | +3,832 B | 0 B | +31,568 B | +35,200 B |

The caller-storage growth is the same Standard-capacity subscription storage recorded in US1.
The US3 resend latch fits existing `mu_subscription_t` padding, so Call does not raise
`MU_SERVER_STORAGE_BYTES` beyond the Standard-capacity build. The library still contributes no
`.bss` and uses no heap.

### Feature 005 US4 host profile build (`make embedded`)

Measured with the Makefile profile wiring:
`MICRO_OPCUA_EMBEDDED_PROFILE=ON`, `MICRO_OPCUA_OPTIMIZE_SIZE=ON`, and the embedded capacity
defines (`MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
`MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`, `MU_MAX_TRIGGER_LINKS=4`).

| Artifact | `.text` | `.data` | `.bss` | Notes |
|---|---:|---:|---:|---|
| `build/embedded/src/libmicro_opcua.a` | 65,664 B | 6,008 B | 0 B | host optimized static library; core `.bss` remains zero |
| `build/embedded/examples/minimal_server` | 79,264 B | 8,068 B | 62,240 B | example-owned storage/RX/TX buffers and host runtime, not core mutable globals |

Caller-provided storage for the embedded profile:

| Metric | Host x86-64 value |
|---|---:|
| `sizeof(struct mu_server)` | 41,960 B |
| `MU_SERVER_STORAGE_BYTES` | 45,696 B |
| `sizeof(mu_subscriptions_t)` | 33,736 B |
| `sizeof(mu_subscription_t)` | 344 B |
| `sizeof(mu_monitored_item_t)` | 328 B |
| `sizeof(mu_publish_request_t)` | 48 B |

This is the expected mandated RAM growth from the Standard DataChange Subscription 2017 capacity
minima. It is caller-provided, not heap, and the core library still contributes no `.bss`.

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

## Feature 009: Core Feature Expansion
- Target size impact estimates for Write service, multiple connections, modern security policies, certificate user authentication, and alarms & events are documented in the design specification and implementation plan. Flash impact is expected to remain under 10 KB total.

