# Implementation Plan: Nano Client — Discovery + Read

**Branch**: `047-nano-client` | **Date**: 2026-07-07 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/047-nano-client/spec.md`

## Summary

Implement the Nano Embedded UA Client Profile: a minimal OPC UA client that can
discover endpoints, establish a SecureChannel and session, read variable values,
and browse the address space. The client reuses the server's existing encoding,
security, and transport layer, bundled in the same library with isolated client
state. Zero-heap protocol hot path, caller-provided buffers, CMake profile gating.

## Technical Context

**Language/Version**: C11 (freestanding, C99-compatible subset)
**Primary Dependencies**: project-internal API headers (shared encoding, security,
  transport). No external client libraries.
**Storage**: N/A (caller-provided buffers for all protocol I/O)
**Testing**: ctest via Unity, `cmake --build . && ctest`
**Target Platform**: Host (Linux) for tests; embedded builds for Pico SDK
**Project Type**: C library (client feature in existing server library)
**Performance Goals**: All tests complete in <10 seconds. Client connection +
  read round-trip in <500ms on host.
**Constraints**: Zero heap on protocol path. All buffers caller-provided.
  Compile-time session/connection limits via `MU_CLIENT_MAX_SESSIONS`.
**Scale/Scope**: 1 session, 1 secure channel per client instance (Nano profile).
**OPC UA Normative References**: OPC-10000-4 §5.4.2 (GetEndpoints), §5.5.2
  (OpenSecureChannel), §5.5.3 (CloseSecureChannel), §5.6.2 (CreateSession),
  §5.6.3 (ActivateSession), §5.6.4 (CloseSession), §5.10.2 (Read),
  §5.8.2 (Browse), §5.8.3 (BrowseNext), §5.8.4 (TranslateBrowsePathsToNodeIds).
  OPC-10000-6 §5 (Binary Encoding), §6 (OPC UA TCP), §7.1.2.3 (Hello), §7.1.2.4 (Acknowledge).
**Target OPC UA Profile/Conformance Units**: Nano Embedded UA Client Profile
  (OPC-10000-7). Conformance units: Discovery Client, Base Client,
  Attribute Client, View Client.
**Conformance Status Target**: experimental (host tests + interop, no CTT)

## Embedded Size Budget

**Flash Impact**: ~6-10 KB standalone client; ~8-12 KB when linked with server
  (shared encoding/security layer)
**RAM Impact**: Client struct ~512 B (config, SecureChannel state, session
  handle, send/receive buffers). Buffers caller-provided.
**Heap Use**: 0 — all buffers are caller-provided or statically embedded in
  `mu_client_t`.
**Static Tables Added**: Service request/response type ID tables (~100 bytes ROM
  if duplicated from server; ideally shared)
**Transport Buffers**: Send buffer 8 KB, receive buffer 8 KB (caller-configured).
  OPC UA TCP frame overhead per OPC-10000-6 §6.7.2.
**Crypto Dependency Impact**: Same pluggable backend as server. Symmetric
  signing/encryption adds ~2-4 KB when Basic256Sha256 is enabled.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. Every service cites exact OPC-10000-4 sections.
  Unsupported services return Bad_NotImplemented.
- **Embedded C Core**: PASS. Freestanding C11, caller-provided buffers, no OS
  dependency in client core. Platform adapter for TCP I/O.
- **Memory Model**: PASS. Protocol hot path uses caller-provided buffers.
  `mu_client_t` is caller-allocated. No heap.
- **Minimal OPC UA Surface**: PASS. Nano profile is the smallest client profile.
  Only Discovery, SecureChannel, Session, Read, Browse — no subscriptions,
  methods, events, PubSub.
- **Profile Research**: PASS. Nano Embedded UA Client Profile from OPC-10000-7.
  Conformance units identified and cited.
- **Correctness Gates**: PASS. Round-trip interop tests against existing
  muc-opcua server. Negative tests for malformed responses.
- **Security Honesty**: PASS. Basic256Sha256 supported using existing server
  crypto stubs. SecurityPolicy None for interop phase only.
- **Toolchain Discipline**: PASS. CMake, Unity tests, clang-format, cppcheck,
  embedded cross-compile for Pico SDK.
- **Size Discipline**: PASS. Flash/RAM estimates documented. Zero heap.

## Project Structure

### Documentation (this feature)

```text
specs/047-nano-client/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (public API contracts)
│   ├── sequences.md     # Client-server interaction sequences
│   └── api.md           # Client API contract
├── checklists/
│   └── requirements.md  # Spec quality checklist
└── tasks.md             # Phase 2 output (NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
include/
├── muc_opcua/
│   ├── client.h                    # NEW: client API typedefs and functions
│   ├── types.h                     # Existing: NodeId, Variant, StatusCode
│   ├── encoding.h                  # Existing: binary encoder/decoder
│   └── security.h                  # Existing: security policy stubs
src/
├── client/                         # NEW: client implementation
│   ├── client.c                    # mu_client_t lifecycle (init, connect, disconnect)
│   ├── client_secure_channel.c     # Open/Close SecureChannel
│   ├── client_session.c            # Create/Activate/Close Session
│   ├── client_read.c              # Read service
│   ├── client_browse.c            # Browse / BrowseNext / TranslateBrowsePaths
│   └── client_state.c             # Client state machine
├── core/
│   └── service_dispatch/          # Existing: server dispatch (unchanged)
├── encoding/                       # Existing: shared encoder/decoder
├── security/                       # Existing: shared crypto stubs
├── platform/                       # Existing: adapter interfaces (+ client TCP)
└── CMakeLists.txt                  # Add MUC_OPCUA_CLIENT_PROFILE gate
tests/
├── unit/
│   └── test_nano_client.c         # NEW: unit + interop tests
└── integration/
    └── test_nano_client_interop.c # NEW: server/client interop test
```

**Structure Decision**: Client code lives in `src/client/` — parallel to existing
`src/services/` (server). Shared encoding, security, and types remain in their
existing directories. No code duplication between client and server.

## Design Artifacts

- Internal object design: `./class-diagram.md`
- Service sequences: `./contracts/sequences.md`
- Data model: `./data-model.md`
- Interface contracts: `./contracts/api.md`
- Validation path: `./quickstart.md`

## Complexity Tracking

> No constitution violations. All features follow existing patterns
> (caller-provided buffers, CMake profile gating, service dispatch).
