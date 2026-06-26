# Embedded-Readiness Review

A whole-project review of micro-opcua against embedded-systems best practices
(resource-constrained MCU target: RP2040/Pico, Cortex-M0+, no FPU, small stack,
no heap). Conducted 2026-06-26 across four dimensions: memory/stack, code
size/modularity, concurrency/I-O/robustness, and portability/MCU-correctness.

**Overall:** the core is in genuinely good shape for an MCU — no heap, zero
mutable global state (`.data`/`.bss` empty in every core object, verified with
`nm`), endian-explicit byte-shift wire codec, fixed-width types, no unaligned
casts, `-Wconversion -Wcast-align -Werror` enforced, and hosted libc/OS deps
cleanly quarantined into host-only adapters. The findings below are about
hardening and footprint, not a broken design. The single most urgent item is a
stack-depth regression introduced by the (optional) security layer.

Severity is relative to the stated MCU target.

---

## HIGH

### H1 — Secured request path peaks at ~25–26 KB of stack — ADDRESSED (in-place sym decrypt)
**Status:** Fixed for the hot MSG path. `mu_sym_chunk_unwrap` now decrypts in place
(frame 8.3 KB → 176 B) and `server.c` no longer uses an 8 KB request scratch
(`handle_data_chunk_secure` 16.4 KB → 6.5 KB). Peak secured-path stack dropped
from ~25 KB to ~12 KB (during dispatch: `respbody[5120]` + the 32-deep dispatch
arrays). Remaining opportunity (follow-up): eliminate `respbody` by framing the
response in place in the send buffer, and apply the same in-place treatment to the
asymmetric OPN buffers. Original finding for reference:


`src/core/server.c:191-192` (`reqscratch[8192]` + `respbody[8192]`),
`src/security/sym_chunk.c` (`buf[MU_SYM_SCRATCH=8192]`),
`src/security/asym_chunk.c` (`plain[2048]` + `sign_buf`/`verify_buf[4096]`),
plus dispatch arrays in `src/core/service_dispatch.c` (`nodes[32]`/`results[32]`,
`ref_pool[32]`).

These nest live on one call chain (`server_poll → process_message →
handle_data_chunk_secure → mu_service_dispatch`/`mu_sym_chunk_unwrap`), summing to
~25 KB before the crypto adapter's own usage. The RP2040 Pico SDK default stack
is 2 KB/core; even a bumped 8 KB blows ~3×. The **plaintext path is fine (~5.5 KB)**
because it writes dispatch output directly into `send_buffer` with no large
locals — so enabling `MICRO_OPCUA_SECURITY` raises peak stack ~5×. Nothing
reserves, documents, or guards this.

Fix (in order of impact):
1. Decrypt **in place**: `mu_sym_chunk_unwrap`'s `buf[8192]` exists only to make
   `[header|plaintext]` contiguous for the HMAC, but the receive buffer already
   holds header-then-ciphertext contiguously; AES-CBC can decrypt in place and
   HMAC can run over it. Removes 8 KB at the deepest point.
2. Drop `reqscratch[8192]` similarly — unwrap into the receive buffer and point
   the reader at it (mirror the plaintext path).
3. Do the same for the asym `sign_buf`/`verify_buf` copies.
4. Move the None-MSG delegation (`server.c:210-213`) above the big locals (or into
   `process_message`) so a None MSG doesn't reserve 16 KB it never uses.
5. Until then: size the scratch from `config.max_message_size`, document a minimum
   stack requirement, and add a `static_assert` budget note.

### H2 — Channel/session lifetime is stored but never enforced (stuck-slot DoS) — ADDRESSED
**Status:** Fixed. `mu_server_poll` now stamps `last_activity_ms` (from
`get_tick_ms`) on accept and on each inbound read, and drops the connection when
idle longer than the channel lifetime (or a 30 s connect timeout before a channel
is open), reclaiming the single slot. A monotonic tick of 0 (stub adapter)
disables the timeout. Covered by `tests/integration/test_connection_robustness.c`.
Original finding:


`src/services/secure_channel.c:48-52`, `src/services/session.c:33-37`,
`src/core/server.c:317-345`. `revised_lifetime`/`revised_session_timeout` are
computed and stored but never read back; `created_at` is only ever set to `0`;
`time_adapter.get_tick_ms` is *required* by config validation yet **never called
anywhere** (verified). With a single client/channel/session slot, a peer that
finishes HELLO/OPN then goes silent (or a half-open TCP peer) holds the only slot
forever — a trivial, permanent single-packet DoS.

Fix: stamp a `last_activity` tick (from `get_tick_ms`) on accept and on each
processed chunk; in `mu_server_poll`, close and reclaim the slot when
`now - last_activity` exceeds the channel lifetime (and a shorter connect-phase
timeout for a pre-OPN connection). Enforce the session timeout likewise.

### H3 — Non-blocking `write()` return and `bytes_written` are ignored — ADDRESSED
**Status:** Fixed. `send_buffer_chunk` (and the HELLO ACK path) now check the
write status and `bytes_written`; on an error or short write the connection is
closed so a half-framed chunk never wedges the slot. Covered by
`test_connection_robustness.c`. Original finding:


`src/core/server.c:113-116` (`send_buffer_chunk`), `:275-277` (HELLO ACK),
`:340-341` (server-full ERR). The TCP adapter is explicitly non-blocking, so
`write()` may legitimately send fewer than `total` bytes; the code discards both
the status and `bytes_written`, silently truncating a framed UASC chunk on the
wire. The client then desyncs on a half-chunk — and with H2 there is no timeout to
recover the wedged slot.

Fix: check the status, and on a short write either retain the unsent tail behind a
send-cursor drained on later polls, or close the connection (reclaiming the slot).

### H4 — No size-oriented build flags (`-Os`, function/data sections, `--gc-sections`) — ADDRESSED
**Status:** Fixed. `cmake/MicroOpcUaCodegen.cmake` compiles the library with
`-ffunction-sections -fdata-sections` and exports `-Wl,--gc-sections` as an
INTERFACE link option, so every executable linking `micro_opcua` strips
unreferenced functions/data at link time (verified: dead `mu_is_unsupported_service`
is absent from the linked example). An opt-in `MICRO_OPCUA_OPTIMIZE_SIZE` adds
`-Os`. Original finding:


`CMakeLists.txt`, `cmake/MicroOpcUaWarnings.cmake`, `platform/arduino/platformio.ini`.
No `-Os`/`-Oz`, no `-ffunction-sections -fdata-sections`, no `-Wl,--gc-sections`,
no default `CMAKE_BUILD_TYPE` (a bare build is `-O0`). Without section GC the
linker keeps whole translation units even if one symbol is referenced, so the
"compile in only what you use" goal isn't realized at link time. (The Pico SDK
enables sections+GC itself, so that path partly escapes; host, the size-report
builds, and the Arduino `platformio.ini` do not.)

Fix: add `-ffunction-sections -fdata-sections` to core compile options and
propagate `-Wl,--gc-sections` to executables; default to a size build type when
unset; optionally an `MICRO_OPCUA_LTO` option. This is the biggest single
footprint lever and unlocks stripping for the items under MED/LOW.

---

## MED

### M1 — Sequence-number validation is implemented but never called — ADDRESSED
**Status:** Fixed. Every inbound OPN/MSG chunk's SequenceNumber is now fed to `mu_sequence_validate` (channel-wide, starting at the OPN); a gap/replay aborts the connection. The validator is reset per connection (not on channel open, which previously skipped the first MSG's check). Covered by `test_server_rejects_sequence_gap`.

_Original:_ ### M1 — Sequence-number validation is implemented but never called (no replay protection)
`src/core/sequence.c` provides `mu_sequence_validate` (gap + wrap handling) and
`mu_secure_channel_t` carries a `sequence` validator that is *initialized* — but
no caller ever invokes it (verified). The inbound SequenceNumber is parsed and
discarded (`server.c`, and the secured path keeps only `request_id`). Result: no
replay or gap/abort detection despite the machinery existing.
Fix: feed the decoded sequence number from each unwrap into
`mu_sequence_validate(&channel.sequence, ...)` and abort the channel on non-GOOD.

### M2 — Services are all-or-nothing — ADDRESSED
**Status:** Fixed. New `MICRO_OPCUA_SERVICE_READ`, `MICRO_OPCUA_SERVICE_BROWSE`, and `MICRO_OPCUA_SERVICE_DISCOVERY` options (default ON) gate the service source files and the matching dispatch-table rows + switch cases, so unselected services compile out entirely (verified: a minimal None-only build without Read/Browse/Discovery is ~22 KB smaller and excludes read.c/browse.c). The OpenSecureChannel/Session services remain always-present; discovery.c's endpoint encoder stays (CreateSession needs it). Original finding:


`src/CMakeLists.txt:11-16`, `src/core/service_dispatch.c` (`g_supported_services[]`
+ dispatch `switch`). Only `MICRO_OPCUA_SECURITY` gates anything; Browse, Read,
Discovery (GetEndpoints/FindServers), and Session are always compiled and the
dispatch table/switch pins them all in even under `--gc-sections`.
Fix: per-service `option(MICRO_OPCUA_SERVICE_BROWSE|READ|DISCOVERY|...)`, gating
both the `target_sources` and the matching rows in `g_supported_services[]` and
the dispatch `switch` so unselected services compile out at the table level.

### M3 — `double`/soft-float in the protocol path — ADDRESSED
**Status:** Fixed for the session path. RequestedSessionTimeout is now read as raw IEEE-754 bits and clamped to [10000, 3600000] ms by integer bit comparison (valid for positive doubles), then echoed via `write_uint64` — no FP compare/convert. Verified: `session.o` has no `__aeabi_d*` symbols on ARM. (ReadRequest maxAge is read with `read_double`, which is a bit-copy, not FP math.) Covered by `test_session` clamp cases. Original finding:


`src/core/service_dispatch.c` (RequestedSessionTimeout), `src/services/session.c:33-37`
(double bounds compares), `src/services/read.c:14` (maxAge). Cortex-M0+ has no FPU,
so these pull in libgcc soft-float (`__aeabi_d*`) for the common connect/read path,
though the logic is pure bounds/compare with no real FP math.
Fix: read the 8 bytes and treat the timeout as integer milliseconds; read-and-ignore
maxAge without `double` arithmetic.

### M4 — No freestanding build is declared or verified — ADDRESSED
**Status:** Fixed. A `freestanding-core` CI job compiles every core source
(everything but the host-only platform adapters, including the security layer)
with `arm-none-eabi-gcc -ffreestanding -Wall -Wextra -Werror`, catching any
hosted-header dependency leaking into the core. Original finding:


CMake never sets `-ffreestanding`/`-nostdlib` and no target/CI compiles the core
without libc; the Pico build links full newlib, and the Arduino skeleton has no
compiled target. The core *is* structurally freestanding (only `<stdint.h>`/
`<stddef.h>`/`<stdbool.h>`/freestanding-safe `<string.h>` in the public surface).
Fix: add a `-ffreestanding -Werror` compile-only sanity target under the
`arm-none-eabi` toolchain and wire it into CI to prevent a hosted-header regression.

### M5 — `mu_server_init` doesn't check caller-storage alignment — ADDRESSED
**Status:** Fixed. `mu_server_init` rejects storage not aligned to `_Alignof(struct mu_server)`; the requirement is documented at the API. Original finding:


`src/core/server.c:87` casts a caller `void *storage` to `mu_server_t *` with no
alignment check; the examples back it with a `uint8_t[]` (declared alignment 1). A
caller using a byte/offset buffer would get a misaligned struct — faulting or
fix-up-slow on Cortex-M0+.
Fix: validate `(uintptr_t)storage % _Alignof(struct mu_server) == 0` in
`mu_server_init`, and/or expose storage as a `union { max_align_t; unsigned char[]; }`;
document the requirement by `MU_SERVER_STORAGE_BYTES`.

### M6 — Inbound SecureChannelId/TokenId never validated — ADDRESSED
**Status:** Fixed. `accept_inbound_chunk` rejects a MSG whose SecureChannelId/TokenId don't match the open channel (both the secure and None paths), aborting the connection.

_Original:_ ### M6 — Inbound SecureChannelId/TokenId never validated
`src/core/server.c`. Unwrap fills `secure_channel_id`/`token_id` but the server
never compares them to the open channel's. For SignAndEncrypt a wrong key fails the
HMAC, but the None-policy MSG path has no channel binding at all.
Fix: reject chunks whose channel id / token id don't match the open channel.

---

## LOW

- **Dead code always linked (absent `--gc-sections`):** `g_unsupported_services[]`
  + `mu_is_unsupported_service()` (`src/core/status.c:45-62`) are never called;
  `src/generated/opcua_ids.c` is an empty TU. Delete or gate.
- **Enum fields cost 4 bytes** in many-instance structs (`mu_variant_t`,
  `mu_datavalue_t`, `mu_node_t`): the 32-deep dispatch result arrays pay 3 wasted
  bytes per enum field. Consider `uint8_t`-backed fields (verify the codec).
- **`mu_status_name()`** (`status.c`) is a ~30-arm debug-string switch; drops out
  under section GC, sits in flash without it.
- **Mock entropy in core:** `src/services/session.c:11-14` fills the nonce with
  `i` instead of the entropy adapter (CreateSession uses a proper `fill_server_nonce`,
  but this path doesn't). Security weakness; replace with the adapter.
- **RSA key-size underflow guard:** `asym_chunk.c` `plain_block = key_bytes - 42`
  underflows for a <336-bit peer cert (caught incidentally downstream). Add an
  explicit `key_bytes <= 42 → Bad_CertificateInvalid`.
- **Negative HELLO MessageSize** accepted (`tcp_connection.c:39-43`, unused after);
  reject `< 8`.
- **Malformed pre-dispatch header** silently dropped with no ServiceFault/close
  (`server.c` `handle_data_chunk_*`); close instead.
- **One poll can run many full crypto cycles** (no `max_messages_per_poll` cap) —
  a long non-yielding burst on a cooperative loop. Add a per-poll cap.
- **Platform skeletons** (`platform/{pico,arduino}/…adapter.c`) stub time/entropy to
  zero — fine as skeletons, but must be replaced before any secure deployment
  (zero nonces).

---

## Suggested remediation order
1. **H1** (stack) — the security layer regressed peak stack ~5×; in-place crypto is the fix.
2. **H3** + **H2** (write handling + timeout) — together they prevent a permanently wedged single slot.
3. **H4** (build flags) — cheap, large footprint win; unlocks dead-code stripping.
4. **M1** (sequence validation) — wire up the code that already exists.
5. **M2** (per-service gating) and the rest as budget allows.
