# Implementation Plan: Optimization Audit Remediation

**Branch**: `004-optimization-fixes` | **Date**: 2026-06-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/004-optimization-fixes/spec.md`

## Summary

Remediate the 22 findings of `docs/review/optimization-audit-2026-06-27.md` across
four prioritized, independently-shippable slices: (US1) crash-safety & parser
correctness on the untrusted/secure path, (US2) secret zeroization, (US3) hot-path
performance, and (US4) flash/RAM footprint. The technical approach is
behaviour-preserving by default: the only wire-observable changes are corrections
*toward* the OPC UA specification (ActivateSession field consumption per
OPC-10000-4 §5.7.3.2, OpenSecureChannel policy/mode handling per §5.6.2.2, NodeId
type guards). Everything else (stack relocation, per-channel cipher context,
address-space index, single-pass Browse, sticky-error encoding, dispatch unify,
dedup, LTO) keeps responses byte-for-byte identical and the no-heap model intact.

## Technical Context

**Language/Version**: Freestanding C11 (C99-compatible subset where practical)
**Primary Dependencies**: None in core; optional pluggable crypto adapter (host: OpenSSL; MCU: mbedTLS/PSA/wolfSSL-class)
**Storage**: N/A (no persistence in hot path; caller-provided buffers)
**Testing**: Unity unit tests, integration tests (asyncua + OPC Foundation .NET reference client), libFuzzer decoders, ASan/UBSan
**Target Platform**: Host (Linux, x86-64) first; RP2040/Pico SDK (Cortex-M0+) and Arduino second
**Project Type**: Portable C library with thin platform adapters + examples
**Performance Goals**: Sub-linear NodeId resolution; zero per-tick node lookup in sampling; one AES key schedule per channel (not per MSG); byte-identical responses
**Constraints**: No heap on hot path; worst-case secure-path stack ≤ 10 KiB (from ~12.8 KiB); net flash reduction ≥ 8% on Micro core; warnings-as-errors clean for all profile permutations
**Scale/Scope**: Single TCP connection, ≥2 logical sessions multiplexed; address space integrator-sized (tens→hundreds of nodes); 22 findings across ~12 source files
**OPC UA Normative References**: OPC-10000-4 §5.6.2.2 (OpenSecureChannel params), §5.7.3.2 (ActivateSession params), §5.9.2 (Browse); OPC-10000-6 §5.2 (Binary encoding rules), §6.7.2 (MessageChunk); OPC-10000-7 (Nano/Micro profile, unchanged)
**Target OPC UA Profile/Conformance Units**: Unchanged — existing Nano/Micro Embedded Device Server profile targeting
**Conformance Status Target**: Unchanged (profile-targeting; Basic256Sha256 gated behind the security build option)

## Embedded Size Budget

**Flash Impact**: **Net negative (goal ≥ −8% Micro core).** Reductions: sticky-error encoder/decoder (~1.5–2.5 KiB), unify dual dispatch (~0.2–0.4 KiB), per-id handler dedup (~90–110 lines), LE pack/unpack consolidation, `element_size` table, gate `mu_status_name` + delete dead unsupported-service table (~1–1.5 KiB), opt-in LTO (several %). Additions (small): index-build code, `mu_secure_zero`, cipher-context management.
**RAM Impact**: **Peak stack reduced ~12.8 KiB → ≤10 KiB** (relocate OPN scratch off the call chain). Static additions (bounded): address-space index `≤ MU_MAX_ADDRESS_SPACE_NODES × 2 B` (e.g. 64 → 128 B), per-channel cipher context (~0.3–0.5 KiB if caching AES key schedules), relocated secure scratch moved from stack into the single `struct mu_server` (~6 KiB now `.bss` instead of stack — a deliberate stack↔static trade for a single-connection server; see Complexity Tracking).
**Heap Use**: **None.** No new dynamic allocation anywhere (FR-019).
**Static Tables Added**: address-space node index (indices sorted by NodeId key); optional `element_size` lookup table (~20 B rodata).
**Transport Buffers**: Unchanged (caller-provided RX/TX, `MU_MIN_CHUNK_SIZE` floor per OPC-10000-6); secure scratch relocated into server storage but not enlarged.
**Crypto Dependency Impact**: Crypto remains pluggable. Optional cipher-context extension to the adapter is **additive and backward-compatible** — adapters that do not provide it fall back to today's per-call behaviour (no key-schedule caching, correctness unchanged).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. The three behavioural changes are each cited (§5.7.3.2, §5.6.2.2, NodeId/encoding §6); FR-021 makes the spec authoritative on conflict; FR-014/SC-003 require byte-identical responses for all non-behavioural work. Malformed input returns cited StatusCodes (OPC-003).
- **Embedded C Core**: PASS. Freestanding C11; no OS leakage; the cipher-context extension stays behind the existing crypto adapter boundary.
- **Memory Model**: PASS (with documented trade). No heap. New state is static or caller-provided; the stack→static scratch relocation is logged in Complexity Tracking.
- **Minimal OPC UA Surface**: PASS. No new services, encodings, transports, or SecurityPolicies.
- **Profile Research**: PASS. Profile/conformance claim is unchanged; this is remediation.
- **Correctness Gates**: PASS. Test-first for FR-002/004/005 and the stack budget (FR-001); decoder fuzz extended; byte-identical regression vectors for perf/size changes.
- **Security Honesty**: PASS. Crypto stays pluggable (additive, graceful-fallback extension); zeroization (FR-007/008) strengthens; SecurityPolicy None handling clarified per §5.6.2.2, not weakened.
- **Toolchain Discipline**: PASS. CMake; host tests + sanitizers; clang-format/warnings-as-errors; ≥1 embedded cross-compile (Pico); opt-in LTO.
- **Size Discipline**: PASS. Flash/RAM/stack/static-table/crypto impacts stated above; net flash down; headroom up (lower stack). No removal of status handling, bounds checks, traceability, or tests.

**Result: PASS — no unjustified violations. Proceed to Phase 0.**

## Project Structure

### Documentation (this feature)

```text
specs/004-optimization-fixes/
├── plan.md              # This file
├── research.md          # Phase 0 — design decisions per finding cluster
├── data-model.md        # Phase 1 — new/changed internal entities
├── quickstart.md        # Phase 1 — how to build/verify each slice
├── contracts/           # Phase 1 — changed interface contracts
│   ├── crypto-adapter-cipher-context.md
│   ├── address-space-index.md
│   ├── binary-codec-sticky-status.md
│   └── secure-zero.md
└── tasks.md             # Phase 2 (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
include/micro_opcua/
├── platform.h           # US3: optional cipher-context fields on mu_crypto_adapter_t (additive)
├── address_space.h      # US3: index type + find-node uses index
├── encoding.h           # US4: sticky status on reader/writer; overflow-safe bound helper
└── config.h             # US3/US4: MU_MAX_ADDRESS_SPACE_NODES, MU_CIPHER_CTX_SIZE, LTO knob doc
src/
├── core/
│   ├── server.c          # US1: relocate OPN scratch into struct mu_server; US3: memmove cursor
│   ├── server_internal.h # US1: add secure_scratch storage
│   └── service_dispatch.c# US1: ActivateSession consume cert array/token; NodeId guards; sub op caps. US4: unify dispatch table+switch; dedup per-id handlers; sticky-status checks
├── encoding/
│   ├── binary_reader.c / binary_writer.c  # US4: sticky status; overflow-safe bounds; LE helpers
│   ├── binary_variant.c  # US4: element_size table
│   └── binary_*.c        # US4: shared LE pack/unpack
├── services/
│   ├── secure_channel.c/.h # US2: zeroize keys on close/reuse; US3: per-channel cipher ctx storage
│   ├── browse.c          # US3: single-pass reference resolution
│   ├── read.c            # US3: benefits from address-space index (no change to semantics)
│   └── subscription.c    # US3: cache resolved node in MonitoredItem; divide fast-path; op caps
├── security/
│   ├── sym_chunk.c       # US2 zeroize; US3 use per-channel cipher ctx
│   ├── asym_chunk.c      # US1 right-size scratch; US2 zeroize; US3 cache thumbprint/peer cert
│   └── key_derivation.c  # US2 zeroize intermediates
├── address_space/
│   └── node_id.c         # US3: build + use sorted NodeId index (bounded, no heap)
└── status.c              # US4: gate mu_status_name; delete dead unsupported-service table
cmake/
└── MicroOpcUaCodegen.cmake # US4: opt-in MICRO_OPCUA_LTO via INTERPROCEDURAL_OPTIMIZATION
tests/
├── unit/                # new: ActivateSession edge cases, NodeId guards, OPN policy, zeroize, index, byte-identical regression
├── fuzz/                # extend decoder corpus for cert-array/token paths
└── integration/         # reference-client/asyncua regression backstop
docs/
├── size/feature-size-ledger.md      # US4: refreshed measured figures
├── traceability/                    # map new behaviour to OPC sections
└── review/optimization-audit-2026-06-27.md  # source of findings
```

**Structure Decision**: Existing repository layout is retained (Constitution: portable C core + adapters + examples + tests + docs). No new top-level directories. Changes are localized edits to the files above, grouped by user story so each slice is independently buildable and testable.

## Complexity Tracking

| Violation / Deviation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Stack→static relocation of ~6 KiB secure scratch into `struct mu_server` (`.bss`) | FR-001/SC-001: ~12.8 KiB worst-case stack risks overflow on a Cortex-M0+ with no guard page (4 reviewers). A single-connection server must already provision worst-case stack, so trading peak stack for fixed static is a net headroom win. | Keeping buffers on the stack keeps `.bss` smaller but leaves the overflow risk; encrypt-in-`send_buffer` (no scratch at all) was considered and is the stretch goal, but the safe first step is relocation. |
| Optional cipher-context extension to `mu_crypto_adapter_t` | FR-012/T2: stateless adapter re-runs the AES-256 key schedule every MSG chunk — dominant per-message cost on a no-FPU M0+. | Leaving the adapter stateless keeps the interface tiny but cannot cache the schedule; the extension is additive with graceful fallback, so existing adapters keep working unchanged. |
| Bounded address-space index (`≤ MU_MAX_ADDRESS_SPACE_NODES`) built at init | FR-009/T3: `mu_address_space_find_node` is O(N) under every Read/Browse/sample (3 reviewers); N is integrator-controlled/unbounded. | Assuming caller-pre-sorted nodes was rejected (caller owns order); pure linear scan was rejected as the audited bottleneck. Index falls back to linear scan if node_count exceeds the cap, preserving correctness. |
