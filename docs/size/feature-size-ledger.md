# Feature Size Ledger

## 2026-07-15: Spec 072 — Nano SecurityPolicy-None Crypto Gating

Introduced `MUC_OPCUA_SECURE_CHANNEL_CRYPTO`, decoupling secure-channel message
crypto (sym/asym chunk, certificate, trustlist, key derivation) from the
`MUC_OPCUA_FACET_CORE_2022_SERVER` umbrella. Off for nano only; on for
micro/embedded/standard/full. `scripts/measure_size.sh all` (ARM Cortex-M0+,
`-Os`), before at commit `e868159` (archive `.text` / elf_text):

| Profile | before | after | Δ elf |
|----------|------------------:|------------------:|------:|
| nano | 29,442 / 27,220 | 23,568 / 21,320 | −5,900 |
| micro | 41,420 / 39,952 | 41,420 / 39,952 | 0 |
| embedded | 54,485 / 60,724 | 54,485 / 60,724 | 0 |
| standard | 55,400 / 63,796 | 55,400 / 63,796 | 0 |
| full | 82,689 / 87,852 | 82,689 / 87,856 | +4 |

- **SC-001 met**: nano `.text` drops ~5.9 KB, BSS unchanged (512) — the crypto
  TUs are excluded from the SecurityPolicy-None-only profile.
- **Secured builds**: archive `.text` is byte-identical for all four secured
  profiles (no code added/removed). Linked `.text` is byte-identical for
  micro/embedded/standard; **full is +4 bytes** — a benign linker-layout
  artifact from relocating `mu_secure_zero`/`mu_secure_memeq` into the
  always-compiled `secure_util.c` (full is the only secured profile with ECC,
  which calls them). Behaviour unchanged (full 136/136 pass); the identical
  archive `.text` confirms no semantic size regression.

## 2026-07-14: Spec 071 - Nano Service Behaviour

Measured with `scripts/measure_size.sh nano` after the Discovery, View, Write,
Session, and Diagnostics changes:

| Profile | text | data | bss | dec |
|---------|-----:|-----:|----:|----:|
| nano | 29,442 | 0 | 512 | 29,442 |

The current measurement is recorded as the feature endpoint. A directly
comparable pre-071 baseline was not available in the active worktree; the
historical ledger baseline is not used as a release delta because it predates
the current profile and CU set.

## 2026-07-11: Spec 066 — Client Redundancy Facet (Project B, B7)

Made `MUC_OPCUA_REDUNDANCY` real: TransferSubscriptions was a no-op self-transfer (found the
subscription keyed on the caller's session, reassigned the same session_id, 0 available
sequence numbers). Now a real cross-session transfer per OPC-10000-4 §5.14.7 —
`mu_subscription_find_any` + `mu_subscription_transfer`, `Bad_NothingToDo`/`Bad_UserAccessDenied`,
StatusChangeNotification to the old session, availableSequenceNumbers — plus per-session user
identity for the mandatory same-user check, and the `RedundancySupport` (3709) node. `full`-only.

**`.text`** (ARM Cortex-M0+ `-Os`):

| Profile | prev (post-067) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano / micro / embedded / standard | 22,531 / 31,961 / 51,773 / 52,023 | (unchanged) | 0 | REDUNDANCY is full-only |
| full | 80,187 | 80,831 | +644 | real transfer + session identity + RedundancySupport node |

**Headroom:** `full` = **80,831 B** — ~50,241 B under the 128 KiB Project-B facet stopper.
**RAM:** full `sizeof(struct mu_server)` 3,060,904 → **3,067,432** (+6,528 B — the per-session
user-identity fingerprint, `MU_INTERN_MAX_SESSIONS`=100 × ~66 B); `MU_SERVER_STORAGE_BYTES`
unchanged. nano/micro/embedded/standard byte- and RAM-identical.

**Project B complete: all seven Part-7 facets (B1–B7) grounded and formalized.**

## 2026-07-11: Spec 067 — Strict Profile Grounding

Redefined `nano`/`micro`/`embedded`/`standard` to equal exactly their namesake OPC UA
Server Profile's mandatory facet set (grounded against the OPC profile DB); `full`
unchanged. Fixes the standard==full defect (they were feature-identical) plus grounded
over-provisioning (extras stripped) and under-provisioning (nano/micro/embedded gained the
mandated Base Info Server object, RegisterNodes, and UserName/Password). `standard` is now a
strict subset of `full` (which adds 22 optional facets).

**`.text`** (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`):

| Profile | prev | now | Δ | why |
|---------|-----:|----:|--:|-----|
| nano | 17,956 | 22,531 | **+4,575** | strict: added BASE_NODES + USER_AUTH + REGISTER_NODES (mandated by NanoEmbeddedDevice2017) |
| micro | 29,706 | 31,961 | +2,255 | + BASE_NODES/REGISTER_NODES; − WRITE/MULTI_CHUNK/EXTENDED_NODEIDS (not mandated) |
| embedded | 54,972 | 51,773 | −3,199 | − EVENTS/WRITE/MULTI_CHUNK/EXTENDED_NODEIDS (not mandated by EmbeddedUA2017); + REGISTER_NODES |
| standard | 80,175 | 52,023 | **−28,152** | − 22 optional facets (Write, History, Query, NodeMgmt, PubSub, DataAccess, Events, Diagnostics, ReverseConnect, Redundancy, Aggregate, ECC, MethodServer, MultiChunk, ExtendedNodeIds, ComplexTypes, TimeSync, Auditing, DynamicNodes, Namespaces, EventFilterWhere, CustomMethods) → now a strict StandardUA2017 |
| full | 80,187 | 80,187 | 0 | unchanged (its feature set is untouched) |

`standard` is now only ~250 B of `.text` above `embedded` — the honest strict delta is X509
+ Enhanced-DataChange capacity + the profile marker (mostly RAM/capacity, not code).

**RAM** `sizeof(struct mu_server)` / `MU_SERVER_STORAGE_BYTES` (ARM): micro 27,624→19,408 /
30,720→22,496; embedded 97,224→87,280 / 127,272→117,648; standard 1,556,784→**961,840** /
2,218,644→**1,394,656**; nano and full unchanged. Optional facets are `-D` opt-ins on any
profile; `full` byte- and RAM-identical to before.

## 2026-07-11: Spec 065 — Reverse Connect Facet (Project B, B6)

Grounding: Reverse Connect is an OPTIONAL transport capability (OPC-10000-6 §7.1.3) but the
existing `MUC_OPCUA_REVERSE_CONNECT` was **broken** — it dispatched `connect()` but never sent
the mandatory-first ReverseHello, so server-initiated connections never completed. FR-1/FR-2
add the ReverseHello (RHE) encoder (`mu_tcp_create_reverse_hello`) and emit it after
`connect()` (`init.c`) before the receive loop.

**`.text`** (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`):

| Profile | prev (post-064) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,956 | 17,956 | 0 | facet off |
| micro | 29,706 | 29,706 | 0 | facet off |
| embedded | 54,972 | 54,972 | 0 | facet off (REVERSE_CONNECT not enabled) |
| standard | 79,910 | 80,175 | +265 | ReverseHello encoder + emit-on-connect |
| full | 79,922 | 80,187 | +265 | same as standard |

**Headroom:** `full` = **80,187 B** — ~50,885 B under the 128 KiB Project-B facet stopper.
**RAM:** unchanged — the encoder writes into the existing `send_buffer`; no new struct fields.
nano/micro/embedded byte-identical; no-heap `.data`/`.bss` invariants preserved.

## 2026-07-11: Spec 064 — Base Server Behaviour Facet (Project B, B5)

Grounding showed the mandatory base-behaviour CU ("Session General Service Behaviour":
auth-token check, requestHandle echo, timeoutHint) was already implemented — FR-1 adds
conformance tests only. The size cost is FR-2: making the previously-dead
`MUC_OPCUA_SERVER_DIAGNOSTICS` real — wiring the counters at session/subscription/dispatch
sites and exposing `Server.ServerDiagnostics`(2274)/`ServerDiagnosticsSummary`(2275, live
`ServerDiagnosticsSummaryDataType` i=861)/`EnabledFlag`(2294) with an ExtensionObject encoder.

**`.text`** (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`):

| Profile | prev (post-063) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,956 | 17,956 | 0 | facet off; hooks are no-op inline stubs |
| micro | 29,706 | 29,706 | 0 | facet off |
| embedded | 54,972 | 54,972 | 0 | facet off (SERVER_DIAGNOSTICS not enabled) |
| standard | 79,107 | 79,910 | +803 | counter wiring + 3 runtime nodes + summary ExtensionObject encoder |
| full | 79,107 | 79,922 | +815 | same as standard |

**Headroom:** `full` = **79,922 B** — ~51,150 B under the 128 KiB Project-B facet stopper.

**RAM** (ARM `arm-none-eabi`, `sizeof(struct mu_server)`): standard 1,556,368 → **1,556,784**
(+416 B), full 3,060,488 → **3,060,904** (+416 B) — the added `serverViewCount` counter
field plus the 3 ServerDiagnostics nodes/value-sources in the runtime-bound base space.
`MU_SERVER_STORAGE_BYTES` is unchanged (existing headroom absorbs it). nano/micro/embedded
byte- and RAM-identical; no-heap `.data`/`.bss` invariants preserved.

## 2026-07-11: Spec 063 — Enhanced DataChange Subscription 2017 Facet (Project B, B4)

Grounding (OPC profile DB, facet `EnhancedDataChangeSubscription2017` id 1678) showed the
`StandardUA2017` profile our `standard`/`full` builds advertise **mandates** this facet,
whose `Monitor MinQueueSize_05` CU requires monitored-item queue depth ≥ 5. We shipped
depth 2, so the fix raises `MU_INTERN_MONITORED_QUEUE_DEPTH` **2 → 5 for standard/full**
(embedded stays 2 for its Standard DataChange 2017 tier). Named as
`MUC_OPCUA_ENHANCED_DATACHANGE`; minima guarded by `_Static_assert`s in `subscription.h`.

**`.text`** (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`) — a capacity constant,
not code, so essentially flat:

| Profile | prev (post-062) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,956 | 17,956 | 0 | facet not claimed |
| micro | 29,706 | 29,706 | 0 | facet not claimed |
| embedded | 54,972 | 54,972 | 0 | Standard DataChange tier (depth 2) unchanged |
| standard | 79,039 | 79,107 | +68 | deeper queue loop bound / struct memset codegen |
| full | 79,039 | 79,107 | +68 | same as standard |

**Headroom:** `full` = **79,107 B** — ~51,965 B under the 128 KiB Project-B facet stopper.

**RAM** (ARM `arm-none-eabi`, `sizeof(struct mu_server)` / `MU_SERVER_STORAGE_BYTES`). The
queue is a fixed inline ring (48 B/entry on ARM); depth 2 → 5 adds 3 entries × 48 B × the
per-profile MonitoredItems count:

| Profile | sizeof(mu_server) prev → now | of which spec 063 | STORAGE_BYTES prev → now |
|---------|------------------------------:|------------------:|-------------------------:|
| nano | 784 → 792 | 0 | 1,408 → 1,408 |
| micro | 27,600 → 27,624 | 0 | 30,720 → 30,720 |
| embedded | 97,216 → 97,224 | 0 | 127,272 → 127,272 |
| standard | 1,068,200 → 1,556,368 | **+144,000** (3×48×1000) | 1,178,260 → 2,218,644 |
| full | 2,084,320 → 3,060,488 | **+288,000** (3×48×2000) | 2,300,460 → 4,380,844 |

Spec 063 contributes only the **+144 KiB (standard) / +288 KiB (full)** `sizeof` growth
above. The remainder of the standard/full jump reconciles **pre-existing struct-table
drift** (the README RAM tables predated monitored-item struct growth from earlier event /
aggregate features); this pass re-measures all five profiles fresh so the README tables are
current. nano/micro/embedded moved only by prior drift (≤ 24 B) and are unaffected by this
facet. `.text` `.data`/`.bss` no-heap invariants preserved (nano/micro/embedded).

## 2026-07-11: Spec 062 — Method Server Facet (Project B, B3)

Cumulative archive `.text` (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`) after
unifying the method API under `MUC_OPCUA_METHOD_SERVER` (context-carrying callback +
declared signature), serving/enforcing Executable (Bad_NotExecutable), per-argument
validation, `Bad_NodeIdUnknown` lookup, and the `Argument`(296) encoder serving the
built-in methods' InputArguments/OutputArguments `Argument[]` metadata.
`MUC_OPCUA_METHOD_SERVER` is default **ON standard/full**; `MUC_OPCUA_CUSTOM_METHODS`
aliases it.

| Profile | prev (post-061) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,956 | 17,956 | 0 | facet off; Executable cases gated on METHOD_SERVER |
| micro | 29,706 | 29,706 | 0 | facet off |
| embedded | 54,972 | 54,972 | 0 | facet off |
| standard | 78,120 | 79,039 | +919 | **METHOD_SERVER**: Argument encoder + ExtensionObject-array Variant path, served built-in Argument[] values, Executable attribute + Bad_NotExecutable, per-argument validation |
| full | 78,112 | 79,039 | +927 | same as standard |

(The prev column is the spec-061 figure; the negligible spec-#293 DataChangeNotification
length fix is folded in.) **Headroom:** `full` = **79,039 B** — ~52,033 B under the 128 KiB
Project-B facet stopper. nano/micro/embedded byte-for-byte unchanged (the `read_attribute`
Executable/UserExecutable cases are `#if MUC_OPCUA_METHOD_SERVER`).

## 2026-07-10: Spec 061 — Standard Event Subscription Server Facet (Project B, B2)

Cumulative archive `.text` (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`) after
the EventFilter WhereClause (ContentFilter) evaluator: a real operand-tree decoder +
typed-value evaluator (`event_filter.c`, `filter_reader.c` where-clause decode), the
item-owned compact `mu_where_clause_t` storage, and the encode-time count backpatch.
`MUC_OPCUA_EVENT_FILTER_WHERE` is default **ON for standard/full**, OFF elsewhere;
it now requires `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` (features.h guard).

| Profile | prev (main) | now | Δ | why |
|---------|------------:|----:|--:|-----|
| nano | 17,956 | 17,956 | 0 | no events |
| micro | 29,706 | 29,706 | 0 | no events |
| embedded | 54,760 | 54,972 | +212 | events on, WHERE off: only the shared select-clause resolver refactor (`read_simple_attribute_fields` + unified `mu_event_field_from_name`) |
| standard | 75,927 | 78,120 | +2,193 | **WHERE feature**: operand-tree decode + typed evaluator (Equals/IsNull/relational/Between/InList/And/Or/Not/Like/OfType), backpatched EventFieldList count |
| full | 75,911 | 78,112 | +2,201 | same as standard |

**Gating intact:** the ~2.2 KB evaluator lands only in WHERE-ON profiles; the +212 B on
embedded is the events-shared select-resolver consolidation (removed the duplicate
5-field resolver in favour of the unified 0-8 one). nano/micro byte-for-byte unchanged.

**Headroom:** `full` = **78,112 B** — ~52,960 B under the 128 KiB Project-B facet stopper.
Per-item RAM grows by the compact `mu_where_clause_t` (~0.5 KB, WHERE-only, accounted in
`MU_WHERE_CLAUSE_STORAGE_BYTES`).

## 2026-07-10: Spec 060 — Data Access Server Facet (Project B, B1)

Cumulative archive `.text` (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`) after
the Data Access Server Facet: the DA VariableType nodes + property instance-declarations
served in `base_nodes.c`, the EUInformation/LocalizedText/Range codecs, and the
percent-deadband accept path. `MUC_OPCUA_DATA_ACCESS` is default **ON for standard/full**,
OFF for nano/micro/embedded.

| Profile | prev (post-059) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,956 | 17,956 | 0 | DA off |
| micro | 29,550 | 29,550 | 0 | DA off |
| embedded | 54,616 | 54,624 | +8 | DA off; negligible shared-path delta |
| standard | 71,370 | 75,827 | +4,457 | **DA feature**: 8 DA type nodes + 16 property instance-decl nodes + 2 ModellingRule objects in `base_nodes.c`, EUInformation/LocalizedText codecs, percent-deadband accept path + BrowseName EURange resolver |
| full | 71,354 | 75,811 | +4,457 | same as standard |

**Gating intact:** the ~4.5 KB DA facet code lands *only* in the DA-ON profiles;
nano/micro are byte-for-byte unchanged. Most of the delta is the served type-system
node table (24 nodes + their reference arrays + pooled BrowseName strings), which is
`#if MUC_OPCUA_DATA_ACCESS` inside the BASE_TYPE_SYSTEM node array.

**Headroom:** `full` = **75,811 B** — ~55,261 B (54 KB) under the 128 KiB Project-B
facet stopper. No new `struct mu_server` fields (the `deadband_span` monitored-item
field predates this spec).

## 2026-07-09: Spec 059 — ECC SecurityPolicies (curve25519 + nistP256)

Cumulative archive `.text` (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`) after
the full ECC secure channel: OPN ephemeral-ECDH handshake + HKDF, ECC OPN asym-chunk
(sign-only), ChaCha20-Poly1305 AEAD MSG path (curve25519) / AES-128-CBC (nistP256), and
the ECC ActivateSession application signature. `MUC_OPCUA_ECC` is default **ON for
standard/full**, **OFF for nano/micro/embedded**.

| Profile | prev (post-058) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,762 | 17,956 | +194 | ECC-OFF: only the always-compiled policy accessors (`asym_family`/`sym_mode`/`ecc_curve`, kept total for switch coverage) + `certificate.c`/`sym_chunk.c` family-branch refactors |
| micro | 29,384 | 29,550 | +166 | ECC-OFF: same always-on accessors |
| embedded | 54,456 | 54,616 | +160 | ECC-OFF: same always-on accessors |
| standard | 68,246 | 71,370 | +3,124 | **ECC feature**: `asym_ecc.c` (HKDF salt), AEAD sym-chunk, ECC branches in osc_handler/asym_chunk/data_chunk/activate_session, `mu_hkdf_sha256` |
| full | 68,266 | 71,354 | +3,088 | same as standard |

**Gating intact:** the ~3.1 KB ECC secure-channel code lands *only* in the ECC-ON
profiles. ECC-OFF profiles gained just ~160–194 B — the policy accessors that exist
regardless of build so `switch`es over `mu_security_policy_id_t` stay total (the ECC
enum values are always present; only the crypto/handshake is `#ifdef MUC_OPCUA_ECC`).
A `--gc-sections` link of an ECC-OFF ELF carries **zero** ECC crypto symbols
(`mu_ecc_derive_channel_keys`, `mu_sym_chunk_wrap_aead`, `mu_hkdf_sha256`, and the
mbedTLS/wolfSSL/OpenSSL `*ecc*`/`*chacha*` primitives all drop out).

**Headroom:** `full` is **71,354 B** — ~59,718 B (58 KB) under the 128 KiB Project-B
facet stopper. **RAM:** `mu_secure_channel_t` gained one `opcua_uint32_t`
(`in_last_sequence_number`, the AEAD previous-SequenceNumber for the Table-69 nonce) =
+4 B per SecureChannel; no other struct growth.

The ECC crypto backends (OpenSSL host, mbedTLS nistP256-only, wolfSSL both curves) are
platform adapters and are not part of this freestanding-core ARM measurement.

## 2026-07-09: Project A (specs 055/057/058) — profile modernization

Cumulative archive `.text` (ARM `-Os`) after Security Time Sync (055), Base Info
OperationLimits (057), and the documentation CU (058, docs-only). Also a **profile
gating integrity check**: the growth lands *only* where the feature is compiled.

| Profile | prev (post-056) | now | Δ | why |
|---------|----------------:|----:|--:|-----|
| nano | 17,790 | 17,762 | −28 | no `BASE_NODES`/`TIME_SYNC`; only the always-on `MaxArrayLength` check (net down w/ LTO) |
| micro | 29,412 | 29,384 | −28 | no `BASE_NODES` |
| embedded | 53,675 | 54,456 | +781 | 057 advertised OperationLimits nodes (`base_nodes.c`) |
| standard | 67,353 | 68,246 | +893 | 057 nodes + 055 time-sync (`TIME_SYNC` on) |
| full | 67,373 | 68,266 | +893 | same as standard |

**Gating intact:** 057's server-capability nodes appear only in `BASE_NODES`
profiles (embedded/standard/full); nano/micro did not gain them. 055's time-sync
code is absent from nano/micro (no `MUC_OPCUA_TIME_SYNC`). `full` is **68,266 B**
— ~61 KB under the 128 KiB Project-B facet stopper. RAM (`MU_SERVER_STORAGE_BYTES`,
`sizeof(struct mu_server)`) is unchanged by 055/057/058 (no new struct fields).

## 2026-07-09: Spec 056 capacity resolution redesign

Capacities now resolve through a single default<profile<user cascade in
`include/muc_opcua/capacities.h` (code compiles off `MU_INTERN_*`; public
`MU_MAX_*` is the `-D` override input). Two functional capacity changes ship with
it: `micro` gains a 2-connection pool (spec 056: ≥2 sessions ⇒ ≥2 SecureChannels),
and `MU_MAX_CONNECTIONS` now scales with sessions per profile (previously frozen at
4 for standard/full — the propagation bug).

**Code (`.text`) is flat** — capacity values add no code. ARM `-Os` archive `.text`:
nano 17,790 (=), embedded 53,675 (=), standard 67,353 (=), full 67,373 (=);
**micro 28,792 → 29,412 (+620 B)** for the multi-connection accept path it now
requires. `full` `.text` is unchanged, so the 128 KiB Project-B facet budget is
unaffected.

**Cost is caller RAM** (`MU_SERVER_STORAGE_BYTES`, ARM target), from the connection
pool (`conns × (8192 + 1328)`):

| Profile | storage before | storage after | Δ | driver |
|---------|---------------:|--------------:|--:|--------|
| nano | 1,408 | 1,408 | 0 | — |
| micro | 11,680 | 30,720 | +19,040 | new 2-connection pool |
| embedded | 127,272 | 127,272 | 0 | — (README's prior 128,232 was stale) |
| standard | 741,300 | 1,178,260 | +436,960 | connections 4 → 50 |
| full | 1,387,500 | 2,300,460 | +912,960 | connections 4 → 100 |

The standard/full growth is the deliberate connections=sessions scaling; it is
tunable in one place (the connection column of `capacities.h`) or per build with
`-DMU_MAX_CONNECTIONS=n`. Reproduce: `bash scripts/measure_size.sh all` (`.text`);
per-profile `MU_SERVER_STORAGE_BYTES` via an ARM `.bss`-symbol probe.

## Current (2026-07-09): No-heap embedded profiles + LTO by default

Two changes, both driven by the realistic linked-server measurement:

**LTO on by default** (`MUC_OPCUA_LTO=ON`). Recovers the cross-TU inlining lost to
the feature-041 file split. The real linked `nano` server (`init`+`poll`+`close`,
`--gc-sections`) is **15,724 B** `.text`, below the previous ~16.1 KB low.

**nano/micro/embedded are now strictly no-heap.** The linked-server measurement
revealed micro/embedded/standard were pulling newlib `malloc`+`stdio` (nonzero
`.data`/`.bss`) — the "no-heap" profiles weren't. Cause: `binary_variant.c` array
decode uses `calloc` under `MUC_OPCUA_ALLOW_HEAP` (default ON, never disabled by the
profiles), and the Write/Call array-`free` paths were ungated. Fixed by gating those
frees, un-gating the host-only crypto adapters (they need a heap and are never in the
freestanding core), and forcing `ALLOW_HEAP=OFF` for nano/micro/embedded (array
Write/Call returns `Bad_OutOfMemory` — conformant). standard/full keep the heap.

- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, Cortex-M0+ `-Os`, `arduino-skeleton`, LTO
- **Reproduce**: `bash scripts/measure_size.sh all` (archive `.text` + linked `elf_text`)

### Library archive `.text` + linked-server `.data`/`.bss`

| Profile | archive .text | linked .data | linked .bss | heap? |
|---------|---------------|--------------|-------------|-------|
| nano | 17,790 B | 0 | 0 | no |
| micro | 28,792 B | 0 | 0 | no |
| embedded | 53,675 B | 0 | 0 | no |
| standard | 67,353 B | 1,336 B | 884 B | yes (ALLOW_HEAP) |
| full | 67,373 B | 1,336 B | 884 B | yes (ALLOW_HEAP) |

(The 512 B `.bss` the linker reserves for the measurement stack is excluded above.
Real linked `nano` server: 15,724 B `.text` with LTO.) Per-profile ctest matrix
green: nano 88 / micro 96 / embedded 116 / standard 124 / full 124.

---

## 2026-07-09: De-bloat nano/micro — remove leaked soft-float + strstr

Investigating a ~2 KB archive `.text` creep in nano over the prior week uncovered a
much larger regression the archive metric could not see. **The archive `.text`
figures below count only our code; they do not include libc.** A realistic link of
the actual server runtime (`mu_server_init` + `mu_server_poll` + `mu_server_close`,
`--gc-sections`) showed the nano *binary* had grown from 16,064 B to **24,493 B** —
~7 KB of it double-precision soft-float and `strstr`/`strchr` pulled in from libc:

- `src/services/read/cache.c` (new Read `maxAge` value cache) did `double`
  arithmetic on `maxAge`, dragging in `__aeabi_dmul` / `dcmp*` / conversions (~5 KB)
  on the FPU-less Cortex-M0+. OPC-10000-4 §5.11.2 makes the cache optional (a fresh
  "best effort" value is conformant) and needs only millisecond granularity, so no
  float is required.
- `src/core/server/init.c` `parse_endpoint_url` (mDNS-only) used `strstr`/`strchr`
  but was compiled unconditionally (~2 KB).

**Fixes:** `MUC_OPCUA_READ_CACHE` (default **OFF**) gates the cache out entirely
(reads always fresh); when enabled its `maxAge` handling is integer-only with the
spec-correct Int32-max "always cache" threshold. `parse_endpoint_url` + the mDNS
publish/unpublish are gated behind `MUC_OPCUA_MDNS_DISCOVERY`. nano/micro are now
soft-float-free; embedded/standard/full retain float only in deadband/aggregate
subscription math, which is spec-inherent (operates on `Double` values).

- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, Cortex-M0+ Thumb `-Os`, `arduino-skeleton`
- **Reproduce (archive)**: `bash scripts/measure_size.sh all`
- **Reproduce (real binary)**: link `scripts/size_measure_startup.c` + a main that
  calls init/poll/close against `-lmuc_opcua` with `-Wl,--gc-sections`, then
  `arm-none-eabi-size`. NOTE: the in-tree `size_measure_main.c` only calls
  `mu_server_init`, so its ELF under-counts (dispatch stripped); the archive `.text`
  is the reported figure and the linked figure below is measured separately.

### Library archive `.text` (flash) + real linked nano

| Profile | archive .text | .data | .bss |
|---------|---------------|-------|------|
| nano | 17,882 B | 0 | 0 |
| micro | 28,952 B | 0 | 0 |
| embedded | 53,927 B | 0 | 0 |
| standard | 67,317 B | 0 | 0 |
| full | 67,337 B | 0 | 0 |

Real linked nano server (`--gc-sections`): **17,168 B** `.text` — down from 24,493 B
before the fix (−7,325 B), vs. 16,064 B a week ago (the remaining ~1.1 KB is the
feature-041 file split losing cross-TU inlining, not a leak).

### Server object and caller storage (ARM, per profile)

| Profile | sizeof(struct mu_server) | MU_SERVER_STORAGE_BYTES | Sessions | Subscriptions | Monitored Items | Publish |
|---------|--------------------------|-------------------------|----------|---------------|-----------------|---------|
| nano | 784 B | 1,408 B | 2 | — | — | — |
| micro | 11,032 B | 11,680 B | 2 | 2 | 8 | 4 |
| embedded | 102,016 B | 128,232 B | 2 | 2 | 100 | 5 |
| standard | 680,712 B | 741,300 B | 50 | 50 | 1,000 | 50 |
| full | 1,270,432 B | 1,387,500 B | 100 | 100 | 2,000 | 100 |

`sizeof(struct mu_server)` dropped 88 B per profile vs. the prior section (the
`read_cache` field is gone by default). `MU_SERVER_STORAGE_BYTES` is unchanged (it
never included the cache).

---

## 2026-07-09: Profile gating single source of truth — Spec 052

Re-measured after making `MUC_OPCUA_PROFILE` the single source of truth for the
profile feature sets and capacity minima (branch `052-fix-profile-gating`). That
fix restored `MU_MAX_SESSIONS` propagation to the compiler — it had been set in the
CMake cache but never passed as `-D`, so standard/full silently compiled a
2-session array — and made `full` advertise StandardUA2017. These figures supersede
the numbers in the older sections below, several of which pre-dated later struct
growth (a prior section even lists identical `sizeof` for standard and full, which
cannot hold at 1,000 vs 2,000 monitored items).

- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, Cortex-M0+ Thumb `-Os`
- **Target**: `MUC_OPCUA_PLATFORM=arduino-skeleton`, no host adapters
- **Reproduce**: `bash scripts/measure_size.sh all` for archive `.text`; server-object
  and storage sizes from a `char probe[sizeof(struct mu_server)]` /
  `char probe[MU_SERVER_STORAGE_BYTES]` TU compiled with each profile's flags and
  read via `arm-none-eabi-nm --print-size` (pointer size is 4 B on Cortex-M0+).

### ARM Cortex-M0+ `.text` (flash), library archive

| Profile | .text | .data | .bss |
|---------|-------|-------|------|
| nano | 18,379 B | 0 | 0 |
| micro | 29,465 B | 0 | 0 |
| embedded | 54,418 B | 0 | 0 |
| standard | 67,824 B | 0 | 0 |
| full | 67,828 B | 0 | 0 |

`.data` and `.bss` are 0 for every profile — the library declares no mutable static
state (the no-heap / caller-owns-all-memory constitution requirement, now verifiable
straight off the archive, not just the linked ELF). This measurement surfaced the one
prior violation: `mu_mdns_adapter_noop`, an all-NULL `mu_mdns_adapter_t` global added
by the mDNS feature (F1) that sat in `.bss`. It was redundant — a NULL `mdns_adapter`
in the server config already disables mDNS — so it was removed (`src/platform/mdns_noop.c`
deleted, `extern` dropped from `platform.h`) rather than merely `--gc-sections`-hidden.

### Server object and caller storage (ARM, per profile)

| Profile | sizeof(struct mu_server) | MU_SERVER_STORAGE_BYTES | Sessions | Subscriptions | Monitored Items | Publish |
|---------|--------------------------|-------------------------|----------|---------------|-----------------|---------|
| nano | 872 B | 1,408 B | 2 | — | — | — |
| micro | 11,120 B | 11,680 B | 2 | 2 | 8 | 4 |
| embedded | 102,104 B | 128,232 B | 2 | 2 | 100 | 5 |
| standard | 680,800 B | 741,300 B | 50 | 50 | 1,000 | 50 |
| full | 1,270,520 B | 1,387,500 B | 100 | 100 | 2,000 | 100 |

`MU_SERVER_STORAGE_BYTES >= sizeof(struct mu_server)` for every profile: the caller
storage holds the server object plus scratch, chunk-assembly, and security buffers.
Both are dominated by `MU_MAX_MONITORED_ITEMS`/`MU_MAX_SUBSCRIPTIONS`, which is why
the `full` profile — a non-MCU "everything on" target — needs ~1.3 MB of each.

---

## 2026-07-06: Deferred Features (D1-D4) — Spec 045

Measured after implementing complex type binary encode/decode (D1), audit event
callback dispatch (D2), additional aggregate functions (D3), and binary size
measurement tooling (D4). All features gate on existing flags with zero impact
when OFF.

- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, Cortex-M0+ Thumb `-Os`
- **Target**: `MUC_OPCUA_PLATFORM=arduino-skeleton`, no host adapters
- **Delta vs pre-045 baseline**: nano +0 B (0.0%), micro +0 B (0.0%),
  standard +0 B, embedded +0 B, full +0 B (flags OFF when not required)

### ARM Cortex-M0+ `.text` (flash)

| Profile | .text | .data | .bss |
|---------|-------|-------|------|
| nano | 18,150 B | 0 | 0 |
| micro | 29,228 B | 0 | 0 |
| embedded | 54,187 B | 0 | 0 |
| standard | 65,752 B | 0 | 0 |
| full | 65,752 B | 0 | 0 |

### Linked-ELF sizes (dead-code eliminated, `--gc-sections`)

| Profile | elf .text | elf .data | elf .bss | elf .dec |
|---------|-----------|-----------|----------|----------|
| standard | 13,104 B | 0 | 512 B | 13,616 B |

- **Archive vs ELF**: archive is conservative (no `--gc-sections`); linked ELF
  better reflects actual flash usage. Standard archive `.text` = 65,752 B,
  ELF `.text` = 13,104 B (80% dead-code elimination).
- **D1 (complex types)**: ~1-2 KB flash when `MUC_OPCUA_COMPLEX_TYPES=ON`.
  Implemented in `src/encoding/binary_complex.c`. Encode/decode for structs
  (scalar, optional, nested, array) and enumerations. Zero static RAM.
- **D2 (audit events)**: ~0.5 KB flash when `MUC_OPCUA_AUDITING=ON`.
  Callback storage: 4 × (function pointer + void*) = ~64 bytes in `mu_server`.
- **D3 (aggregate functions)**: ~2-4 KB flash when `MUC_OPCUA_AGGREGATE_FULL=ON`.
  21 new aggregate type IDs, expanded accumulator union ~180 bytes.
- **D4 (measurement tooling)**: 0 bytes (script only). Now produces linked-ELF
  measurements with JSON output and LTO comparison.

---

Measured after specs 043-044 implementation (placeholder stub tests, reverse connect,
time synchronization, async inventory doc, server-param size gating fix).

- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, Cortex-M0+ Thumb `-Os`
- **Target**: `MUC_OPCUA_PLATFORM=arduino-skeleton`, no host adapters
- **Delta vs pre-044 baseline**: nano +0 B (0.0%), micro +0 B (0.0%),
  standard +0 B, embedded +0 B, full +0 B (baseline refreshed)

### ARM Cortex-M0+ `.text` (flash)

| Profile | .text | .data | .bss |
|---------|-------|-------|------|
| nano | 18,150 B | 0 | 0 |
| micro | 29,228 B | 0 | 0 |
| embedded | 54,187 B | 0 | 0 |
| standard | 63,491 B | 0 | 0 |
| full | 63,495 B | 0 | 0 |

### Caller-provided storage (host x86-64 `sizeof`)

| Profile | sizeof(struct mu_server) | MU_SERVER_STORAGE_BYTES | Sessions | Subscriptions | Monitored Items | Publish |
|---------|--------------------------|-------------------------|----------|---------------|-----------------|---------|
| nano | 1,064 B | 1,280 B | 2 | — | — | — |
| micro | 11,448 B | 11,552 B | 2 | 2 | 8 | 2 |
| embedded | 64,752 B | 69,656 B | 2 | 2 | 100 | 5 |
| standard | 94,912 B | 97,116 B | 50 | 50 | 1,000 | 50 |
| full | 94,912 B | 97,116 B | 100 | 100 | 2,000 | 100 |

### Delta vs spec 043 baseline

| Profile | .text delta | .data delta | .bss delta |
|---------|-------------|-------------|------------|
| nano | +0 B | +0 B | +0 B |
| micro | +0 B | +0 B | +0 B |
| embedded | +0 B | +0 B | +0 B |
| standard | +0 B | +0 B | +0 B |
| full | +0 B | +0 B | +0 B |

Nano is byte-identical to pre-044 baseline (server param properly gated behind
MUC_OPCUA_TIME_SYNC). Reverse connect adds `connect` callback to TCP adapter
struct — negligible in all profiles when feature is OFF.

---

## Previous (2026-07-05): Standard 2017 UA Server Profile (Feature 037)

Measured after Standard 2017 UA Server Profile implementation. Five profiles,
all with zero `.data`, `.bss`, and heap.

- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, Cortex-M0+ Thumb `-Os`
- **Target**: `MUC_OPCUA_PLATFORM=external`, no host adapters

### ARM Cortex-M0+ `.text` (flash)

| Profile | .text | .data | .bss |
|---------|-------|-------|------|
| nano | 17,392 B | 0 | 0 |
| micro | 27,826 B | 0 | 0 |
| embedded | 52,765 B | 0 | 0 |
| standard | 62,958 B | 0 | 0 |
| full | 62,950 B | 0 | 0 |

### Caller-provided storage (`MU_SERVER_STORAGE_BYTES`)

| Profile | Storage | Sessions | Subscriptions | Monitored Items | Publish |
|---------|---------|----------|---------------|-----------------|---------|
| nano | 1,280 B | 2 | — | — | — |
| micro | 11,552 B | 2 | 2 | 8 | 2 |
| embedded | 100,792 B | 2 | 2 | 100 | 5 |
| standard | 457,052 B | 50 | 50 | 1,000 | 50 |
| full | 820,052 B | 100 | 100 | 2,000 | 100 |

- **Nano** is byte-identical to the pre-feature baseline (HANDOFF.md: 17,392 B).
- **Micro** and **embedded** show negligible growth (<1%) from feature-gate
  additions in headers and the extended `ServerProfileArray` string constant.
- **Standard** is the OPC-10000-7 minima (50 sessions, 1000 monitored items).
- **Full** is everything on with generous capacities. Not an OPC UA profile.

---

## Historical

Measured footprint of the muc-opcua server across the three build profiles. The core
has **no mutable global state** (`.data`, `.bss`, and heap are all zero — no `malloc` in
the protocol path), so all RAM is caller-provided. (Feature 004 briefly introduced ~156 B
of file-static state; issue #197 relocated it into the caller-provided server storage,
restoring `.bss = 0`.)

- **Measured**: 2026-06-28 (Feature 011 completion)
- **Toolchain**: `arm-none-eabi-gcc` 13.2.1 (RP2040, Cortex-M0+), `gcc` (host)
- **How**: cross-compile the portable core (`src/**/*.c` minus the host POSIX/OpenSSL
  adapters) with `-Os -mcpu=cortex-m0plus -mthumb -ffunction-sections -fdata-sections`,
  sum archive sections with `arm-none-eabi-size -t`; `sizeof(struct mu_server)` from the
  headers. Reproduce with `scripts/measure_size.sh all`.

## Summary — core library `.text` (flash), ARM Cortex-M0+ Thumb `-Os`

Refreshed 2026-07-04 after feature `031-deferred-audit-fixes`.

| Profile | Services | Core `.text` | vs nano | `.data` | `.bss` | Heap |
|---|---|---|---|---|---|---|
| **nano** | Core + View + Read, None | **16.1 KiB** (16,520 B) | — | 0 | 0 | 0 |
| **micro** | nano + Subscriptions + Write, None | **23.6 KiB** (24,203 B) | +7.7 KiB | 0 | 0 | 0 |
| **embedded** | micro + Security + Events | **43.7 KiB** (44,764 B) | +28.2 KiB | 0 | 0 | 0 |
| **full-featured** | embedded + methods + diagnostics + dynamic nodes | **52.3 KiB** (53,514 B) | +36.9 KiB | 0 | 0 | 0 |

- **Subscriptions (Micro)** cost **~7.7 KiB** of flash (engine `subscription.c` plus the
  Subscription/MonitoredItem dispatch handlers + DataChangeNotification encoding).
- **Nano and Micro are unchanged** by feature 005/006: the current ARM Thumb totals are
  smaller than the previous ledger because the new Embedded 2017 work is profile-gated.
- **Embedded 2017** compiles to **44,764 B** of flash, which includes standard
  subscriptions, security policies, event notifications, and the feature-025/026/031
  audit remediation fixes (binary search dispatch, bitmap publish scan, session
  ordering, nonce zeroization).
- **`.bss` = 0 (no mutable global state).** The bitmap added by feature 031 is
  allocated in the caller-provided `mu_subscriptions_t` struct — zero static RAM impact.
  Feature 004's file-static state was relocated in issue #197, restoring zero-BSS.
- **Feature 031 delta vs previous ledger (16,278/23,785/42,990/51,612 B):**
  nano +242 B (+1.5%), micro +418 B (+1.8%), embedded +1,774 B (+4.1%),
  full-featured +1,902 B (+3.7%). The subscription bitmap, binary search dispatch,
  session cleanup path, and nonce zeroization add modest .text to the shared core.
  No new heap, no new .bss, no new .data.

## RAM (all caller-provided; the library adds 0 static RAM)

`sizeof` measured on host (x86-64, 8-byte pointers); on Cortex-M0+ (4-byte pointers)
the context is smaller. `MU_SERVER_STORAGE_BYTES` is the arch-independent caller block
the integrator must provide (and is the value `mu_server_init` checks against).

| Profile | `sizeof(struct mu_server)` (host) | `sizeof(struct mu_server)` (ARM) | `MU_SERVER_STORAGE_BYTES` | + RX/TX buffers |
|---|---|---|---|---|
| nano | **912 B** | **648 B** | **1,280 B** | 2 × 8192 = 16 KiB |
| micro | **3,016 B** | **2,552 B** | **3,328 B** | 2 × 8192 = 16 KiB |
| embedded | **62,112 B** | **54,848 B** | **63,240 B** | 2 × 8192 = 16 KiB |
| full-featured | **62,440 B** | **55,008 B** | **63,240 B** | 2 × 8192 = 16 KiB |

- The subscription engine adds the fixed-size subscription / MonitoredItem / parked-Publish
  arrays (capacities `-D`-overridable: `MU_MAX_SUBSCRIPTIONS`=2, `MU_MAX_MONITORED_ITEMS`=8,
  `MU_MAX_PUBLISH_REQUESTS`=4); each MonitoredItem also caches its resolved node (FR-010).
  `MU_SERVER_STORAGE_BYTES` rises to 3,328 automatically when `MUC_OPCUA_SUBSCRIPTIONS`
  is defined.
- **Embedded 2017 grows to `MU_SERVER_STORAGE_BYTES`=63,240** because the Standard
  DataChange facet requires 100 monitored items, queue depth 2, trigger links, five
  parked Publish requests, secure scratch buffer (12 KiB), events, and multiple client connections.
- **Static RAM for the protocol ≈ 17.3 KiB (nano) / 19.3 KiB (micro) / 79.3 KiB (embedded)**
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

| Profile | `libmuc_opcua.a` `.text` | `minimal_server` ELF `.text` |
|---|---|---|
| nano | 32,169 B | 36,213 B |
| micro | 42,552 B | 47,072 B |
| embedded | 65,664 B | 79,264 B |

### Feature 005 US1 host proxy (Standard DataChange facet)

Measured after enabling `MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON` with the Standard-facet
capacity overrides (`MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
`MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`). This is a host `gcc -Os`
proxy, not the final RP2040 US4 measurement.

| Build | `libmuc_opcua.a` `.text` | `.data` | `.bss` | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` |
|---|---:|---:|---:|---:|---:|
| default subscriptions/security host build | 93,145 B | 2,176 B | 0 B | 10,392 B | 10,496 B |
| Standard US1 capacity build | 101,955 B | 2,200 B | 0 B | 41,960 B | 45,696 B |
| delta | +8,810 B | +24 B | 0 B | +31,568 B | +35,200 B |

The RAM increase is caller-provided server storage for 100 fixed MonitoredItems, queue depth 2,
trigger-link fields, and five parked Publish requests. The library still contributes no `.bss`
and uses no heap. `MU_SERVER_STORAGE_BYTES` is intentionally conservative so it remains above
`sizeof(struct mu_server)` across host and embedded ABIs.

### Feature 005 US2 host proxy (Base Info Type System)

Measured with `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`, without the Standard DataChange capacity
overrides. This is a host `gcc -Os` proxy for the gated namespace-0 type-system table and browse
NodeClassMask correction, not the final RP2040 US4 measurement.

| Build | `libmuc_opcua.a` `.text` | `.data` | `.bss` | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` |
|---|---:|---:|---:|---:|---:|
| default subscriptions/security host build | 93,129 B | 2,176 B | 0 B | 10,392 B | 10,496 B |
| Base Type System build | 96,466 B | 5,520 B | 0 B | 10,392 B | 10,496 B |
| delta | +3,337 B | +3,344 B | 0 B | 0 B | 0 B |

The Base Type System slice adds gated standard type/reference nodes, HasSubtype links,
HasTypeDefinition references, and Embedded 2017 `ServerProfileArray` contents. It does not grow
the caller-provided server context, leaves `.bss` at zero, and introduces no heap use.

### Feature 005 US3 host proxy (Method Call: GetMonitoredItems + ResendData)

Measured with `MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON`, `MUC_OPCUA_BASE_TYPE_SYSTEM=ON`, and
the Standard-facet capacity overrides (`MU_MAX_MONITORED_ITEMS=100`,
`MU_MAX_SUBSCRIPTIONS=2`, `MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`,
`MU_MAX_TRIGGER_LINKS=4`). This is a host `gcc -Os` proxy for the gated Call service,
method-node metadata, and resend/enumeration helpers.

| Build | `libmuc_opcua.a` `.text` | `.data` | `.bss` | `sizeof(struct mu_server)` | `MU_SERVER_STORAGE_BYTES` |
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
`MUC_OPCUA_PROFILE=embedded`, `MUC_OPCUA_OPTIMIZE_SIZE=ON`, and the embedded capacity
defines (`MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
`MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`, `MU_MAX_TRIGGER_LINKS=4`).

| Artifact | `.text` | `.data` | `.bss` | Notes |
|---|---:|---:|---:|---|
| `build/embedded/src/libmuc_opcua.a` | 65,664 B | 6,008 B | 0 B | host optimized static library; core `.bss` remains zero |
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

### Feature 020 US4 blocker-remediation refresh

Updated 2026-06-30 after the T102b blocker fixes. This section supersedes the
historical T088/T099/T100 attempts below for the active audit-hardening blocker
rows. Older failed rows are retained as pre-remediation evidence.

Current host/full default size evidence:

| Artifact | text | data | bss | dec | Baseline | Delta / status |
|---|---:|---:|---:|---:|---|---|
| `build/audit-host-size-default/src/libmuc_opcua.a` (`MUC_OPCUA_PROFILE=full`, default `MUC_OPCUA_OPTIMIZE_SIZE=ON`) | 96,398 B | 6,224 B | 0 B | 102,622 B | host archive baseline `text=152,709 B`, `data=6,224 B`, `bss=0 B` | text `-56,311 B`; data `+0 B`; bss `+0 B`; **PASS** for the +8 KiB host/full text budget |
| `build/audit-host-size-off/src/libmuc_opcua.a` (`MUC_OPCUA_OPTIMIZE_SIZE=OFF`) | 166,784 B | 6,224 B | 0 B | 173,008 B | host archive baseline above | reference-only opt-out build; not the constrained-device default |

Current ARM Cortex-M0+ profile matrix:

| Profile | text | data | bss | dec | Status |
|---|---:|---:|---:|---:|---|
| nano | 16,366 B | 0 B | 0 B | 16,366 B | PASS; current value recorded |
| micro | 23,873 B | 0 B | 0 B | 23,873 B | PASS; current value recorded |
| embedded | 43,078 B | 0 B | 0 B | 43,078 B | PASS; current value recorded |
| full-featured | 51,172 B | 0 B | 0 B | 51,172 B | PASS; current value recorded |

Current Pico embedded-profile artifacts:

| Artifact | File size / section totals | Status |
|---|---:|---|
| `build/t089a-pico-embedded/src/libmuc_opcua.a` | 703,784 B file size | PASS; archive produced |
| `build/t089a-pico-embedded/platform/pico/pico_minimal_server.elf` | 1,014,188 B file size; `text=73,428`, `data=0`, `bss=119,340`, `dec=192,768` | PASS; ELF produced and measured |
| `build/t089a-pico-embedded/platform/pico/pico_minimal_server.uf2` | 138,752 B file size | PASS; UF2 produced |

Current RAM, heap, and stack evidence:

| Check | Current evidence | Status |
|---|---|---|
| Host/full archive static RAM | `.data=6,224 B`, `.bss=0 B`, flat against baseline; `nm -S --size-sort build/src/libmuc_opcua.a \| rg ' [BbDd] '` reports initialized data only and no BSS symbols. | PASS for archive static RAM |
| Caller-provided storage | Host/full `sizeof(struct mu_server)=116,040 B`, `MU_SERVER_STORAGE_BYTES=125,020 B`, `sizeof(mu_connection_t)=8,560 B`, `MU_CONNECTION_RX_BUFFER_SIZE=8,192 B`, `MU_CONNECTION_BASE_STORAGE_BYTES=1,328 B`; ARM embedded `sizeof(struct mu_server)=85,040 B`, `MU_SERVER_STORAGE_BYTES=98,520 B`, `sizeof(mu_connection_t)=9,480 B`; ARM nano `sizeof(struct mu_server)=664 B`, `MU_SERVER_STORAGE_BYTES=1,280 B`. | Absolute storage evidence recorded; no unbaselined delta is invented |
| Protocol hot-path heap | Source review found no allocator calls in `src/core`, `src/encoding`, or `src/services`; remaining matches are `mu_session_find_free` false positives or adapter cleanup hooks such as `cipher_ctx_free`. | PASS for the core protocol hot path |
| Host secured OPN stack | `scripts/check_stack_usage.sh --build-dir build/audit-embedded-stack --threshold 10240` reports `3,040 B`; threshold `10,240 B`. | PASS |
| Pico secured OPN stack | `scripts/check_stack_usage.sh --build-dir build/t089b-pico-stack --threshold 10240` reports `2,776 B` with 35 `.su` files; threshold `10,240 B`. | PASS |

Application-headroom conclusion for the active T102b refresh: the repository now
has current passing evidence for the host/full default flash budget, ARM
profile matrix, Pico build artifacts, archive static RAM, core protocol hot-path
heap claim, and host/Pico stack thresholds. T102c closes the source-ID ledger
gap separately in `docs/validation/audit-hardening-triage-ledger.md`.

### Historical Feature 020 US4 host/full audit-hardening evidence

Measured 2026-06-30 for `020-audit-hardening` task T088a before the T102b
blocker-remediation refresh. This is a host/full
profile proxy for the active `build` tree, not the ARM Cortex-M0+ matrix. It
records the current absolute host evidence and the available comparison against
`docs/size/audit-hardening-baseline.md`; it does not close the full resource
gate because embedded/nano/stack evidence is tracked by separate tasks.

Build/profile knobs from `build/CMakeCache.txt`:
`MUC_OPCUA_PROFILE=full`, `MUC_OPCUA_BUILD_TESTS=ON`,
`MUC_OPCUA_PLATFORM=host`, `MUC_OPCUA_OPTIMIZE_SIZE=OFF`,
`MUC_OPCUA_LTO=OFF`, `CMAKE_BUILD_TYPE=` empty, `CMAKE_C_FLAGS=` empty,
compiler `/usr/bin/cc`, full-profile services enabled, OpenSSL detected through
`MUC_OPCUA_HAVE_OPENSSL=1`, and capacity overrides
`MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
`MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`,
`MU_MAX_TRIGGER_LINKS=4`.

Commands and transcripts:

```sh
cmake --build build
./scripts/size-report.sh build/src/libmuc_opcua.a
size -t build/src/libmuc_opcua.a
nm -S --size-sort build/src/libmuc_opcua.a | rg ' [BbDd] '
rg -n 'malloc|calloc|realloc|free\(|OPENSSL_malloc' src include tests
```

- Size/build transcript: `/tmp/muc-opcua-size/t088a-host-full-size.log`
  (`cmake --build build` exit code `0`; `size-report.sh` exit code `0`).
- Totals transcript: `/tmp/muc-opcua-size/t088a-host-full-size-totals.log`
  (`size -t` exit code `0`; GNU binutils `size` 2.42).
- Data/BSS symbol transcript:
  `/tmp/muc-opcua-size/t088a-host-full-nm-data-bss.log` (`nm`/`rg` exit code
  `0`; GNU binutils `nm` 2.42).
- Heap-review transcript: `/tmp/muc-opcua-size/t088a-heap-grep.log` (`rg`
  exit code `0`).
- Caller-storage measurement helper was compiled from stdin with the same
  compile definitions as `build/src/libmuc_opcua.a`; build/run transcript:
  `/tmp/muc-opcua-size/t088a-sizeof-server.build.log`; output:
  `/tmp/muc-opcua-size/t088a-sizeof-server.run.log`; compile/run exit codes
  `0`/`0`.

T099b raw size-report evidence from T099a:

- Build transcript: `/tmp/muc-opcua-t099a/t099a-build.log`
  (`cmake --build build` exit code `0`; produced
  `build/src/libmuc_opcua.a`).
- Size-report transcript: `/tmp/muc-opcua-t099a/t099a-size-report.log`
  (`./scripts/size-report.sh build/src/libmuc_opcua.a` exit code `0`).
- `size-report.sh` emitted per-object rows only, not a totals row. The summed
  raw row values are `text=166,738 B`, `data=6,224 B`, `bss=0 B`,
  `dec=172,962 B`. This preserves the known host/full budget failure recorded
  below; T099c owns the formal budget comparison.

```text
command: ./scripts/size-report.sh build/src/libmuc_opcua.a
started: 2026-06-30T08:49:57+02:00
Size report for: build/src/libmuc_opcua.a
   text	   data	    bss	    dec	    hex	filename
  11243	      0	      0	  11243	   2beb	server.c.o (ex build/src/libmuc_opcua.a)
     32	      0	      0	     32	     20	status.c.o (ex build/src/libmuc_opcua.a)
   1966	      0	      0	   1966	    7ae	tcp_connection.c.o (ex build/src/libmuc_opcua.a)
   1718	      0	      0	   1718	    6b6	message_chunk.c.o (ex build/src/libmuc_opcua.a)
    309	      0	      0	    309	    135	sequence.c.o (ex build/src/libmuc_opcua.a)
    472	      0	      0	    472	    1d8	service_message.c.o (ex build/src/libmuc_opcua.a)
  45691	    816	      0	  46507	   b5ab	service_dispatch.c.o (ex build/src/libmuc_opcua.a)
   1022	      0	      0	   1022	    3fe	uasc.c.o (ex build/src/libmuc_opcua.a)
   1077	      0	      0	   1077	    435	secure_channel.c.o (ex build/src/libmuc_opcua.a)
   2808	      0	      0	   2808	    af8	session.c.o (ex build/src/libmuc_opcua.a)
    694	      0	      0	    694	    2b6	service_header.c.o (ex build/src/libmuc_opcua.a)
   3256	      0	      0	   3256	    cb8	discovery.c.o (ex build/src/libmuc_opcua.a)
     32	      0	      0	     32	     20	alarms_conditions.c.o (ex build/src/libmuc_opcua.a)
    531	      0	      0	    531	    213	address_space.c.o (ex build/src/libmuc_opcua.a)
   6080	   5408	      0	  11488	   2ce0	base_nodes.c.o (ex build/src/libmuc_opcua.a)
   2270	      0	      0	   2270	    8de	node_id.c.o (ex build/src/libmuc_opcua.a)
    529	      0	      0	    529	    211	value_source.c.o (ex build/src/libmuc_opcua.a)
   3118	      0	      0	   3118	    c2e	binary_reader.c.o (ex build/src/libmuc_opcua.a)
   2248	      0	      0	   2248	    8c8	binary_writer.c.o (ex build/src/libmuc_opcua.a)
    987	      0	      0	    987	    3db	binary_string.c.o (ex build/src/libmuc_opcua.a)
   1730	      0	      0	   1730	    6c2	binary_nodeid.c.o (ex build/src/libmuc_opcua.a)
   1138	      0	      0	   1138	    472	binary_extension_object.c.o (ex build/src/libmuc_opcua.a)
   2644	      0	      0	   2644	    a54	binary_variant.c.o (ex build/src/libmuc_opcua.a)
    941	      0	      0	    941	    3ad	binary_datavalue.c.o (ex build/src/libmuc_opcua.a)
   1200	      0	      0	   1200	    4b0	security_policy.c.o (ex build/src/libmuc_opcua.a)
     32	      0	      0	     32	     20	opcua_ids.c.o (ex build/src/libmuc_opcua.a)
   1965	      0	      0	   1965	    7ad	read.c.o (ex build/src/libmuc_opcua.a)
   4286	      0	      0	   4286	   10be	browse.c.o (ex build/src/libmuc_opcua.a)
  10937	      0	      0	  10937	   2ab9	node_management.c.o (ex build/src/libmuc_opcua.a)
   2281	      0	      0	   2281	    8e9	query.c.o (ex build/src/libmuc_opcua.a)
   3013	      0	      0	   3013	    bc5	binary_query.c.o (ex build/src/libmuc_opcua.a)
  21205	      0	      0	  21205	   52d5	subscription.c.o (ex build/src/libmuc_opcua.a)
    749	      0	      0	    749	    2ed	pubsub.c.o (ex build/src/libmuc_opcua.a)
    323	      0	      0	    323	    143	uadp_encoder.c.o (ex build/src/libmuc_opcua.a)
    596	      0	      0	    596	    254	host_udp_adapter.c.o (ex build/src/libmuc_opcua.a)
    710	      0	      0	    710	    2c6	write.c.o (ex build/src/libmuc_opcua.a)
   4416	      0	      0	   4416	   1140	history.c.o (ex build/src/libmuc_opcua.a)
   1219	      0	      0	   1219	    4c3	key_derivation.c.o (ex build/src/libmuc_opcua.a)
    492	      0	      0	    492	    1ec	certificate.c.o (ex build/src/libmuc_opcua.a)
   7051	      0	      0	   7051	   1b8b	asym_chunk.c.o (ex build/src/libmuc_opcua.a)
   4232	      0	      0	   4232	   1088	sym_chunk.c.o (ex build/src/libmuc_opcua.a)
    278	      0	      0	    278	    116	trustlist.c.o (ex build/src/libmuc_opcua.a)
   1636	      0	      0	   1636	    664	host_tcp_adapter.c.o (ex build/src/libmuc_opcua.a)
   7581	      0	      0	   7581	   1d9d	host_crypto_adapter.c.o (ex build/src/libmuc_opcua.a)
exit_code: 0
finished: 2026-06-30T08:49:57+02:00
```

T099c host/full budget comparison:

| Artifact | Current raw size-report totals | Baseline host archive | Delta | Plan budget | Budget result |
|---|---:|---:|---:|---|---|
| `build/src/libmuc_opcua.a` | text 166,738 B; data 6,224 B; bss 0 B; dec 172,962 B | text 152,709 B; data 6,224 B; bss 0 B | text +14,029 B; data +0 B; bss +0 B | Host/full `.text` growth under +8 KiB (`< 8,192 B`); no new default static RAM beyond existing storage | **FAIL**: text growth exceeds the host/full flash budget by 5,837 B. Data and BSS are flat, but that does not make the resource gate pass. |

T099c conclusion: host/full flash is over budget because `+14,029 B` is greater
than the `< 8,192 B` limit in `specs/020-audit-hardening/plan.md`. Archive
static RAM is flat against the available baseline (`data +0 B`, `bss +0 B`),
but this is only the static-RAM portion of the evidence. ARM/Pico/nano/embedded
size evidence remains blocked or incomplete as recorded below, so this ledger
does not claim SC-007, release resource-gate passage, or full Feature 020
resource passage.

T088a host/full archive comparison:

| Evidence | text | data | bss | dec | Baseline | Delta | Budget status |
|---|---:|---:|---:|---:|---:|---:|---|
| `build/src/libmuc_opcua.a` (`size -t`) | 166,738 B | 6,224 B | 0 B | 172,962 B | text 152,709 B; data 6,224 B; bss 0 B | text +14,029 B; data +0 B; bss +0 B | **FAIL for host/full +8 KiB text budget** |

RAM and heap evidence:

| Item | Current host/full value | Comparison status | Notes |
|---|---:|---|---|
| `sizeof(struct mu_server)` | 116,040 B | Baseline unavailable in `docs/size/audit-hardening-baseline.md` | Caller-provided storage, not archive `.bss`. |
| `MU_SERVER_STORAGE_BYTES` | 121,516 B | Baseline unavailable in `docs/size/audit-hardening-baseline.md` | Absolute full-profile host value for the active feature knobs. |
| Archive `.data` | 6,224 B | 0 B delta vs baseline | `nm` review shows existing lowercase `d` symbols such as `g_supported_services` and base-node arrays; no `.bss` symbols were found. |
| Archive `.bss` | 0 B | 0 B delta vs baseline | Supports the no-new-default-static-RAM check only for archive `.bss`; caller-storage deltas still need baseline evidence. |
| Protocol hot-path heap | No new core allocation proven by grep review | Advisory, not a release-gate pass | Matches are platform adapters, backend/test cleanup calls, OpenSSL allocation, and `mu_session_find_free` false positives; platform crypto/host adapters remain outside the no-hot-path-heap core claim. |

Resource risk: **High** for host flash because current host/full `.text` growth
is +14,029 B, exceeding the +8 KiB budget in
`specs/020-audit-hardening/plan.md`. Static RAM risk is **medium/incomplete**:
archive `.data`/`.bss` did not grow, but no audit-hardening baseline was
available for `sizeof(struct mu_server)` or `MU_SERVER_STORAGE_BYTES`, so this
entry records current absolute caller-storage measurements without inventing
deltas. Do not claim SC-007 or release-gate resource passage from this host-only
evidence.

### Historical Feature 020 US4 embedded/nano audit-hardening evidence

Measured-attempt 2026-06-30 for `020-audit-hardening` task T088b before the
T102b blocker-remediation refresh. This entry
targets the ARM Cortex-M0+ Thumb `-Os` profile matrix required by the active
feature's size gate: nano/embedded `.text` growth under +4 KiB, no new default
static RAM unless justified, no protocol hot-path heap, and release-gate honesty.
The current embedded/nano evidence is **blocked** because the ARM profile builds
fail before producing `libmuc_opcua.a` archives, so this entry records command
results and blockers rather than invented flash/RAM deltas.

Toolchain inventory:

- `arm-none-eabi-gcc (15:13.2.rel1-2) 13.2.1 20231009`
- `GNU size (2.42-1ubuntu1+23) 2.42`
- `cmake version 3.28.3`

Commands and transcripts:

```sh
BUILD_ROOT=build/audit-size-arm scripts/measure_size.sh all
BUILD_ROOT=build/audit-size-arm scripts/measure_size.sh nano
BUILD_ROOT=build/audit-size-arm scripts/measure_size.sh embedded
```

- Matrix transcript: `/tmp/muc-opcua-size/t088b-arm-size-matrix.log`
  (`measure_size.sh all` exit code `2`).
- Nano transcript: `/tmp/muc-opcua-size/t088b-arm-nano-size.log`
  (`measure_size.sh nano` exit code `2`).
- Embedded transcript: `/tmp/muc-opcua-size/t088b-arm-embedded-size.log`
  (`measure_size.sh embedded` exit code `2`).
- `find build/audit-size-arm -path '*/src/libmuc_opcua.a' -printf '%p\n'`
  produced no archive paths after these failed runs.

Build/profile knobs come from `scripts/measure_size.sh`: `CMAKE_SYSTEM_NAME=Generic`,
`CMAKE_C_COMPILER=arm-none-eabi-gcc`,
`CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`,
`CMAKE_C_FLAGS="-mcpu=cortex-m0plus -mthumb"`,
`MUC_OPCUA_PLATFORM=arduino-skeleton`,
`MUC_OPCUA_OPTIMIZE_SIZE=ON`, and per-profile
`MUC_OPCUA_PROFILE=nano` or `MUC_OPCUA_PROFILE=embedded`. The embedded run
also sets `MU_MAX_SUBSCRIPTIONS=2`, `MU_MAX_MONITORED_ITEMS=100`,
`MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`, and
`MU_MAX_TRIGGER_LINKS=4`.

Current ARM size result:

| Profile | Command result | text | data | bss | Delta/budget status |
|---|---|---:|---:|---:|---|
| nano | **BLOCKED**, compile fails before archive creation | N/A | N/A | N/A | Cannot compare to +4 KiB budget. |
| embedded | **BLOCKED**, compile fails before archive creation | N/A | N/A | N/A | Cannot compare to +4 KiB budget. |

Observed blockers:

- Nano: `src/services/discovery.c:142:29: error: variable 'policy' set but not used [-Werror=unused-but-set-variable]`.
- Embedded: `src/core/server_internal.h:45:1: error: static assertion failed: "MU_CONNECTION_BASE_STORAGE_BYTES must cover mu_connection_t fields outside rx_buffer"`.

RAM and heap evidence status:

| Item | Current evidence | Status |
|---|---|---|
| Archive `.data` / `.bss` for nano and embedded | No current ARM archive was produced. | **Blocked**; cannot claim no-new-default-static-RAM for nano/embedded from this task. |
| `MU_SERVER_STORAGE_BYTES` / `sizeof(struct mu_server)` nano and embedded deltas | No audit-hardening pre-change ARM baseline exists in `docs/size/audit-hardening-baseline.md`, and current ARM builds fail before measurement. | **Blocked/incomplete**; record no deltas. |
| Protocol hot-path heap | No new ARM-specific heap evidence from T088b. T088a's host grep remains advisory source-level evidence only. | **Incomplete** for embedded release-gate purposes. |

Resource risk: **High/incomplete** for nano and embedded release gating. The
required embedded/nano flash/RAM evidence is unavailable until the ARM profile
build blockers above are fixed and the profile matrix can produce current
absolute `text`/`data`/`bss` rows. Do not claim SC-007 or release-gate resource
passage from this T088b evidence.

### Historical Feature 020 US4 static RAM data/bss audit-hardening evidence

Measured 2026-06-30 for `020-audit-hardening` task T088c before the T102b
blocker-remediation refresh. This entry focuses
only on archive `.data` and `.bss` static RAM evidence for the active
audit-hardening budget: no new default static RAM unless justified, no
protocol hot-path heap, and no release-gate resource claim without current
evidence across the required artifacts.

Current host/full artifact evidence:

| Artifact | Command | Result | Current `.data` | Current `.bss` | Baseline comparison | Transcript |
|---|---|---|---:|---:|---|---|
| `build/src/libmuc_opcua.a` | `size -t build/src/libmuc_opcua.a` | exit code `0`; GNU `size` 2.42; totals row `text=166738`, `data=6224`, `bss=0`, `dec=172962` | 6,224 B | 0 B | `docs/size/audit-hardening-baseline.md` records `data=6224`, `bss=0`; delta `data +0 B`, `bss +0 B` | `/tmp/muc-opcua-size/t088c-host-full-size-totals.log` |
| `build/src/libmuc_opcua.a` | `nm -S --size-sort build/src/libmuc_opcua.a \| rg ' [BbDd] '` | exit code `0`; GNU `nm` 2.42; found lowercase `d` symbols only, no `B`/`b` symbols | Existing initialized data symbols only | No BSS symbols reported | Supports the host/full archive static-RAM comparison above; does not measure caller-provided `struct mu_server` storage | `/tmp/muc-opcua-size/t088c-host-full-nm-data-bss.log` |

Host/full symbol review: the `.data` matches are existing initialized-data
symbols: `g_supported_services` (`0x330` bytes), four 16-byte server array
descriptors, four 56-byte array values, `s_base_space` (`0x10` bytes), and
`s_base_nodes` (`0x13f0` bytes). No `.bss` symbols were reported by the
host/full `nm` review. This supports the host/full archive portion of the
no-new-default-static-RAM check, but it does not prove the full static-RAM gate
because caller-provided storage and embedded/nano profile artifacts still need
current evidence.

Current embedded/nano status:

| Artifact set | Command | Result | `.data` / `.bss` status | Transcript |
|---|---|---|---|---|
| Current T088b ARM audit build root `build/audit-size-arm` | `find build/audit-size-arm -path '*/src/libmuc_opcua.a' -printf '%p\n'` | exit code `0`; no archive paths printed | **Blocked**: no current nano/embedded archives exist to measure | `/tmp/muc-opcua-size/t088c-audit-arm-archive-find.log` |
| Pre-existing ARM reference archives under `build/size-arm` | `size -t build/size-arm/nano/src/libmuc_opcua.a`; `size -t build/size-arm/embedded/src/libmuc_opcua.a` | exit code `0`; nano totals `data=0`, `bss=0`; embedded totals `data=0`, `bss=0` | Reference only; archive mtimes are 2026-06-29, before the current T088b build attempts, so these are not current audit-hardening release evidence | `/tmp/muc-opcua-size/t088c-preexisting-arm-summary.log` |
| Pre-existing ARM reference archives under `build/size-arm` | `nm -S --size-sort build/size-arm/nano/src/libmuc_opcua.a build/size-arm/embedded/src/libmuc_opcua.a \| rg ' [BbDd] '` | exit code `1`; no matches | Reference only; no `.data`/`.bss` symbols found in stale nano/embedded archives | `/tmp/muc-opcua-size/t088c-preexisting-arm-nm-data-bss.log` |

The current embedded/nano `.data`/`.bss` gate remains **blocked/incomplete** for
the same reasons documented in T088b: `BUILD_ROOT=build/audit-size-arm
scripts/measure_size.sh nano` fails on the unused `policy` variable in
`src/services/discovery.c`, and `BUILD_ROOT=build/audit-size-arm
scripts/measure_size.sh embedded` fails the
`MU_CONNECTION_BASE_STORAGE_BYTES` static assertion in
`src/core/server_internal.h`. Do not use the pre-existing `build/size-arm`
archives to claim release-gate passage for `020-audit-hardening`; they only show
that older ARM reference archives had no archive `.data` or `.bss`.

Static RAM conclusion for T088c: host/full archive `.data` and `.bss` are flat
against the available pre-change baseline (`data +0 B`, `bss +0 B`), and the
fresh host/full symbol review found no `.bss`. The full no-new-default-static-RAM
release gate is **not passed** by this evidence because current embedded/nano
archives are unavailable and caller-provided storage deltas are not baselined in
`docs/size/audit-hardening-baseline.md`.

### Historical Feature 020 US4 stack audit-hardening evidence

Measured 2026-06-30 for `020-audit-hardening` task T088d before the T102b
blocker-remediation refresh. This entry records
only the secured OpenSecureChannel stack evidence required by the active
feature's resource budget and release-gate honesty rules. The stack threshold
check passes, but this does **not** claim SC-007 or full resource-gate passage:
T088a records host/full flash over the +8 KiB budget, and T088b/T088c record
current embedded/nano size and static-RAM evidence as blocked or incomplete.

Build/profile knobs from `build/audit-embedded-stack/CMakeCache.txt`:
`MUC_OPCUA_PROFILE=embedded`, `MUC_OPCUA_OPTIMIZE_SIZE=ON`, `MUC_OPCUA_STACK_USAGE=ON`,
`MUC_OPCUA_STACK_USAGE_LIMIT=10240`, `MUC_OPCUA_PLATFORM=host`,
`CMAKE_BUILD_TYPE=Release`, compiler `/usr/bin/cc`, `MUC_OPCUA_SECURITY=ON`,
`MUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON`, `MUC_OPCUA_MULTIPLE_CONNECTIONS=ON`,
and capacity overrides `MU_MAX_MONITORED_ITEMS=100`,
`MU_MAX_SUBSCRIPTIONS=2`, `MU_MAX_PUBLISH_REQUESTS=5`,
`MU_MONITORED_QUEUE_DEPTH=2`, `MU_MAX_TRIGGER_LINKS=4`.

Commands and transcripts:

```sh
cmake --build build/audit-embedded-stack
bash scripts/check_stack_usage.sh --build-dir build/audit-embedded-stack --threshold 10240
find build/audit-embedded-stack -type f -name '*.su' | wc -l
```

- Build transcript: `/tmp/muc-opcua-size/t088d-stack-build.log`
  (`cmake --build build/audit-embedded-stack` exit code `0`; produced
  `build/audit-embedded-stack/src/libmuc_opcua.a`).
- Stack-check transcript: `/tmp/muc-opcua-size/t088d-stack-check.log`
  (`check_stack_usage.sh` exit code `0`).
- Stack-usage artifacts: `37` `.su` files found under
  `build/audit-embedded-stack`.

T100b refresh evidence from the T100a rerun:

- Build transcript: `/tmp/muc-opcua-t100a/t100a-stack-build.log`
  (`cmake --build build/audit-embedded-stack` exit code `0`; produced
  `build/audit-embedded-stack/src/libmuc_opcua.a`).
- Stack-check transcript: `/tmp/muc-opcua-t100a/t100a-stack-check.log`
  (`check_stack_usage.sh` exit code `0`; secured OPN stack estimate
  `3040 bytes`; threshold `10240 bytes`; stack-specific check passed).
- Stack-usage count transcript:
  `/tmp/muc-opcua-t100a/t100a-su-count.log` (`find ... '*.su' | wc -l`
  exit code `0`; `37` `.su` files found under `build/audit-embedded-stack`).

Stack comparison:

| Evidence | Current estimate | Baseline | Delta | Threshold | Result |
|---|---:|---:|---:|---:|---|
| Secured OPN stack estimate | 3,040 B | 3,040 B | +0 B | 10,240 B | **PASS for stack threshold** |
| T100a rerun secured OPN stack estimate | 3,040 B | 3,040 B | +0 B | 10,240 B | **PASS for host stack-specific threshold only** |

Frame and phase summary from `scripts/check_stack_usage.sh`:

| Frame / phase | Bytes |
|---|---:|
| `src/core/server.c:handle_data_chunk_secure` | 0 B (not emitted) |
| `src/security/asym_chunk.c:mu_asym_chunk_unwrap` | 304 B |
| `src/security/asym_chunk.c:mu_asym_chunk_wrap` | 2,912 B |
| `src/security/asym_chunk.c:max_helper` | 128 B |
| `src/core/service_dispatch.c:mu_service_dispatch` | 176 B |
| `src/core/service_dispatch.c:handle_open_secure_channel` | 256 B |
| `src/services/secure_channel.c:mu_secure_channel_open` | 48 B |
| `src/security/sym_chunk.c:mu_sym_keys_derive` | 272 B |
| `src/security/key_derivation.c:mu_p_sha256` | 288 B |
| asymmetric unwrap/wrap phase | 3,040 B |
| OpenSecureChannel dispatch/KDF phase | 992 B |

Historical T088d stack conclusion: the stack-recording command passed and was
flat against the pre-change `3040 bytes` baseline in
`docs/size/audit-hardening-baseline.md`, but that pre-remediation snapshot did
not clear the full resource gate because other T088/T089 rows were still
failing or blocked. The T102b blocker-remediation refresh above supersedes this
historical status.

### Feature 020 US4 application-headroom summary

Updated 2026-06-30 for the T102b blocker-remediation refresh. The active
application-headroom budget comes from `specs/020-audit-hardening/plan.md` and
`specs/020-audit-hardening/spec.md`: host/full `.text` growth under +8 KiB,
nano/embedded `.text` growth under +4 KiB, no new default static RAM beyond
existing server/session/channel storage, no heap use on the protocol hot path,
and recorded stack evidence for hardening changes.

| Budget item | Current evidence source | Budget evaluation | Resource risk |
|---|---|---|---|
| Host/full `.text` growth under +8 KiB | T102b: default full build with `MUC_OPCUA_OPTIMIZE_SIZE=ON` reports `text=96,398 B` vs baseline `152,709 B`. | **PASS**: delta is `-56,311 B`, below the `< 8,192 B` growth budget. | Low for the default constrained-device build; the explicit `OPTIMIZE_SIZE=OFF` opt-out remains over budget as reference-only evidence. |
| Nano `.text` growth under +4 KiB | T102b ARM matrix: nano `text=16,366 B`, `data=0`, `bss=0`. | **PASS / current value recorded**: blocker cleared and size remains below the historical nano value in this ledger. | Low for current measured nano archive. |
| Embedded `.text` growth under +4 KiB | T102b ARM matrix: embedded `text=43,078 B`, `data=0`, `bss=0`; Pico embedded artifacts also build. | **PASS / current value recorded**: blocker cleared and embedded/Pico artifacts exist. | Low for current measured embedded archive and Pico build presence. |
| No new default static RAM | Host/full archive `.data=6,224 B`, `.bss=0 B`; ARM archives `.data=0`, `.bss=0`; caller-storage absolutes recorded with `MU_CONNECTION_BASE_STORAGE_BYTES=1,328 B`. | **PASS / absolute caller-storage evidence recorded**: archive static RAM is flat, and storage requirements are explicit. | Low for archive static RAM; caller storage is documented as integrator-provided memory. |
| No protocol hot-path heap | T102b source review found no allocator calls in `src/core`, `src/encoding`, or `src/services`; adapter cleanup hooks remain outside the core hot-path claim. | **PASS for the core protocol hot path**. | Low for core protocol hot path; third-party backend internals remain adapter-specific. |
| Stack-recording budget | Host secured OPN stack `3,040 B`; Pico secured OPN stack `2,776 B`; threshold `10,240 B`. | **PASS**: both measured stack estimates are below threshold. | Low for measured secured OPN stack path. |

Application-headroom conclusion for the active refresh: the Feature 020
repository-fixable resource blocker rows are closed by current evidence. This
does not supply external CTT evidence. T102c closes the source-ID ledger gap
separately in `docs/validation/audit-hardening-triage-ledger.md`.

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
`MUC_OPCUA_SECURITY`). Worst-case secured-OPN stack measured by
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
gating `mu_status_name` strings behind `MUC_OPCUA_STATUS_STRINGS`. Measured Micro
core `.text` (host `gcc -Os`, `libmuc_opcua.a`, subscriptions on / security off):
**44,561 B → 42,232 B = −2,329 B (~5.2%)** vs the post-US3 core. Net vs the
pre-remediation baseline (`1f84905`, 43,084 B) is **−852 B (~2%)** — US1–US3 added
parser-robustness / address-space-index / perf code that offsets part of the US4
reduction. NOTE: SC-005's ≥8% target is **not fully met** (US4 isolated ~5.2%); the
sticky-status conversion was applied conservatively (≈48 of ~294 call sites, to
guarantee byte-identical output), leaving further flash on the table, and opt-in
`MUC_OPCUA_LTO` (default OFF) is available for additional reduction. Figures are a
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

## Feature 021 US3 aggregate-slice size evidence

- **Measured**: 2026-06-30
- **Command**: `scripts/measure_size.sh all`
- **Toolchain**: `arm-none-eabi-gcc` 13.2.1 for RP2040/Cortex-M0+ Thumb `-Os`

| Profile | text | data | bss | dec | Archive |
|---|---:|---:|---:|---:|---|
| nano | 16,366 B | 0 B | 0 B | 16,366 B | `build/size-arm/nano/src/libmuc_opcua.a` |
| micro | 23,873 B | 0 B | 0 B | 23,873 B | `build/size-arm/micro/src/libmuc_opcua.a` |
| embedded | 43,078 B | 0 B | 0 B | 43,078 B | `build/size-arm/embedded/src/libmuc_opcua.a` |
| full-featured | 51,172 B | 0 B | 0 B | 51,172 B | `build/size-arm/full-featured/src/libmuc_opcua.a` |

Aggregate-slice conclusion: T017a-T017e had no production-code delta in this
worktree; the aggregate updates are host tests and documentation evidence only.
The runtime flash/RAM delta attributable to the aggregate slice is therefore
`text +0 B`, `.data +0 B`, `.bss +0 B`. No new heap use, mutable static storage,
or runtime static tables were added. The table above records current absolute
profile outputs from the size script, not aggregate-slice runtime growth.

## Feature 021 US4 subscription-negative-path size evidence

- **Measured**: 2026-06-30
- **Command**: `scripts/measure_size.sh all`
- **Toolchain**: `arm-none-eabi-gcc` 13.2.1 for RP2040/Cortex-M0+ Thumb `-Os`

| Profile | text | data | bss | dec | Archive |
|---|---:|---:|---:|---:|---|
| nano | 16,366 B | 0 B | 0 B | 16,366 B | `build/size-arm/nano/src/libmuc_opcua.a` |
| micro | 23,873 B | 0 B | 0 B | 23,873 B | `build/size-arm/micro/src/libmuc_opcua.a` |
| embedded | 43,078 B | 0 B | 0 B | 43,078 B | `build/size-arm/embedded/src/libmuc_opcua.a` |
| full-featured | 51,172 B | 0 B | 0 B | 51,172 B | `build/size-arm/full-featured/src/libmuc_opcua.a` |

Subscription-negative-path conclusion: US4 added negative-path tests and
traceability documentation for MonitoredItem/Subscription invalid IDs,
capacities, queue overflow, trigger-link capacity, invalid Publish
acknowledgement, invalid Republish sequence, and unsupported
TransferSubscriptions. T024a-T025 production validation found no production-code
changes were needed because the production paths were already present. The
runtime flash/RAM delta attributable to the US4 implementation is therefore
`text +0 B`, `.data +0 B`, `.bss +0 B`.

No new heap use, runtime static tables, mutable static storage, or
caller-storage changes are attributable to US4. Throughput impact is also
`+0` for runtime paths: only tests and documentation changed, so the
negative-path tests increase CI/test coverage without changing runtime
throughput. The table above records current absolute profile outputs from the
size script, not subscription-negative-path runtime growth.

## Feature 012 PubSub hardening resource evidence

- **Measured**: 2026-06-30
- **Commands**:
  - `cmake --build build/pubsub-hardening -j$(nproc)`
  - `ctest --test-dir build/pubsub-hardening --output-on-failure`
  - `cmake --build build/pubsub-hardening --target format-check`
  - `cmake --build build/pubsub-hardening --target cppcheck`
  - `make all-profiles`
  - `BUILD_ROOT=build/pubsub-size-arm scripts/measure_size.sh all`
  - `cmake -S . -B build/pubsub-pico -DMUC_OPCUA_PLATFORM=pico -DPICO_SDK_FETCH_FROM_GIT=ON -DMUC_OPCUA_BUILD_EXAMPLES=ON -DMUC_OPCUA_PROFILE=embedded -DMUC_OPCUA_OPTIMIZE_SIZE=ON -DMU_MAX_SUBSCRIPTIONS=2 -DMU_MAX_MONITORED_ITEMS=100 -DMU_MAX_PUBLISH_REQUESTS=5 -DMU_MONITORED_QUEUE_DEPTH=2 -DMU_MAX_TRIGGER_LINKS=4`
  - `cmake --build build/pubsub-pico -j$(nproc)`
  - `make speed-compare`

Current ARM Cortex-M0+ profile matrix:

| Profile | text | data | bss | dec | Archive |
|---|---:|---:|---:|---:|---|
| nano | 16,278 B | 0 B | 0 B | 16,278 B | `build/pubsub-size-arm/nano/src/libmuc_opcua.a` |
| micro | 23,785 B | 0 B | 0 B | 23,785 B | `build/pubsub-size-arm/micro/src/libmuc_opcua.a` |
| embedded | 42,990 B | 0 B | 0 B | 42,990 B | `build/pubsub-size-arm/embedded/src/libmuc_opcua.a` |
| full-featured | 51,248 B | 0 B | 0 B | 51,248 B | `build/pubsub-size-arm/full-featured/src/libmuc_opcua.a` |

PubSub object budget for the full-featured ARM build:

| Object | text | data | bss | Budget result |
|---|---:|---:|---:|---|
| `encoding/uadp_encoder.c.obj` | 252 B | 0 B | 0 B | PASS |
| `core/pubsub.c.obj` | 280 B | 0 B | 0 B | PASS |
| combined | 532 B | 0 B | 0 B | PASS: below 3 KiB flash and below 200 B archive RAM |

Host profile archive totals after `make all-profiles` plus a full-profile examples
build:

| Profile | text | data | bss | dec |
|---|---:|---:|---:|---:|
| nano | 33,604 B | 264 B | 0 B | 33,868 B |
| micro | 46,568 B | 528 B | 0 B | 47,096 B |
| embedded | 80,023 B | 5,984 B | 0 B | 86,007 B |
| full | 96,190 B | 6,224 B | 0 B | 102,414 B |

Full-profile host example binaries:

| Artifact | text | data | bss | dec |
|---|---:|---:|---:|---:|
| `build/full/examples/minimal_server` | 109,997 B | 8,292 B | 141,616 B | 259,905 B |
| `build/full/examples/pubsub_server` | 97,621 B | 7,200 B | 141,440 B | 246,261 B |

Pico embedded-profile cross-compile:

| Artifact | text | data | bss | dec | Status |
|---|---:|---:|---:|---:|---|
| `build/pubsub-pico/platform/pico/pico_minimal_server.elf` | 73,292 B | 0 B | 119,340 B | 192,632 B | PASS |

Caller-storage check for the full-profile host build with embedded capacity
overrides: `sizeof(struct mu_server)=120,896 B`,
`MU_SERVER_STORAGE_BYTES=125,980 B`, and
`sizeof(mu_pubsub_writer_group_t)=40 B`; the storage macro remains above the
actual server struct size. Published field values are caller-owned and do not
add fixed server-side field storage.

Controlled speed comparison (`make speed-compare`) passed on isolated CPU 11
with realtime scheduling and CPU shielding. All 66 paired rows passed the
configured gates. Median normalized ratios versus `docs/benchmarks/speed-baseline.json`
were nano `0.994`, micro `1.003`, embedded `1.004`, and full `1.005`; the worst
row was full `subscription-active-tick` at `0.937`, above the `0.85` gate.

## Feature 023 US3 PubSub subscriber resource-impact evidence

- **Measured**: 2026-07-01 for Feature 023 T037a
  (`023-conformance-docs-subscriber`, scoped UADP/UDP subscriber decode)
- **Command**: `scripts/measure_size.sh all`
- **Exit code**: `0`
- **Toolchain**: `arm-none-eabi-gcc` 13.2.1, `GNU size` 2.42, `cmake` 3.28.3
- **Plan budget context**: `specs/023-conformance-docs-subscriber/plan.md`
  targets less than 2 KiB added Arm Cortex-M0+ `.text` for the subscriber
  decoder, no mandatory static RAM, caller-provided decode output slots, no heap,
  and added peak stack below 256 B.

Current ARM Cortex-M0+ profile matrix:

| Profile | text | data | bss | dec | Archive |
|---|---:|---:|---:|---:|---|
| nano | 16,278 B | 0 B | 0 B | 16,278 B | `build/size-arm/nano/src/libmuc_opcua.a` |
| micro | 23,785 B | 0 B | 0 B | 23,785 B | `build/size-arm/micro/src/libmuc_opcua.a` |
| embedded | 42,990 B | 0 B | 0 B | 42,990 B | `build/size-arm/embedded/src/libmuc_opcua.a` |
| full-featured | 51,612 B | 0 B | 0 B | 51,612 B | `build/size-arm/full-featured/src/libmuc_opcua.a` |

Delta versus the most recent ARM matrix already recorded in this ledger:

| Profile | `.text` delta | `.data` delta | `.bss` delta |
|---|---:|---:|---:|
| nano | -88 B | +0 B | +0 B |
| micro | -88 B | +0 B | +0 B |
| embedded | -88 B | +0 B | +0 B |
| full-featured | +440 B | +0 B | +0 B |

Resource-risk conclusion: the measured archive `.data` and `.bss` remain 0 B for
all profiles. The measured `.text` movement is flat-to-down for nano, micro, and
embedded, and the full-featured archive grows by 440 B, which is within the
Feature 023 subscriber decoder flash target. This entry records the T037a ARM
size evidence only; it does not claim external CTT/profile compliance, full
release-gate passage, stack-budget proof, or heap proof beyond the archive
section results above.

### Feature 023 T047 final size validation

- **Measured**: 2026-07-01 for Feature 023 T047 after T041-T043 documentation
  and test-evidence work.
- **Command**: `scripts/measure_size.sh all`
- **Exit code**: `0`
- **Result**: **PASS**. The current ARM Cortex-M0+ archive matrix is unchanged
  from the T037a/T037b Feature 023 entry above, so the post-implementation
  documentation/test-evidence tasks did not add runtime size. Archive `.data`
  and `.bss` remain 0 B for every profile, matching the plan's no mandatory
  static RAM expectation.

Command output:

```text
profile              text     data      bss        dec archive
nano           16278        0        0      16278 build/size-arm/nano/src/libmuc_opcua.a
micro          23785        0        0      23785 build/size-arm/micro/src/libmuc_opcua.a
embedded       42990        0        0      42990 build/size-arm/embedded/src/libmuc_opcua.a
full-featured      51612        0        0      51612 build/size-arm/full-featured/src/libmuc_opcua.a
```

Current ARM Cortex-M0+ profile matrix:

| Profile | text | data | bss | dec | Archive |
|---|---:|---:|---:|---:|---|
| nano | 16,278 B | 0 B | 0 B | 16,278 B | `build/size-arm/nano/src/libmuc_opcua.a` |
| micro | 23,785 B | 0 B | 0 B | 23,785 B | `build/size-arm/micro/src/libmuc_opcua.a` |
| embedded | 42,990 B | 0 B | 0 B | 42,990 B | `build/size-arm/embedded/src/libmuc_opcua.a` |
| full-featured | 51,612 B | 0 B | 0 B | 51,612 B | `build/size-arm/full-featured/src/libmuc_opcua.a` |

Budget comparison:

| Budget item | Current T047 evidence | Status |
|---|---|---|
| Scoped UADP subscriber decoder target less than 2 KiB added Arm Cortex-M0+ `.text` | The T037a/T037b delta remains nano `-88 B`, micro `-88 B`, embedded `-88 B`, and full-featured `+440 B`; the T047 matrix is identical to T037a/T037b. | PASS |
| No mandatory static RAM | Every measured archive reports `.data=0 B` and `.bss=0 B`. | PASS |
| Caller-provided output slots and no heap | No runtime source changes occurred after T037a/T037b for T041-T043; T047 records unchanged archive sections after documentation/test-evidence work. | PASS |

### Feature 024 (rename to muc-opcua) T001 pre-rename baseline

- **Measured**: 2026-07-01 for Feature 024 T001, immediately before any
  rename work landed, as the reference point T012/T033 diff against.
- **Command**: `scripts/measure_size.sh all`
- **Result**: Identical to the T047 matrix directly above (as expected — no
  code changed between T047 and this baseline). Reproduced here as an
  explicit pre-024 checkpoint:

```text
profile              text     data      bss        dec archive
nano           16278        0        0      16278 build/size-arm/nano/src/libmuc_opcua.a
micro          23785        0        0      23785 build/size-arm/micro/src/libmuc_opcua.a
embedded       42990        0        0      42990 build/size-arm/embedded/src/libmuc_opcua.a
full-featured      51612        0        0      51612 build/size-arm/full-featured/src/libmuc_opcua.a
```

Feature 024 is a pure identifier rename (`MUC_OPCUA_*` -> `MUC_OPCUA_*`,
`include/muc_opcua/`, `muc_opcua` CMake/target names).

### Feature 024 T012 post-rename measurement

- **Measured**: 2026-07-01 for Feature 024 T012, after Phase 3 (User Story 1:
  header/CMake relocation and every `#include`/macro reference update across
  `src/`, `tests/`, `platform/`, `examples/`).
- **Command**: `scripts/measure_size.sh all`

```text
profile              text     data      bss        dec archive
nano           16278        0        0      16278 build/size-arm/nano/src/libmuc_opcua.a
micro          23785        0        0      23785 build/size-arm/micro/src/libmuc_opcua.a
embedded       42988        0        0      42988 build/size-arm/embedded/src/libmuc_opcua.a
full-featured      51610        0        0      51610 build/size-arm/full-featured/src/libmuc_opcua.a
```

- **nano/micro**: byte-identical to the T001 baseline (16,278 B / 23,785 B).
- **embedded/full-featured**: **-2 B** each (42,990 B -> 42,988 B; 51,612 B ->
  51,610 B). This is not a regression or unexplained drift: the Base Info Type
  System's `ServerArray` value in `src/address_space/base_nodes.c` (compiled
  only when `MUC_OPCUA_BASE_TYPE_SYSTEM` is on, i.e. embedded/full-featured
  only) is the literal string `"urn:muc-opcua:server"`, two ASCII characters
  shorter than the pre-rename `"urn:muc-opcua:server"`. Renaming the
  identifier necessarily renames this OPC UA `ServerArray` wire value too
  (it's the server's own application URI, not an arbitrary label), and a
  shorter string is a smaller compiled `.rodata`/`.text` footprint. No
  behavior, encoding, or conformance change resulted — the `mu_string_t`
  length field was updated to match (`{22, ...}` -> `{20, ...}`; the
  `examples/minimal_server` `NamespaceArray`/`ServerArray` URNs were fixed the
  same way, `{40, ...}` -> `{38, ...}`) so the wire-encoded String length is
   exactly correct.

### Feature 025 (audit remediation) post-completion measurement

- **Measured**: 2026-07-02 for Feature 025 completion. Handlers kept `static` in `service_dispatch.c` per project size constraint.
- **Command**: `scripts/measure_size.sh all`

```text
profile              text     data      bss        dec archive
nano           16436        0        0      16436 build/size-arm/nano/src/libmuc_opcua.a
micro          23839        0        0      23839 build/size-arm/micro/src/libmuc_opcua.a
embedded       44100        0        0      44100 build/size-arm/embedded/src/libmuc_opcua.a
full-featured      52822        0        0      52822 build/size-arm/full-featured/src/libmuc_opcua.a
```

**Delta vs previous (Feature 024 T012):**

| Profile | Previous | Current | Delta | Notes |
|---|---|---|---|---|
| nano | 16,278 | 16,436 | +158 | Gate fixes: ServerCertificate branch now behind `#ifdef MUC_OPCUA_SECURITY` |
| micro | 23,785 | 23,839 | +54 | Gate fixes: RSA OAEP decrypt behind `#ifdef MUC_OPCUA_SECURITY`; security scratch macros gated |
| embedded | 42,988 | 44,100 | +1,112 | Session scratch (+2KB), certificate trust hooks, ClientSignature verification |
| full-featured | 51,610 | 52,822 | +1,212 | Embedded delta + full-featured overhead |

**Key contributors to growth:**
- `MU_SECURE_SCRATCH_SIZE`: 12,288 → 14,336 (+2,048 B) for session handshake buffers (embedded/full only)
- Certificate validity hook call + trust-list fail-closed enforcement (embedded/full only)
- Integer deadband branching in subscription hot path
- Unconditional username-token ServerNonce anti-replay check
- `.data = 0`, `.bss = 0` across all profiles (no-heap invariant preserved)

### Feature 031 (deferred audit fixes) post-completion measurement

- **Measured**: 2026-07-04 for Feature 031 completion.
- **Command**: `BUILD_ROOT=build/size-031 scripts/measure_size.sh all`

```text
profile              text     data      bss        dec archive
nano           16520        0        0      16520 build/size-031/nano/src/libmuc_opcua.a
micro          24203        0        0      24203 build/size-031/micro/src/libmuc_opcua.a
embedded       44764        0        0      44764 build/size-031/embedded/src/libmuc_opcua.a
full-featured      53514        0        0      53514 build/size-031/full-featured/src/libmuc_opcua.a
```

**Delta vs previous (Feature 025):**

| Profile | Previous | Current | Delta | Notes |
|---|---|---|---|---|
| nano | 16,436 | 16,520 | +84 | Binary search dispatch, session create cleanup path, cert token ifdef consolidation (service_dispatch.c shared across all profiles) |
| micro | 23,839 | 24,203 | +364 | Nano delta + bitmap publish scan, deadband NONE fix, publish timer bound, 64-bit div avoidance (subscription.c) |
| embedded | 44,100 | 44,764 | +664 | Micro delta + nonce zeroization, cert token verification path |
| full-featured | 52,822 | 53,514 | +692 | Embedded delta + profile URI cache, triggered-items scan |

**Key contributors to growth:**
- Bitmap publish scan (T003): ~+100 B flash, ~+32 B RAM (struct field in `mu_subscriptions_t`)
- Binary search dispatch (T013): ~+50 B flash (binary search code in shared service_dispatch.c)
- Session create cleanup path (T010): ~+30 B flash (cleanup label + session_close call)
- Nonce zeroization (T012): ~+24 B flash (two `mu_secure_zero` calls)
- Publish timer bound (T007): ~+12 B flash
- Service_dispatch.c shared growth: ~+145 B across all profiles (binary search + cleanup + ifdef)
- **`<math.h>` removed from subscription.c** — double-precision software emulation library (~12 KB) no longer linked for profiles with subscriptions. The net effect is that the code-size additions above are partially offset for micro/embedded/full, keeping `.text` growth modest.
- `.data = 0`, `.bss = 0`, heap = 0 across all profiles (no-heap invariant preserved).

## 2026-07-16: Spec 074 — Server-Emitted, Client-Observable AuditEvents

Made auditing conformant (emit + observe over OPC UA), gated by
`MUC_OPCUA_CU_AUDITING` (defaults on for `full` only; 0 on all other profiles).

**RAM** (auditing-enabled profiles only): shared static audit-payload pool
`audit_pool[16]` = 16 × 336 B = **5.2 KB**, plus an 8-byte `audit_ref` per queued
event (8 × MU_MAX_EVENT_QUEUE_SIZE 8 × MU_INTERN_MAX_SUBSCRIPTIONS 100 ≈ 6.4 KB on
full) → **~11.6 KB on full**. This is **~23× smaller** than embedding the full
audit payload in every queued event (~268 KB); the pool design (research.md
Decision 7) is the reason the full-field path is affordable. Flash: audit→event
adapter + SELECT resolver/parser + 4 emitters, ~1–2 KB, auditing-only. Non-auditing
profiles (nano/micro/embedded/standard): **0** flash + RAM (compiled out).

## 2026-07-16: Spec 075 — Nano Time-Sync (configurable clock skew)

Turned nano's last unreconciled *required* CU (`opc_cu_5793` Time Sync – Support)
into a claimable feature: retyped the OpenSecureChannel timestamp-freshness skew
knob from a compile-time `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` constant to a runtime
`mu_server_config_t.acceptable_clock_skew_ns` field (nanoseconds), compared in the
native 100 ns DateTime domain so a host-class (x86_64/aarch64) deployment reaches
sub-microsecond (IEEE-802.1AS / PTP) precision. No sync protocol is added — the
disciplined clock is supplied by the integrator via `time_adapter.get_time`.

Gated by `MUC_OPCUA_CU_TIME_SYNC` (now on for all profiles incl. nano). Cost is a
single `opcua_uint64_t` config field + the ns-scaling arithmetic at the one OPN
call site; no address-space nodes. `scripts/measure_size.sh` (ARM Cortex-M0+,
`-Os`), nano archive `.text` / elf_text:

| Profile | before | after | Δ elf |
|----------|------------------:|------------------:|------:|
| nano | 23,568 / 21,320 | 23,660 / 21,524 | +204 |

**RAM**: +8 B (`.data`/`.bss` unchanged — the field lives in caller-owned config
storage; no-heap invariant preserved). Reconciled `opc_cu_5793`/`2478` (OS-based)/
`2786` (NTP)/`3802` (Configure Clock Skew) via `satisfied_by`; `2479` (PTP)/`2480`
(802.1AS)/`5505` (UA-based) remain unclaimed (integrator's protocol, not ours).
Nano reaches **18/18 required** CUs.

## 2026-07-16: Spec 073 long tail — DataChange Subscription facet completion (CUs 5208/3911/4055/3922)

Completed the Standard DataChange Subscription 2022 Server Facet (opc_id 1324) to
**16/16 required** by implementing its last four required CUs, all gated on
`MUC_OPCUA_CU_SUBSCRIPTION_BASIC` (off for nano):

- **5208** Monitor Value Change V2 — MonitoredItem IndexRange parsed at create and
  applied to array samples; the read-path range helpers were un-gated from
  `MUC_OPCUA_CU_MULTI_CHUNK` to also compile under subscriptions, sharing one slicer.
- **3911/4055** ServerCapabilities subscription-limit nodes (`24096`/`24097`/`24098`/
  `24104`/`31916` + `AggregateFunctions` `2997`) added to both `base_nodes.c` tables.
- **3922** SemanticsChanged StatusCode bit via `mu_server_signal_semantic_change`.

`scripts/measure_size.sh` (ARM Cortex-M0+, `-Os`), archive `.text` / elf_text:

| Profile | before | after | Δ elf |
|----------|------------------:|------------------:|------:|
| nano  | 23,660 / 21,524 | 23,660 / 21,524 | 0 (gated out) |
| micro | 41,644 / 40,336 | 43,656 / 42,212 | +1,876 |

The micro delta is dominated by the now-compiled IndexRange helpers (the
`variant_elem_size` type switch + `parse_numeric_range`) plus the base-node table
additions and the 3922 emit/clear path.

**RAM**: per-MonitoredItem +8 B (two `opcua_int32_t` parsed IndexRange, `-1` = none)
+ 1 B (`semantics_changed` latch, absorbed into existing padding on most layouts).
The `MU_SERVER_STORAGE_BYTES` estimate was bumped to match: base `3200 → 3328`
(BASIC-only servers' fixed array) and the Standard per-item term `368 → 384`. No-heap
invariant preserved (all storage is caller-owned). nano RAM unchanged.

## 2026-07-16: Spec 077 — Auditing 074 follow-ups (OldValue, failure-path, SessionId/SecureChannelId)

Full-profile-only (auditing is full-only). Deepens the existing AuditEvents:
pre-write OldValue capture on writes, failure-path emission (Status=false) on
Write/ActivateSession/CreateSession, SecureChannelId on the channel event, and
SessionId on the activate event. `scripts/measure_size.sh full` (ARM Cortex-M0+):

| Profile | before | after | Δ elf |
|----------|------------------:|------------------:|------:|
| full  | 85,754 / 91,144 | 86,046 / 91,420 | +276 |

nano/micro/embedded/standard: 0 (auditing compiled out). No RAM change (the audit
payload pool and event struct are unchanged; the SecureChannelId string is formatted
into a transient stack buffer copied into the existing pool slot).

## 2026-07-17: Spec 080b — Base Types + Core Types Folders (CU 3188/3185)

Flash-only change: ~21 static `const mu_node_t` rows + ~19 pooled BrowseName strings + several
`mu_reference_t` arrays added to the **type-system** address-space table (Table A, gated on
`MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER`). Growth lands on embedded/standard/full only;
**nano/micro unchanged** (their minimal Table B has no type system). No `struct mu_server` field
changes, so RAM and `MU_SERVER_STORAGE_BYTES` are unaffected and the 32-bit ARM static assert is
not at risk.

`scripts/measure_size.sh all` (Arm Cortex-M0+, `-Os`), before at `main` commit `153e88e`
(archive `.text` / elf_text):

| Profile | before | after | Δ .text |
|----------|------------------:|------------------:|------:|
| nano | 23,660 / 21,524 | 23,660 / 21,524 | 0 |
| micro | 43,682 / 42,240 | 43,682 / 42,240 | 0 |
| embedded | 58,295 / 64,488 | 61,420 / 67,616 | +3,125 |
| standard | 59,234 / 67,572 | 62,359 / 70,700 | +3,125 |
| full | 87,512 / 92,888 | 90,450 / 95,828 | +2,938 |

- `.bss`/`.data` unchanged on every profile (0 on nano/micro/embedded; standard/full keep their
  existing 4 B `.bss` / 1,336 B `.data`) — the addition is pure `const` flash.
- embedded == standard delta (both +3,125); `full` is a touch smaller (+2,938) because some of the
  new BrowseName strings/refs already exist there via other full-only features and LTO folds them.
- The primitive re-parenting is net-neutral ref restructuring; `--gc-sections` drops the
  unreferenced pooled strings when a profile disables the CU.

## 2026-07-17: Spec 082 — Response diagnostics: remove file-static (0-B-.bss fix)

Moved the request `returnDiagnostics` echo from a file-static `g_return_diagnostics`
in `response_encode.c` onto `struct mu_server` (`server->return_diagnostics`), threaded
through the existing `server` parameter of `write_response_prefix`/`mu_write_service_fault`
(guard widened from `MUC_OPCUA_CU_TIME_SYNC` to `MU_RESPONSE_PREFIX_WANTS_SERVER` =
`TIME_SYNC || BASE_SERVICES_DIAGNOSTICS`). Restores the zero-mutable-static / 0-B-`.bss`
constitution invariant that the base-types size audit surfaced.

`scripts/measure_size.sh all` (Arm Cortex-M0+, `-Os`), before at `main` commit `4d1e6bd`
(archive `.text` / `.bss`):

| Profile | .text before/after | .bss before → after |
|----------|------------------:|:------:|
| nano | 23,660 / 23,660 | 0 → 0 |
| micro | 43,682 / 43,678 | **4 → 0** |
| embedded | 61,420 / 61,416 | **4 → 0** |
| standard | 62,359 / 62,355 | **4 → 0** |
| full | 90,450 / 90,446 | **4 → 0** |

- `.bss` returns to **0 B on every profile**; the removed global + its two accessor
  functions also trim 4 B of `.text` on the DIAGNOSTICS profiles.
- RAM: `+4 B` in `struct mu_server` on the DIAGNOSTICS profiles (caller-owned storage,
  not static); `MU_SERVER_STORAGE_BYTES` already had slack — the 32-bit ARM
  `_Static_assert` passes with no bump. nano/micro unaffected in RAM terms beyond the
  gated field (nano has no DIAGNOSTICS).
